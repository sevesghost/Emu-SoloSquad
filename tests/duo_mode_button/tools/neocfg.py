#!/usr/bin/env python3
"""NEO-Duo binary `config` encoder/decoder.

Byte layout is taken from EmuFramework on sevesghost/NEO-Duo master 9350574:
  EmuFramework/src/configFile.ccm
  EmuFramework/src/vcontrols/VController.cc  (readVControllerElement,
      writeToConfig, configDataSizeBytes, writeUIButtonsConfig,
      writeDeviceButtonsConfig, writeDuoButtonsConfig)
  EmuFramework/include/emuframework/VController.hh
  EmuFramework/include/emuframework/AppKeyCode.hh
  EmuFramework/include/emuframework/inputDefs.hh
  EmuFramework/include/emuframework/Option.hh
  imagine/include/imagine/util/2DOrigin.h

The file is little-endian (native write of FileIO::put on Android/ARM and
x86_64). All integers below are little-endian.

File:
  uint8  header = 2          # block size field width in bytes
  repeated blocks:
    uint16 size              # includes the 2-byte key
    uint16 key
    uint8  payload[size-2]

VController keys:
  113  CFGKEY_VCONTROLLER_UI_BUTTONS_V2
  112  CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V2
  125  CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3

See README.md for usage, sample files, and format uncertainties.
"""

from __future__ import annotations

import argparse
import io
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional


# ---------------------------------------------------------------------------
# Constants from the C++ sources
# ---------------------------------------------------------------------------

HEADER_BLOCK_SIZE = 2  # writeConfigHeader: uint8_t blockHeaderSize = 2

CFGKEY_VCONTROLLER_LAYOUT_POS = 68
CFGKEY_VCONTROLLER_DPAD = 99
CFGKEY_VCONTROLLER_DEVICE_BUTTONS = 100
CFGKEY_VCONTROLLER_UI_BUTTONS = 101
CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V2 = 112
CFGKEY_VCONTROLLER_UI_BUTTONS_V2 = 113
CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3 = 125

CFGKEY_NAMES = {
    15: "CFGKEY_TOUCH_CONTROL_FACE_BTN_SPACE",
    16: "CFGKEY_TOUCH_CONTROL_FACE_BTN_STAGGER",
    21: "CFGKEY_TOUCH_CONTROL_DPAD_DEADZONE",
    22: "CFGKEY_TOUCH_CONTROL_CENTER_BTN_POS",
    23: "CFGKEY_TOUCH_CONTROL_TRIGGER_BTN_POS",
    24: "CFGKEY_TOUCH_CONTROL_MENU_POS",
    25: "CFGKEY_TOUCH_CONTROL_FF_POS",
    30: "CFGKEY_TOUCH_CONTROL_IMG_PIXELS",
    37: "CFGKEY_TOUCH_CONTROL_DIAGONAL_SENSITIVITY",
    38: "CFGKEY_TOUCH_CONTROL_EXTRA_X_BTN_SIZE",
    39: "CFGKEY_TOUCH_CONTROL_EXTRA_Y_BTN_SIZE",
    40: "CFGKEY_TOUCH_CONTROL_EXTRA_Y_BTN_SIZE_MULTI_ROW",
    59: "CFGKEY_TOUCH_CONTROL_BOUNDING_BOXES",
    65: "CFGKEY_TOUCH_CONTROL_SHOW_ON_TOUCH",
    68: "CFGKEY_VCONTROLLER_LAYOUT_POS",
    93: "CFGKEY_VCONTROLLER_ALLOW_PAST_CONTENT_BOUNDS",
    99: "CFGKEY_VCONTROLLER_DPAD",
    100: "CFGKEY_VCONTROLLER_DEVICE_BUTTONS",
    101: "CFGKEY_VCONTROLLER_UI_BUTTONS",
    112: "CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V2",
    113: "CFGKEY_VCONTROLLER_UI_BUTTONS_V2",
    115: "CFGKEY_VCONTROLLER_HIGHLIGHT_PUSHED_BUTTONS",
    125: "CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3",
}

# AppKeyCode enum class : KeyCode, starting at openMenu = 1, declaration order.
APP_KEY_NAMES = [
    "openMenu",  # 1
    "openContent",
    "openSystemActions",
    "saveState",
    "loadState",
    "decStateSlot",
    "incStateSlot",
    "fastForward",
    "toggleFastForward",
    "slowMotion",
    "toggleSlowMotion",
    "takeScreenshot",
    "turboModifier",
    "exitApp",
    "rewind",
    "softReset",
    "hardReset",
    "resetMenu",
    "closeContent",
    "toggleDuoLayout",  # 20
]
APP_KEY_BY_NAME = {n: i for i, n in enumerate(APP_KEY_NAMES, start=1)}
TOGGLE_DUO_LAYOUT = APP_KEY_BY_NAME["toggleDuoLayout"]
OPEN_MENU = APP_KEY_BY_NAME["openMenu"]
TOGGLE_FF = APP_KEY_BY_NAME["toggleFastForward"]
REWIND = APP_KEY_BY_NAME["rewind"]

# enum class VControllerState : uint8_t
STATE_NAMES = {0: "OFF", 1: "SHOWN", 2: "HIDDEN"}

# Origin enum class : uint8_t (2DOrigin.h)
ORIGIN_NAMES = {
    0: "center",
    1: "min",
    2: "max",
    3: "centerInverted",
    4: "minInverted",
    5: "maxInverted",
}

# cartesian shortcuts (x, y)
ORIGIN_SHORT = {
    "CT": (0, 2),  # CT2DO
    "RT": (2, 2),  # RT2DO
    "RC": (2, 0),
    "RB": (2, 1),
    "CB": (0, 1),
    "LB": (1, 1),
    "LC": (1, 0),
    "LT": (1, 2),
    "C": (0, 0),
}
ORIGIN_SHORT_FROM_XY = {v: k for k, v in ORIGIN_SHORT.items()}

# Default values from VController.hh
DEFAULT_P1_SIZE = 1000
DEFAULT_P2_SIZE = 1000
DEFAULT_SHARED_SIZE = 1000
DEFAULT_P1_ALPHA = int(255 * 0.5)  # 127
DEFAULT_P2_ALPHA = int(255 * 0.5)  # 127
DEFAULT_SHARED_ALPHA = int(255 * 0.4)  # 102
DEFAULT_BUTTON_SPACING_MM = 2
DEFAULT_DPAD_DEADZONE = 135
DEFAULT_DPAD_DIAGONAL = 0.57  # float, defaultDPadDiagonalSensitivity

# Synthetic (non-sentinel) pixel positions for the default Open Menu and
# Fast-forward/Rewind groups. pos.x == -1 on uiElements[0] makes
# VController::configure() call resetUIGroups(), which would bring the
# toggle button back. These values are not taken from a real device.
DEFAULT_MENU_POS = (-16, 24)
DEFAULT_FF_POS = (24, 24)

# What syncDuoModeButton() + setDuoPosition(..., CT2DO, 0.f, 30.f) writes.
# Positions are millimetres*10, but writeUIButtonsConfig currently does NOT
# write duoFlags, so a reader treats these as pixels (the bug under test).
DEFAULT_TOGGLE_POS = (0, 300)

# Custom positions used by --duo-toggle-moved when --pos0/--pos1 are omitted.
MOVED_POS0 = (80, 120)   # landscape, origin CB
MOVED_POS1 = (-40, 250)  # portrait,  origin LT


# ---------------------------------------------------------------------------
# Binary helpers
# ---------------------------------------------------------------------------

class DecodeError(Exception):
    pass


class Reader:
    def __init__(self, data: bytes, offset: int = 0):
        self.data = data
        self.offset = offset

    def remaining(self) -> int:
        return len(self.data) - self.offset

    def eof(self) -> bool:
        return self.offset >= len(self.data)

    def tell(self) -> int:
        return self.offset

    def _need(self, n: int) -> None:
        if self.remaining() < n:
            raise DecodeError(
                f"truncated: need {n} byte(s) at offset {self.offset}, "
                f"only {self.remaining()} left"
            )

    def get(self, fmt: str):
        size = struct.calcsize(fmt)
        self._need(size)
        val = struct.unpack_from(fmt, self.data, self.offset)
        self.offset += size
        return val[0] if len(val) == 1 else val

    def raw(self, n: int) -> bytes:
        self._need(n)
        b = self.data[self.offset : self.offset + n]
        self.offset += n
        return b

    def rest(self) -> bytes:
        b = self.data[self.offset :]
        self.offset = len(self.data)
        return b


class Writer:
    def __init__(self):
        self.buf = bytearray()

    def put(self, fmt: str, *vals) -> None:
        self.buf.extend(struct.pack(fmt, *vals))

    def raw(self, b: bytes) -> None:
        self.buf.extend(b)

    def bytes(self) -> bytes:
        return bytes(self.buf)

    def __len__(self) -> int:
        return len(self.buf)


def hexdump(data: bytes, width: int = 16) -> str:
    if not data:
        return "(empty)"
    parts = []
    for i in range(0, len(data), width):
        chunk = data[i : i + width]
        hexs = " ".join(f"{b:02x}" for b in chunk)
        parts.append(f"  {i:04x}: {hexs}")
    return "\n".join(parts)


def pack_origin(x: int, y: int) -> int:
    return (x & 0xF) | ((y & 0xF) << 4)


def unpack_origin(val: int) -> tuple[int, int]:
    return val & 0xF, (val >> 4) & 0xF


def origin_label(x: int, y: int) -> str:
    short = ORIGIN_SHORT_FROM_XY.get((x, y))
    xl = ORIGIN_NAMES.get(x, str(x))
    yl = ORIGIN_NAMES.get(y, str(y))
    if short:
        return f"{short}2DO ({xl},{yl}) pack=0x{pack_origin(x, y):02x}"
    return f"({xl},{yl}) pack=0x{pack_origin(x, y):02x}"


def parse_origin_token(tok: str) -> tuple[int, int]:
    t = tok.strip().upper().removesuffix("2DO")
    if t in ORIGIN_SHORT:
        return ORIGIN_SHORT[t]
    raise ValueError(f"unknown origin {tok!r}; use CT/RT/RC/RB/CB/LB/LC/LT/C")


def parse_pos_arg(s: str) -> tuple[int, int, tuple[int, int]]:
    # "x,y,ORIGIN"
    parts = [p.strip() for p in s.split(",")]
    if len(parts) != 3:
        raise ValueError(f"expected x,y,ORIGIN, got {s!r}")
    return int(parts[0]), int(parts[1]), parse_origin_token(parts[2])


# KeyInfo is 4 bytes: ZArray<KeyCode,3> (3×uint8) + KeyFlags (uint8 bitfield).
# GCC/Clang (Itanium ABI, used by this project's Android NDK / Linux builds):
# appCode:1, turbo:1, toggle:1, bothPlayers:1, deviceId:4  — low bit first.
def pack_keyinfo(codes: Iterable[int], flags: int) -> bytes:
    c = list(codes) + [0, 0, 0]
    return bytes([c[0] & 0xFF, c[1] & 0xFF, c[2] & 0xFF, flags & 0xFF])


def pack_app_key(code: int) -> bytes:
    return pack_keyinfo([code], flags=0x01)  # appCode bit


def unpack_keyinfo(b: bytes) -> dict:
    if len(b) != 4:
        raise DecodeError(f"KeyInfo must be 4 bytes, got {len(b)}")
    codes = [b[0], b[1], b[2]]
    flags_b = b[3]
    flags = {
        "appCode": flags_b & 1,
        "turbo": (flags_b >> 1) & 1,
        "toggle": (flags_b >> 2) & 1,
        "bothPlayers": (flags_b >> 3) & 1,
        "deviceId": (flags_b >> 4) & 0xF,
        "raw": flags_b,
    }
    return {"codes": codes, "flags": flags}


def format_keyinfo(k: dict) -> str:
    codes = k["codes"]
    flags = k["flags"]
    if flags["appCode"] and codes[0] != 255:
        idx = codes[0]
        name = APP_KEY_NAMES[idx - 1] if 1 <= idx <= len(APP_KEY_NAMES) else f"app:{idx}"
        extra = ""
        if any(codes[1:]):
            extra = f" combocodes={codes}"
        flagbits = []
        if flags["turbo"]:
            flagbits.append("turbo")
        if flags["toggle"]:
            flagbits.append("toggle")
        if flags["bothPlayers"]:
            flagbits.append("bothPlayers")
        if flags["deviceId"]:
            flagbits.append(f"deviceId={flags['deviceId']}")
        fl = f" flags[{','.join(flagbits)}]" if flagbits else ""
        return f"{name} (app {idx}){extra}{fl}"
    if flags["appCode"] and codes[0] == 255:
        return f"comboKey idx={codes[1]} flags=0x{flags['raw']:02x}"
    bits = []
    if flags["turbo"]:
        bits.append("turbo")
    if flags["toggle"]:
        bits.append("toggle")
    if flags["bothPlayers"]:
        bits.append("bothPlayers")
    if flags["deviceId"]:
        bits.append(f"deviceId={flags['deviceId']}")
    fl = f" flags[{','.join(bits)}]" if bits else ""
    useful = [c for c in codes if c]
    return f"sys{useful if useful else codes}{fl}"


def has_app_key(keys: list[dict], code: int) -> bool:
    return any(k["flags"]["appCode"] and k["codes"][0] == code for k in keys)


# ---------------------------------------------------------------------------
# Element / block data classes
# ---------------------------------------------------------------------------

@dataclass
class LayoutPos:
    x: int = -1
    y: int = -1
    origin: tuple[int, int] = (1, 2)  # default LT2DO

    def pack(self) -> bytes:
        return struct.pack("<hhB", self.x, self.y, pack_origin(*self.origin))

    @staticmethod
    def unpack(r: Reader) -> "LayoutPos":
        x, y = r.get("<hh")
        ox, oy = unpack_origin(r.get("<B"))
        return LayoutPos(x, y, (ox, oy))

    def label(self) -> str:
        return f"({self.x}, {self.y}) origin={origin_label(*self.origin)}"


@dataclass
class DuoExtra:
    size_mm100x: int = 0
    own_alpha: int = 0
    duo_flags: int = 0
    duo_role: int = 255

    def pack(self) -> bytes:
        return struct.pack("<hBBB", self.size_mm100x, self.own_alpha, self.duo_flags, self.duo_role)

    @staticmethod
    def unpack(r: Reader) -> "DuoExtra":
        return DuoExtra(*r.get("<hBBB"))

    def label(self) -> str:
        flags = self.duo_flags
        bits = []
        if flags & 1:
            bits.append("DUO_POS_MM")
        if flags & 2:
            bits.append("DUO_FIXED_PLAYER")
        if flags & 4:
            bits.append("DUO_OWN_ALPHA")
        player = (flags >> 3) & 3
        if flags & (3 << 3):
            bits.append(f"playerSet={player}")
        return (
            f"sizeMM100x={self.size_mm100x} ownAlpha={self.own_alpha} "
            f"duoFlags=0x{self.duo_flags:02x}[{','.join(bits) or '0'}] "
            f"duoRole={self.duo_role}"
        )


@dataclass
class Element:
    # type: 0 = button group (or UI group when in a UI block), 1 = dpad
    typ: int
    is_ui: bool
    row_items: int = 1
    origin: tuple[int, int] = (0, 0)
    # device-only layout fields
    spacing_mm: int = DEFAULT_BUTTON_SPACING_MM
    x_padding: int = 0
    y_padding: int = 0
    stagger_type: int = 2
    show_bounding_area: int = 0
    keys: list[dict] = field(default_factory=list)
    raw_keys: list[bytes] = field(default_factory=list)
    # dpad-only
    diagonal_sensitivity: float = DEFAULT_DPAD_DIAGONAL
    deadzone_mm100x: int = DEFAULT_DPAD_DEADZONE
    visualize_bounds: int = 0
    layout_pos: list[LayoutPos] = field(default_factory=lambda: [LayoutPos(), LayoutPos()])
    state: int = 1  # SHOWN
    duo_extra: Optional[DuoExtra] = None

    def type_name(self) -> str:
        if self.typ == 0:
            return "UIButtonGroup" if self.is_ui else "ButtonGroup"
        if self.typ == 1:
            return "DPad"
        return f"unknown({self.typ})"


@dataclass
class VBlock:
    kind: str  # 'ui_v2' | 'device_v2' | 'device_v3'
    config_id: int = 0
    device_id: int = 0
    flags: int = 0
    p1_size: int = DEFAULT_P1_SIZE
    p2_size: int = DEFAULT_P2_SIZE
    p1_alpha: int = DEFAULT_P1_ALPHA
    p2_alpha: int = DEFAULT_P2_ALPHA
    shared_size: int = DEFAULT_SHARED_SIZE
    shared_alpha: int = DEFAULT_SHARED_ALPHA
    lock_player_sizes: int = 0
    has_shared_fields: bool = False
    two_player: bool = False
    elements: list[Element] = field(default_factory=list)
    trailing: bytes = b""
    trailing_note: str = ""
    removed_flag: Optional[int] = None  # PR #2 trailer: uint8 duoModeButtonRemoved_


@dataclass
class ConfigFile:
    header: int = HEADER_BLOCK_SIZE
    blocks: list[tuple[int, int, bytes]] = field(default_factory=list)  # key, size, payload
    parsed: dict[int, VBlock] = field(default_factory=dict)
    unknown: list[tuple[int, int, bytes]] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Element (de)serialization — matches writeToConfig / readVControllerElement
# ---------------------------------------------------------------------------

def write_sized_keys(w: Writer, raw_keys: list[bytes]) -> None:
    # writeSizedData<uint8_t>: uint8 count, then count * sizeof(KeyInfo) bytes
    n = min(len(raw_keys), 255)
    w.put("<B", n)
    for k in raw_keys[:n]:
        assert len(k) == 4
        w.raw(k)


def read_sized_keys(r: Reader) -> tuple[list[dict], list[bytes]]:
    n = r.get("<B")
    raw, parsed = [], []
    for _ in range(n):
        b = r.raw(4)
        raw.append(b)
        parsed.append(unpack_keyinfo(b))
    return parsed, raw


def write_element(el: Element, with_duo_extra: bool) -> bytes:
    """Exactly writeToConfig(e, io, duoExtra)."""
    w = Writer()
    w.put("<b", 1 if el.typ == 1 else 0)  # int8_t type; dPad() ? 1 : 0
    if el.typ == 1 and not el.is_ui:
        # VControllerDPad::Config: keys[4], float, int16, bool
        if len(el.raw_keys) != 4:
            raise ValueError("DPad requires 4 KeyInfo")
        for k in el.raw_keys:
            w.raw(k)
        w.put("<f", el.diagonal_sensitivity)
        w.put("<h", el.deadzone_mm100x)
        w.put("<B", 1 if el.visualize_bounds else 0)  # bool as 1 byte
    elif el.is_ui:
        w.put("<b", el.row_items)  # int8_t
        w.put("<B", pack_origin(*el.origin))
        write_sized_keys(w, el.raw_keys)
    else:
        w.put("<b", el.row_items)
        w.put("<b", el.spacing_mm)
        w.put("<b", el.x_padding)
        w.put("<b", el.y_padding)
        w.put("<B", el.stagger_type)
        w.put("<B", pack_origin(*el.origin))
        w.put("<B", 1 if el.show_bounding_area else 0)
        write_sized_keys(w, el.raw_keys)
    w.raw(el.layout_pos[0].pack())
    w.raw(el.layout_pos[1].pack())
    w.put("<B", el.state)
    if with_duo_extra:
        extra = el.duo_extra or DuoExtra()
        w.raw(extra.pack())
    return w.bytes()


def element_config_size(el: Element) -> int:
    """VControllerElement::configSize() — excludes the leading type byte,
    includes layoutPos[2] + state + the variant's own configSize()."""
    if el.typ == 1 and not el.is_ui:
        variant = 16 + 4 + 2 + 1  # keys + float + int16 + bool
    elif el.is_ui:
        variant = 1 + len(el.raw_keys) * 4 + 1 + 1  # count + keys + rowItems + origin
    else:
        variant = 1 + len(el.raw_keys) * 4 + 7  # count + keys + layoutConfig (7 bytes)
    return (4 + 1) * 2 + 1 + variant


def read_element(r: Reader, is_ui: bool, with_duo_extra: bool) -> Element:
    typ = r.get("<B")
    el = Element(typ=typ, is_ui=is_ui)
    if typ == 0:
        if is_ui:
            el.row_items = r.get("<b")
            el.origin = unpack_origin(r.get("<B"))
            el.keys, el.raw_keys = read_sized_keys(r)
        else:
            el.row_items = r.get("<b")
            el.spacing_mm = r.get("<b")
            el.x_padding = r.get("<b")
            el.y_padding = r.get("<b")
            el.stagger_type = r.get("<B")
            el.origin = unpack_origin(r.get("<B"))
            el.show_bounding_area = r.get("<B")
            el.keys, el.raw_keys = read_sized_keys(r)
    elif typ == 1 and not is_ui:
        raw, parsed = [], []
        for _ in range(4):
            b = r.raw(4)
            raw.append(b)
            parsed.append(unpack_keyinfo(b))
        el.raw_keys, el.keys = raw, parsed
        el.diagonal_sensitivity = r.get("<f")
        el.deadzone_mm100x = r.get("<h")
        el.visualize_bounds = r.get("<B")
    else:
        raise DecodeError(f"bad VControllerElement type {typ} (is_ui={is_ui})")
    el.layout_pos = [LayoutPos.unpack(r), LayoutPos.unpack(r)]
    el.state = r.get("<B")
    if with_duo_extra:
        el.duo_extra = DuoExtra.unpack(r)
    return el


def try_read_elements(
    payload_at_elements: bytes, count: int, is_ui: bool, with_duo_extra: bool
) -> tuple[list[Element], bytes]:
    r = Reader(payload_at_elements)
    elems = []
    for i in range(count):
        try:
            elems.append(read_element(r, is_ui, with_duo_extra))
        except DecodeError as e:
            raise DecodeError(f"element {i}: {e}") from e
    return elems, r.rest()


def read_elements_tolerant(
    payload_at_elements: bytes, count: int, is_ui: bool, prefer_duo_extra: bool
) -> tuple[list[Element], bytes, str]:
    """Parse `count` elements. Prefer `prefer_duo_extra` (True for key 125).

    The upcoming fix may append duoFlags/sizeMM100x for UI buttons, either
    after each element or as trailing bytes at the end of the 113 block.
    Never raise on leftover data: return it so the caller can print a hex dump.
    """
    note = ""
    # Attempt 1: preferred layout
    try:
        elems, rest = try_read_elements(payload_at_elements, count, is_ui, prefer_duo_extra)
        if not rest:
            return elems, rest, note
        # PR #2 format: uint8 removed flag, then 5 bytes per element
        if not prefer_duo_extra and len(rest) == 1 + 5 * count and all(e.duo_extra is None for e in elems):
            rr = Reader(rest)
            flag = rr.get("<B")
            for e in elems:
                e.duo_extra = DuoExtra.unpack(rr)
            return elems, bytes([flag]), "PR2_TRAILER"
        # leftover: maybe extras were stored at the end of the block (5 bytes * N)
        if not prefer_duo_extra and len(rest) == 5 * count and all(e.duo_extra is None for e in elems):
            rr = Reader(rest)
            for e in elems:
                e.duo_extra = DuoExtra.unpack(rr)
            return elems, b"", "interpreted leftover 5*N bytes as per-element duo extras (appended at end of block)"
        return elems, rest, note
    except DecodeError as first_err:
        first = first_err

    # Attempt 2: flip duo-extra interleaving
    try:
        elems, rest = try_read_elements(payload_at_elements, count, is_ui, not prefer_duo_extra)
        how = "with" if not prefer_duo_extra else "without"
        note = f"retried {how} per-element duo extras after: {first}"
        return elems, rest, note
    except DecodeError as second:
        # Best-effort: parse as many as possible without extras, dump the rest
        elems = []
        r = Reader(payload_at_elements)
        for i in range(count):
            start = r.tell()
            try:
                elems.append(read_element(r, is_ui, False))
            except DecodeError:
                r.offset = start
                rest = r.rest()
                note = (
                    f"stopped at element {i}/{count}: {first}; "
                    f"retry also failed: {second}"
                )
                return elems, rest, note
        return elems, r.rest(), f"partial parse; first error: {first}; retry: {second}"


# ---------------------------------------------------------------------------
# Block (de)serialization
# ---------------------------------------------------------------------------

def write_block(key: int, payload: bytes) -> bytes:
    """writeOptionValueHeader: size = payload_len + sizeof(key)."""
    size = len(payload) + 2
    if size > 0xFFFF:
        raise ValueError(f"block {key} too large: {size}")
    return struct.pack("<HH", size, key) + payload


def add_pr2_trailer(data: bytes, removed: int, size_mm100x: int = 900) -> bytes:
    """Append the PR #2 trailer to block 113: uint8 removed, then per element
    int16 sizeMM100x, uint8 ownAlpha, uint8 duoFlags, uint8 duoRole.
    The toggle group gets DUO_POS_MM and size_mm100x; others get zeros."""
    out = bytearray(data[:1])
    off = 1
    while off + 2 <= len(data):
        size = struct.unpack_from("<H", data, off)[0]
        key = struct.unpack_from("<H", data, off + 2)[0]
        payload = data[off + 4: off + 2 + size]
        if key == CFGKEY_VCONTROLLER_UI_BUTTONS_V2:
            blk = parse_ui_v2(payload)
            tr = Writer()
            tr.put("<B", removed)
            for e in blk.elements:
                if find_toggle([e]) is not None:
                    tr.put("<hBBB", size_mm100x, 0, 1, 0)
                else:
                    tr.put("<hBBB", 0, 0, 0, 0)
            payload = payload + tr.bytes()
        out += write_block(key, payload)
        off += 2 + size
    return bytes(out)


def write_ui_v2(elements: list[Element]) -> bytes:
    """writeUIButtonsConfig — size computed like configDataSizeBytes(..., true)."""
    # bytes = 2 (key) + 1 (config index) + 1 (count) + sum(1 + configSize)
    size = 2 + 1 + 1 + sum(1 + element_config_size(e) for e in elements)
    payload = Writer()
    payload.put("<b", 0)  # int8_t configId
    payload.put("<B", min(len(elements), 255))
    for e in elements:
        payload.raw(write_element(e, with_duo_extra=False))
    raw = payload.bytes()
    # size field includes the 2-byte key, so payload must be size-2
    if size != len(raw) + 2:
        raise RuntimeError(
            f"UI V2 size mismatch: configDataSizeBytes={size}, "
            f"payload+2={len(raw)+2}"
        )
    return write_block(CFGKEY_VCONTROLLER_UI_BUTTONS_V2, raw)


def write_device_v2(elements: list[Element]) -> bytes:
    """writeDeviceButtonsConfig."""
    size = 2 + 1 + 1 + 1 + sum(1 + element_config_size(e) for e in elements)
    payload = Writer()
    payload.put("<b", 0)  # int8_t emuDeviceId
    payload.put("<b", 0)  # int8_t configId
    payload.put("<B", min(len(elements), 255))
    for e in elements:
        payload.raw(write_element(e, with_duo_extra=False))
    raw = payload.bytes()
    if size != len(raw) + 2:
        raise RuntimeError(
            f"device V2 size mismatch: configDataSizeBytes={size}, "
            f"payload+2={len(raw)+2}"
        )
    return write_block(CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V2, raw)


def write_device_v3(
    elements: list[Element],
    *,
    two_player: bool = True,
    joint: bool = True,
    show_shared_row: bool = False,
    duo_customized: bool = False,
    p1_size: int = DEFAULT_P1_SIZE,
    p2_size: int = DEFAULT_P2_SIZE,
    p1_alpha: int = DEFAULT_P1_ALPHA,
    p2_alpha: int = DEFAULT_P2_ALPHA,
    shared_size: int = DEFAULT_SHARED_SIZE,
    shared_alpha: int = DEFAULT_SHARED_ALPHA,
    lock_player_sizes: bool = False,
) -> bytes:
    """writeDuoButtonsConfig. Current writer always sets flags bit 8 and
    therefore always writes sharedSize/sharedAlpha/lockPlayerSizes."""
    flags = (
        (1 if two_player else 0)
        | (2 if show_shared_row else 0)
        | (4 if duo_customized else 0)
        | 8
    )
    # bytes = 2 (key) + 1+1+1 + 2+2 + 1+1 + 2+1+1 + 1 (count)  = 16
    size = 2 + 1 + 1 + 1 + 2 + 2 + 1 + 1 + 2 + 1 + 1 + 1
    written_elems = elements if two_player else []
    if two_player:
        size += sum(1 + element_config_size(e) + 5 for e in written_elems)
    payload = Writer()
    payload.put("<B", 0)  # deviceId
    payload.put("<B", 1 if joint else 0)  # configId: 0=Separate, 1=Joint
    payload.put("<B", flags)
    payload.put("<h", p1_size)
    payload.put("<h", p2_size)
    payload.put("<B", p1_alpha)
    payload.put("<B", p2_alpha)
    payload.put("<h", shared_size)
    payload.put("<B", shared_alpha)
    payload.put("<B", 1 if lock_player_sizes else 0)
    payload.put("<B", min(len(written_elems), 255) if two_player else 0)
    if two_player:
        for e in written_elems:
            payload.raw(write_element(e, with_duo_extra=True))
    raw = payload.bytes()
    if size != len(raw) + 2:
        raise RuntimeError(
            f"device V3 size mismatch: computed={size}, payload+2={len(raw)+2}"
        )
    return write_block(CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3, raw)


def parse_ui_v2(payload: bytes) -> VBlock:
    r = Reader(payload)
    block = VBlock(kind="ui_v2")
    block.config_id = r.get("<B")
    count = r.get("<B")
    elems, rest, note = read_elements_tolerant(r.rest(), count, is_ui=True, prefer_duo_extra=False)
    # If count said N but we got fewer, still keep what we have
    if len(elems) < count and not note:
        note = f"declared {count} element(s), parsed {len(elems)}"
    if note == "PR2_TRAILER":
        block.removed_flag = rest[0]
        rest = b""
        note = f"PR #2 trailer: duoModeButtonRemoved={block.removed_flag}, per-element duo extras read"
    block.elements = elems
    block.trailing = rest
    block.trailing_note = note
    return block


def parse_device_v2(payload: bytes) -> VBlock:
    r = Reader(payload)
    block = VBlock(kind="device_v2")
    block.device_id = r.get("<B")
    block.config_id = r.get("<B")
    count = r.get("<B")
    elems, rest, note = read_elements_tolerant(r.rest(), count, is_ui=False, prefer_duo_extra=False)
    block.elements = elems
    block.trailing = rest
    block.trailing_note = note
    return block


def parse_device_v3(payload: bytes) -> VBlock:
    r = Reader(payload)
    block = VBlock(kind="device_v3")
    block.device_id = r.get("<B")
    block.config_id = r.get("<B")
    block.flags = r.get("<B")
    block.p1_size = r.get("<h")
    block.p2_size = r.get("<h")
    block.p1_alpha = r.get("<B")
    block.p2_alpha = r.get("<B")
    block.two_player = bool(block.flags & 1)
    block.has_shared_fields = bool(block.flags & 8)
    if block.has_shared_fields:
        block.shared_size = r.get("<h")
        block.shared_alpha = r.get("<B")
        block.lock_player_sizes = r.get("<B")
    count = r.get("<B")
    # C++ returns early without reading elements when !twoPlayer_
    if not block.two_player:
        block.trailing = r.rest()
        if count:
            block.trailing_note = f"twoPlayer=0 but count={count}; not reading elements (matches C++ early return)"
        return block
    elems, rest, note = read_elements_tolerant(r.rest(), count, is_ui=False, prefer_duo_extra=True)
    block.elements = elems
    block.trailing = rest
    block.trailing_note = note
    return block


def decode_bytes(data: bytes) -> ConfigFile:
    cfg = ConfigFile()
    if len(data) < 1:
        cfg.warnings.append("empty file")
        return cfg
    r = Reader(data)
    cfg.header = r.get("<B")
    if cfg.header != 2:
        cfg.warnings.append(
            f"header block size is {cfg.header}, not 2; "
            "C++ readConfigKeys refuses to continue. Parsing remaining bytes anyway."
        )
    file_left_start = r.remaining()
    # Walk blocks the same way as readConfigKeys
    while not r.eof() and r.remaining() >= 2:
        try:
            size = r.get("<H")
        except DecodeError as e:
            cfg.warnings.append(str(e))
            break
        if size == 0:
            cfg.warnings.append("invalid 0 size block, stopping")
            break
        if size > r.remaining():
            cfg.warnings.append(
                f"block size {size} exceeds remaining {r.remaining()} bytes, stopping"
            )
            break
        body = r.raw(size)
        if size < 2:
            cfg.warnings.append(f"skipping {size}-byte block (no key)")
            continue
        key = struct.unpack_from("<H", body, 0)[0]
        payload = body[2:]
        cfg.blocks.append((key, size, payload))
        try:
            if key == CFGKEY_VCONTROLLER_UI_BUTTONS_V2:
                cfg.parsed[key] = parse_ui_v2(payload)
            elif key == CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V2:
                cfg.parsed[key] = parse_device_v2(payload)
            elif key == CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3:
                cfg.parsed[key] = parse_device_v3(payload)
            else:
                cfg.unknown.append((key, size, payload))
        except DecodeError as e:
            cfg.warnings.append(f"key {key}: {e}")
            cfg.unknown.append((key, size, payload))
    if not r.eof():
        leftover = r.rest()
        cfg.warnings.append(f"{len(leftover)} trailing byte(s) after last block")
        cfg.unknown.append((-1, len(leftover), leftover))
    return cfg


def decode_file(path: Path) -> ConfigFile:
    return decode_bytes(path.read_bytes())


# ---------------------------------------------------------------------------
# Pretty printers
# ---------------------------------------------------------------------------

def key_name(key: int) -> str:
    return CFGKEY_NAMES.get(key, "UNKNOWN")


def find_toggle(elements: list[Element]) -> Optional[tuple[int, Element]]:
    for i, e in enumerate(elements):
        if has_app_key(e.keys, TOGGLE_DUO_LAYOUT):
            return i, e
    return None


def format_element(i: int, e: Element) -> str:
    lines = [
        f"    [{i}] type={e.type_name()} (type byte {e.typ})  "
        f"state={e.state} ({STATE_NAMES.get(e.state, '?')})"
    ]
    if e.typ == 0:
        lines.append(
            f"        rowItems={e.row_items}  layout.origin={origin_label(*e.origin)}"
        )
        if not e.is_ui:
            lines.append(
                f"        spacingMM={e.spacing_mm} xPadding={e.x_padding} "
                f"yPadding={e.y_padding} staggerType={e.stagger_type} "
                f"showBoundingArea={e.show_bounding_area}"
            )
        if e.keys:
            for j, k in enumerate(e.keys):
                lines.append(f"        key[{j}] {format_keyinfo(k)}")
        else:
            lines.append("        keys: (none)")
    elif e.typ == 1:
        lines.append(
            f"        diagonalSensitivity={e.diagonal_sensitivity} "
            f"deadzoneMM100x={e.deadzone_mm100x} visualizeBounds={e.visualize_bounds}"
        )
        for j, k in enumerate(e.keys):
            lines.append(f"        key[{j}] {format_keyinfo(k)}")
    lines.append(f"        layoutPos[0] landscape {e.layout_pos[0].label()}")
    lines.append(f"        layoutPos[1] portrait  {e.layout_pos[1].label()}")
    if e.duo_extra is not None:
        lines.append(f"        V3/UI duo extra: {e.duo_extra.label()}")
    return "\n".join(lines)


def format_vblock(key: int, block: VBlock) -> str:
    lines = []
    if block.kind == "ui_v2":
        lines.append(f"    configId={block.config_id}  elements={len(block.elements)}")
        hit = find_toggle(block.elements)
        if hit is None:
            lines.append("    FLAG: toggleDuoLayout group NOT present in key 113")
        else:
            i, e = hit
            lines.append(
                f"    FLAG: toggleDuoLayout group PRESENT at index {i}, "
                f"state={e.state} ({STATE_NAMES.get(e.state, '?')})"
            )
    elif block.kind == "device_v2":
        lines.append(
            f"    emuDeviceId={block.device_id} configId={block.config_id} "
            f"elements={len(block.elements)}"
        )
    elif block.kind == "device_v3":
        layout = "Joint" if block.config_id == 1 else "Separate" if block.config_id == 0 else f"configId={block.config_id}"
        lines.append(
            f"    deviceId={block.device_id} layout={layout} flags=0x{block.flags:02x} "
            f"twoPlayer={int(block.two_player)} showSharedRow={int(bool(block.flags & 2))} "
            f"duoCustomized={int(bool(block.flags & 4))} hasSharedFields={int(block.has_shared_fields)}"
        )
        lines.append(
            f"    p1Size={block.p1_size} p2Size={block.p2_size} "
            f"p1Alpha={block.p1_alpha} p2Alpha={block.p2_alpha}"
        )
        if block.has_shared_fields:
            lines.append(
                f"    sharedSize={block.shared_size} sharedAlpha={block.shared_alpha} "
                f"lockPlayerSizes={block.lock_player_sizes}"
            )
        lines.append(f"    elements={len(block.elements)}")
    for i, e in enumerate(block.elements):
        lines.append(format_element(i, e))
    if block.trailing_note:
        lines.append(f"    note: {block.trailing_note}")
    if block.trailing:
        lines.append(
            f"    TRAILING {len(block.trailing)} byte(s) left in this block "
            f"(printed as hex; not treated as a fatal error):"
        )
        lines.append(hexdump(block.trailing))
    return "\n".join(lines)


def format_config(cfg: ConfigFile, path: Optional[Path] = None) -> str:
    lines = []
    if path is not None:
        lines.append(f"file: {path}")
        lines.append(f"bytes: {path.stat().st_size if path.exists() else '?'}")
    lines.append(f"header (block size width): {cfg.header}")
    if cfg.header != 2:
        lines.append("  (C++ requires this to be 2)")
    lines.append(f"blocks: {len(cfg.blocks)}")
    for key, size, payload in cfg.blocks:
        lines.append("")
        lines.append(
            f"  key={key} ({key_name(key)})  size={size} "
            f"(payload {len(payload)} bytes; size includes the 2-byte key)"
        )
        if key in cfg.parsed:
            lines.append(format_vblock(key, cfg.parsed[key]))
        else:
            lines.append("    unknown / unparsed key; hex payload:")
            lines.append(hexdump(payload))
    for w in cfg.warnings:
        lines.append(f"WARNING: {w}")
    # summary flag even if 113 is missing
    if CFGKEY_VCONTROLLER_UI_BUTTONS_V2 not in cfg.parsed:
        lines.append("")
        lines.append("FLAG: no key 113 (CFGKEY_VCONTROLLER_UI_BUTTONS_V2) in this file")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Synthetic UI builders (Android defaults: TOUCH_DEVICES is true)
# ---------------------------------------------------------------------------

def ui_group(
    key_codes: list[int],
    *,
    row_items: int,
    origin: tuple[int, int],
    pos0: tuple[int, int],
    pos1: tuple[int, int],
    origin0: Optional[tuple[int, int]] = None,
    origin1: Optional[tuple[int, int]] = None,
    state: int = 1,
) -> Element:
    raw = [pack_app_key(c) for c in key_codes]
    keys = [unpack_keyinfo(b) for b in raw]
    return Element(
        typ=0,
        is_ui=True,
        row_items=row_items,
        origin=origin,
        keys=keys,
        raw_keys=raw,
        layout_pos=[
            LayoutPos(pos0[0], pos0[1], origin0 or origin),
            LayoutPos(pos1[0], pos1[1], origin1 or origin),
        ],
        state=state,
    )


def default_open_menu() -> Element:
    # VControllerUIButtonGroup(span, origin) sets rowItems = 4
    return ui_group(
        [OPEN_MENU],
        row_items=4,
        origin=ORIGIN_SHORT["RT"],
        pos0=DEFAULT_MENU_POS,
        pos1=DEFAULT_MENU_POS,
        state=1,
    )


def default_ff_rewind() -> Element:
    return ui_group(
        [TOGGLE_FF, REWIND],
        row_items=4,
        origin=ORIGIN_SHORT["LT"],
        pos0=DEFAULT_FF_POS,
        pos1=DEFAULT_FF_POS,
        state=1,
    )


def default_toggle(
    *,
    state: int = 1,
    pos0: tuple[int, int] = DEFAULT_TOGGLE_POS,
    pos1: tuple[int, int] = DEFAULT_TOGGLE_POS,
    origin0: tuple[int, int] = ORIGIN_SHORT["CT"],
    origin1: tuple[int, int] = ORIGIN_SHORT["CT"],
) -> Element:
    # syncDuoModeButton: rowItems=1, layout.origin=CT2DO, then setDuoPosition
    return ui_group(
        [TOGGLE_DUO_LAYOUT],
        row_items=1,
        origin=ORIGIN_SHORT["CT"],
        pos0=pos0,
        pos1=pos1,
        origin0=origin0,
        origin1=origin1,
        state=state,
    )


def default_ui_elements(
    *,
    include_toggle: bool,
    toggle_state: int = 1,
    toggle_pos0: tuple[int, int] = DEFAULT_TOGGLE_POS,
    toggle_pos1: tuple[int, int] = DEFAULT_TOGGLE_POS,
    toggle_origin0: tuple[int, int] = ORIGIN_SHORT["CT"],
    toggle_origin1: tuple[int, int] = ORIGIN_SHORT["CT"],
) -> list[Element]:
    elems = [default_open_menu(), default_ff_rewind()]
    if include_toggle:
        elems.append(
            default_toggle(
                state=toggle_state,
                pos0=toggle_pos0,
                pos1=toggle_pos1,
                origin0=toggle_origin0,
                origin1=toggle_origin1,
            )
        )
    return elems


def make_config_bytes(
    *,
    no_duo_toggle: bool = False,
    duo_toggle_state: Optional[int] = None,
    duo_toggle_moved: bool = False,
    pos0: Optional[tuple[int, int, tuple[int, int]]] = None,
    pos1: Optional[tuple[int, int, tuple[int, int]]] = None,
    v1_only: bool = False,
    no_113: bool = False,
) -> bytes:
    if v1_only:
        # Current C++ no longer writes or reads keys 68/100/101. This repo's
        # git history is a single commit, so the historic payload layout is
        # not available here. We emit a valid config *container* with only
        # those v1 keys so loaders that ignore unknown keys behave like an
        # old file (no 112/113/125 → uiElements keep the constructor
        # sentinel pos.x == -1 → configure() calls resetUIGroups()).
        out = Writer()
        out.put("<B", HEADER_BLOCK_SIZE)
        # key 68: a handful of int16 -1 sentinels as a placeholder payload
        payload68 = struct.pack("<" + "h" * 8, *([-1] * 8))
        out.raw(write_block(CFGKEY_VCONTROLLER_LAYOUT_POS, payload68))
        # keys 100 / 101: non-empty but undocumented placeholder
        out.raw(write_block(CFGKEY_VCONTROLLER_DEVICE_BUTTONS, b"\x00"))
        out.raw(write_block(CFGKEY_VCONTROLLER_UI_BUTTONS, b"\x00"))
        return out.bytes()

    include_toggle = not no_duo_toggle
    if duo_toggle_state is None:
        state = 2 if duo_toggle_moved else 1
    else:
        state = duo_toggle_state

    tpos0, torig0 = DEFAULT_TOGGLE_POS, ORIGIN_SHORT["CT"]
    tpos1, torig1 = DEFAULT_TOGGLE_POS, ORIGIN_SHORT["CT"]
    if duo_toggle_moved:
        tpos0, torig0 = MOVED_POS0, ORIGIN_SHORT["CB"]
        tpos1, torig1 = MOVED_POS1, ORIGIN_SHORT["LT"]
    if pos0 is not None:
        tpos0, torig0 = (pos0[0], pos0[1]), pos0[2]
    if pos1 is not None:
        tpos1, torig1 = (pos1[0], pos1[1]), pos1[2]

    out = Writer()
    out.put("<B", HEADER_BLOCK_SIZE)
    # Default twoPlayer_=true, duoLayout_=Joint, so the C++ writer emits 125
    # (with whatever gpElements are in memory) and 113, but not 112.
    # Synthetic samples use an empty gpElements list: the V3 *header*
    # matches writeDuoButtonsConfig exactly; gamepad buttons are omitted
    # because these files exist to exercise UI key 113.
    out.raw(write_device_v3([], two_player=True, joint=True))
    if not no_113:
        elems = default_ui_elements(
            include_toggle=include_toggle,
            toggle_state=state,
            toggle_pos0=tpos0,
            toggle_pos1=tpos1,
            toggle_origin0=torig0,
            toggle_origin1=torig1,
        )
        out.raw(write_ui_v2(elems))
    return out.bytes()


def write_config(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


# ---------------------------------------------------------------------------
# diff
# ---------------------------------------------------------------------------

def ui_summary(cfg: ConfigFile) -> list[dict]:
    """One record per UI element in key 113, used by diff."""
    block = cfg.parsed.get(CFGKEY_VCONTROLLER_UI_BUTTONS_V2)
    if block is None:
        return []
    recs = []
    for i, e in enumerate(block.elements):
        names = []
        for k in e.keys:
            if k["flags"]["appCode"] and 1 <= k["codes"][0] <= len(APP_KEY_NAMES):
                names.append(APP_KEY_NAMES[k["codes"][0] - 1])
            else:
                names.append(format_keyinfo(k))
        recs.append(
            {
                "index": i,
                "keys": tuple(names),
                "has_toggle": has_app_key(e.keys, TOGGLE_DUO_LAYOUT),
                "state": e.state,
                "state_name": STATE_NAMES.get(e.state, str(e.state)),
                "pos0": (e.layout_pos[0].x, e.layout_pos[0].y, e.layout_pos[0].origin),
                "pos1": (e.layout_pos[1].x, e.layout_pos[1].y, e.layout_pos[1].origin),
                "row_items": e.row_items,
            }
        )
    return recs


def _pos_str(p: tuple) -> str:
    x, y, origin = p
    return f"({x},{y}) {origin_label(*origin)}"


def diff_configs(a: ConfigFile, b: ConfigFile, name_a: str, name_b: str) -> str:
    sa, sb = ui_summary(a), ui_summary(b)
    lines = [f"UI element diff: {name_a}  ->  {name_b}"]

    def has113(cfg: ConfigFile) -> bool:
        return CFGKEY_VCONTROLLER_UI_BUTTONS_V2 in {k for k, _, _ in cfg.blocks}

    if not has113(a):
        lines.append(f"  {name_a}: no key 113")
    if not has113(b):
        lines.append(f"  {name_b}: no key 113")

    def keyset(recs):
        return {r["keys"] for r in recs}

    only_a = keyset(sa) - keyset(sb)
    only_b = keyset(sb) - keyset(sa)
    if only_a:
        for k in sorted(only_a):
            rec = next(r for r in sa if r["keys"] == k)
            lines.append(
                f"  REMOVED  keys={list(k)}  state={rec['state_name']}  "
                f"pos0={_pos_str(rec['pos0'])}  pos1={_pos_str(rec['pos1'])}"
            )
    if only_b:
        for k in sorted(only_b):
            rec = next(r for r in sb if r["keys"] == k)
            lines.append(
                f"  ADDED    keys={list(k)}  state={rec['state_name']}  "
                f"pos0={_pos_str(rec['pos0'])}  pos1={_pos_str(rec['pos1'])}"
            )

    for rec_a in sa:
        rec_b = next((r for r in sb if r["keys"] == rec_a["keys"]), None)
        if rec_b is None:
            continue
        changes = []
        if rec_a["state"] != rec_b["state"]:
            changes.append(f"state {rec_a['state_name']} -> {rec_b['state_name']}")
        if rec_a["pos0"] != rec_b["pos0"]:
            changes.append(f"layoutPos[0] {_pos_str(rec_a['pos0'])} -> {_pos_str(rec_b['pos0'])}")
        if rec_a["pos1"] != rec_b["pos1"]:
            changes.append(f"layoutPos[1] {_pos_str(rec_a['pos1'])} -> {_pos_str(rec_b['pos1'])}")
        if rec_a["row_items"] != rec_b["row_items"]:
            changes.append(f"rowItems {rec_a['row_items']} -> {rec_b['row_items']}")
        if changes:
            lines.append(f"  CHANGED  keys={list(rec_a['keys'])}")
            for c in changes:
                lines.append(f"           {c}")

    # Toggle-focused one-liner
    ta = next((r for r in sa if r["has_toggle"]), None)
    tb = next((r for r in sb if r["has_toggle"]), None)
    lines.append("")
    if ta and tb:
        same = (
            ta["state"] == tb["state"]
            and ta["pos0"] == tb["pos0"]
            and ta["pos1"] == tb["pos1"]
        )
        if same:
            lines.append(
                f"  toggleDuoLayout: still present, state={tb['state_name']}, positions unchanged"
            )
        else:
            lines.append("  toggleDuoLayout: still present, but state/position changed (see above)")
    elif ta and not tb:
        lines.append("  toggleDuoLayout: present in first file, missing in second")
    elif tb and not ta:
        lines.append("  toggleDuoLayout: missing in first file, present in second")
    else:
        lines.append("  toggleDuoLayout: absent from both files")

    if len(lines) == 1 or (not only_a and not only_b and all(
        next((r for r in sb if r["keys"] == ra["keys"]), None)
        and next(r for r in sb if r["keys"] == ra["keys"])["state"] == ra["state"]
        and next(r for r in sb if r["keys"] == ra["keys"])["pos0"] == ra["pos0"]
        and next(r for r in sb if r["keys"] == ra["keys"])["pos1"] == ra["pos1"]
        for ra in sa
    ) and has113(a) == has113(b) and len(sa) == len(sb)):
        if has113(a) == has113(b) and not only_a and not only_b:
            # Check if we already recorded changes
            if not any(l.startswith("  CHANGED") or l.startswith("  REMOVED") or l.startswith("  ADDED") for l in lines):
                lines.insert(1, "  (no UI presence / position / state differences)")

    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Self-test + sample generation
# ---------------------------------------------------------------------------

class SelfTestFailure(Exception):
    pass


def _assert(cond: bool, msg: str) -> None:
    if not cond:
        raise SelfTestFailure(msg)


def run_selftest(samples_dir: Path) -> str:
    log: list[str] = []

    def ok(msg: str) -> None:
        log.append(f"  PASS  {msg}")

    # --- make + decode each variant ---
    cases = {
        "toggle-present.cfg": dict(),
        "toggle-removed.cfg": dict(no_duo_toggle=True),
        "no-113.cfg": dict(no_113=True),
        "v1-only.cfg": dict(v1_only=True),
        "toggle-moved-hidden.cfg": dict(duo_toggle_moved=True, duo_toggle_state=2),
        "toggle-moved-off.cfg": dict(duo_toggle_moved=True, duo_toggle_state=0),
    }
    blobs: dict[str, bytes] = {}
    cfgs: dict[str, ConfigFile] = {}
    for name, kwargs in cases.items():
        data = make_config_bytes(**kwargs)
        blobs[name] = data
        cfgs[name] = decode_bytes(data)
        _assert(cfgs[name].header == 2, f"{name}: header is {cfgs[name].header}")
        _assert(not cfgs[name].warnings, f"{name}: unexpected warnings: {cfgs[name].warnings}")
        ok(f"make {name} ({len(data)} bytes) decodes with header 2 and no warnings")

    # present
    p = cfgs["toggle-present.cfg"]
    _assert(CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3 in p.parsed, "present: missing key 125")
    _assert(CFGKEY_VCONTROLLER_UI_BUTTONS_V2 in p.parsed, "present: missing key 113")
    _assert(CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V2 not in p.parsed, "present: unexpected key 112")
    v3 = p.parsed[CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3]
    _assert(v3.two_player and v3.config_id == 1 and v3.flags == 9, f"present: V3 header {v3.flags} {v3.config_id}")
    _assert(v3.p1_size == 1000 and v3.p1_alpha == 127 and v3.shared_alpha == 102, "present: V3 default sizes/alphas")
    ui = p.parsed[CFGKEY_VCONTROLLER_UI_BUTTONS_V2]
    hit = find_toggle(ui.elements)
    _assert(hit is not None, "present: toggleDuoLayout missing")
    i, el = hit
    _assert(el.state == 1, f"present: state {el.state} != SHOWN")
    _assert(
        (el.layout_pos[0].x, el.layout_pos[0].y) == DEFAULT_TOGGLE_POS
        and (el.layout_pos[1].x, el.layout_pos[1].y) == DEFAULT_TOGGLE_POS
        and el.layout_pos[0].origin == ORIGIN_SHORT["CT"]
        and el.layout_pos[1].origin == ORIGIN_SHORT["CT"],
        "present: toggle positions are not the syncDuoModeButton default (0,300) CT2DO",
    )
    _assert(el.row_items == 1, "present: toggle rowItems should be 1")
    _assert(has_app_key(ui.elements[0].keys, OPEN_MENU), "present: openMenu missing")
    _assert(ui.elements[0].layout_pos[0].x != -1, "present: openMenu pos.x is sentinel -1")
    _assert(has_app_key(ui.elements[1].keys, TOGGLE_FF) and has_app_key(ui.elements[1].keys, REWIND), "present: FF/rewind missing")
    _assert(ui.elements[0].row_items == 4 and ui.elements[1].row_items == 4, "present: default UI rowItems should be 4")
    ok("toggle-present: 125+113, no 112, toggle SHOWN at (0,300) CT both layouts, openMenu/FF present, no sentinel")

    # removed
    rmcfg = cfgs["toggle-removed.cfg"]
    ui = rmcfg.parsed[CFGKEY_VCONTROLLER_UI_BUTTONS_V2]
    _assert(find_toggle(ui.elements) is None, "removed: toggle still present")
    _assert(len(ui.elements) == 2, f"removed: expected 2 UI groups, got {len(ui.elements)}")
    _assert(has_app_key(ui.elements[0].keys, OPEN_MENU), "removed: openMenu missing")
    _assert(ui.elements[0].layout_pos[0].x != -1, "removed: openMenu pos.x is sentinel -1 (would reset UI on load)")
    ok("toggle-removed: key 113 has openMenu+FF only, no toggle, first pos.x != -1")

    # no-113
    n113 = cfgs["no-113.cfg"]
    keys = {k for k, _, _ in n113.blocks}
    _assert(CFGKEY_VCONTROLLER_UI_BUTTONS_V2 not in keys, "no-113: key 113 present")
    _assert(CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3 in keys, "no-113: key 125 missing")
    ok("no-113: has 125, no 113")

    # v1-only
    v1 = cfgs["v1-only.cfg"]
    keys = {k for k, _, _ in v1.blocks}
    _assert(keys == {68, 100, 101}, f"v1-only: keys {keys}")
    _assert(112 not in keys and 113 not in keys and 125 not in keys, "v1-only: contains a V2/V3 key")
    ok("v1-only: keys 68, 100, 101 only")

    # moved / hidden
    mv = cfgs["toggle-moved-hidden.cfg"]
    ui = mv.parsed[CFGKEY_VCONTROLLER_UI_BUTTONS_V2]
    hit = find_toggle(ui.elements)
    _assert(hit is not None, "moved-hidden: toggle missing")
    _, el = hit
    _assert(el.state == 2, f"moved-hidden: state {el.state} != HIDDEN")
    _assert((el.layout_pos[0].x, el.layout_pos[0].y) == MOVED_POS0, "moved-hidden: landscape pos")
    _assert((el.layout_pos[1].x, el.layout_pos[1].y) == MOVED_POS1, "moved-hidden: portrait pos")
    _assert(el.layout_pos[0].origin == ORIGIN_SHORT["CB"], "moved-hidden: landscape origin")
    _assert(el.layout_pos[1].origin == ORIGIN_SHORT["LT"], "moved-hidden: portrait origin")
    ok("toggle-moved-hidden: present, state HIDDEN, custom land/port positions")

    off = cfgs["toggle-moved-off.cfg"]
    _, el = find_toggle(off.parsed[CFGKEY_VCONTROLLER_UI_BUTTONS_V2].elements)
    _assert(el.state == 0, f"moved-off: state {el.state} != OFF")
    ok("toggle-moved-off: present, state OFF, custom positions")

    # size field includes the 2-byte key (spot-check 113 of present file)
    present_113 = next(s for k, s, pld in p.blocks if k == 113)
    present_pld = next(pld for k, s, pld in p.blocks if k == 113)
    _assert(present_113 == len(present_pld) + 2, "113 size field does not include the key")
    # 3 UI groups: 2 (key) + 1 (configId) + 1 (count) + (19+4? wait) verify against writer
    expected = 2 + 1 + 1
    for e in p.parsed[113].elements:
        expected += 1 + element_config_size(e)
    _assert(present_113 == expected, f"113 size {present_113} != configDataSizeBytes {expected}")
    ok(f"key 113 size field {present_113} matches configDataSizeBytes")

    # V3 size: empty two-player list → 18
    present_125 = next(s for k, s, pld in p.blocks if k == 125)
    _assert(present_125 == 16, f"empty V3 size should be 16, got {present_125}")
    ok("key 125 size is 16 for empty two-player V3 (2+14)")

    # --- diff ---
    d = diff_configs(p, rmcfg, "present", "removed")
    _assert("REMOVED" in d and "toggleDuoLayout" in d, f"diff present->removed missed toggle:\n{d}")
    _assert("missing in second" in d, f"diff summary:\n{d}")
    ok("diff present vs removed reports toggle removed")

    d2 = diff_configs(p, mv, "present", "moved-hidden")
    _assert("CHANGED" in d2 and "HIDDEN" in d2, f"diff present->moved:\n{d2}")
    ok("diff present vs moved-hidden reports state/position change")

    d3 = diff_configs(p, p, "present", "present")
    _assert("no UI presence" in d3 or "positions unchanged" in d3, f"diff identical:\n{d3}")
    ok("diff of a file with itself reports no differences")

    # --- trailing bytes / unknown key tolerance ---
    # Append 5 bytes of fake duo extra at the end of the 113 payload (after
    # rewriting the size field), once per element, and also a lone unknown key.
    base = blobs["toggle-present.cfg"]
    # Rebuild: header + 125 block + patched 113 + unknown key 200
    r = Reader(base)
    header = r.get("<B")
    parts = [struct.pack("<B", header)]
    while not r.eof():
        size = r.get("<H")
        body = r.raw(size)
        key = struct.unpack_from("<H", body, 0)[0]
        payload = body[2:]
        if key == 113:
            n_el = len(p.parsed[113].elements)
            extra = b"\x84\x03\x00\x01\xff" * n_el  # sizeMM100x=900, alpha=0, flags=1, role=255
            new_payload = payload + extra
            parts.append(write_block(113, new_payload))
        else:
            parts.append(struct.pack("<HH", size, key) + payload)
    parts.append(write_block(200, b"\xde\xad\xbe\xef"))
    tolerant = b"".join(parts)
    tcfg = decode_bytes(tolerant)
    _assert(not any("truncated" in w or "stopping" in w for w in tcfg.warnings), f"tolerant decode warnings: {tcfg.warnings}")
    ui = tcfg.parsed[113]
    _assert(find_toggle(ui.elements) is not None, "tolerant: failed to parse toggle with trailing extras")
    _assert(all(e.duo_extra is not None for e in ui.elements), "tolerant: did not interpret 5*N trailing as duo extras")
    _assert(ui.elements[0].duo_extra.size_mm100x == 900, "tolerant: sizeMM100x")
    unk = [u for u in tcfg.unknown if u[0] == 200]
    _assert(unk and unk[0][2] == b"\xde\xad\xbe\xef", "tolerant: unknown key 200 not preserved")
    text = format_config(tcfg)
    _assert("UNKNOWN" in text and "de ad be ef" in text, "tolerant: decode text missing unknown hex")
    ok("decoder is tolerant: trailing 5*N bytes on 113 become duo extras; unknown key 200 printed as hex")

    # Interleaved extras (after each element's state) should also parse
    # Build a 113 payload with write_element(..., True) but keep key 113
    elems = default_ui_elements(include_toggle=True)
    for e in elems:
        e.duo_extra = DuoExtra(900, 0, 1, 255)
    w = Writer()
    w.put("<b", 0)
    w.put("<B", len(elems))
    for e in elems:
        w.raw(write_element(e, with_duo_extra=True))
    interleaved = struct.pack("<B", 2) + write_device_v3([]) + write_block(113, w.bytes())
    icfg = decode_bytes(interleaved)
    ui = icfg.parsed[113]
    _assert(len(ui.elements) == 3 and all(e.duo_extra and e.duo_extra.size_mm100x == 900 for e in ui.elements), "interleaved extras failed")
    _assert(find_toggle(ui.elements) is not None, "interleaved: toggle missing")
    ok("decoder retries with per-element duo extras when they are interleaved in key 113")

    # --- write samples (the four official cases + moved/hidden) ---
    samples_dir.mkdir(parents=True, exist_ok=True)
    official = [
        "toggle-present.cfg",
        "toggle-removed.cfg",
        "no-113.cfg",
        "v1-only.cfg",
        "toggle-moved-hidden.cfg",
    ]
    for name in official:
        write_config(samples_dir / name, blobs[name])
        # round-trip: file on disk decodes the same
        on_disk = decode_file(samples_dir / name)
        _assert(on_disk.blocks == cfgs[name].blocks, f"{name}: disk round-trip block mismatch")
        ok(f"wrote {samples_dir / name} and round-tripped")

    log.append("ALL SELF-TESTS PASSED")
    return "\n".join(log) + "\n"


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Decode, create, and diff NEO-Duo binary config files."
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    d = sub.add_parser("decode", help="print header, every block, and VController elements")
    d.add_argument("config", type=Path)

    m = sub.add_parser("make", help="write a synthetic config matching the C++ writer")
    m.add_argument("out", type=Path)
    m.add_argument("--pr2-removed", type=int, choices=[0, 1], default=None,
                   help="write the PR #2 format: 113 trailer with duoModeButtonRemoved flag + per-element duo extras")
    m.add_argument("--no-duo-toggle", action="store_true", help="omit the toggleDuoLayout UI group (removed-button case)")
    m.add_argument("--duo-toggle-state", type=int, metavar="N", choices=(0, 1, 2), help="VControllerState for the toggle group: 0=OFF 1=SHOWN 2=HIDDEN")
    m.add_argument("--duo-toggle-moved", action="store_true", help="give the toggle group custom landscape/portrait positions (and default state HIDDEN unless --duo-toggle-state is set)")
    m.add_argument("--pos0", type=str, help="landscape layoutPos as x,y,ORIGIN (e.g. 80,120,CB)")
    m.add_argument("--pos1", type=str, help="portrait layoutPos as x,y,ORIGIN (e.g. -40,250,LT)")
    m.add_argument("--v1-only", action="store_true", help="old-file case: only keys 68/100/101, no 112/113/125")
    m.add_argument("--no-113", action="store_true", help="old-file case: current V3 header, but no key 113")

    df = sub.add_parser("diff", help="compare UI elements (presence, positions, state) in two configs")
    df.add_argument("a", type=Path)
    df.add_argument("b", type=Path)

    st = sub.add_parser("selftest", help="round-trip every variant, then write official samples")
    st.add_argument("--samples-dir", type=Path, default=Path(__file__).resolve().parent / "samples")
    return p


def main(argv: Optional[list[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    if args.cmd == "decode":
        cfg = decode_file(args.config)
        sys.stdout.write(format_config(cfg, args.config))
        return 0
    if args.cmd == "make":
        pos0 = parse_pos_arg(args.pos0) if args.pos0 else None
        pos1 = parse_pos_arg(args.pos1) if args.pos1 else None
        data = make_config_bytes(
            no_duo_toggle=args.no_duo_toggle,
            duo_toggle_state=args.duo_toggle_state,
            duo_toggle_moved=args.duo_toggle_moved,
            pos0=pos0,
            pos1=pos1,
            v1_only=args.v1_only,
            no_113=args.no_113,
        )
        if args.pr2_removed is not None:
            data = add_pr2_trailer(data, args.pr2_removed)
        write_config(args.out, data)
        print(f"wrote {args.out} ({len(data)} bytes)")
        return 0
    if args.cmd == "diff":
        a, b = decode_file(args.a), decode_file(args.b)
        sys.stdout.write(diff_configs(a, b, str(args.a), str(args.b)))
        return 0
    if args.cmd == "selftest":
        try:
            sys.stdout.write(run_selftest(args.samples_dir))
        except SelfTestFailure as e:
            sys.stderr.write(f"SELF-TEST FAILED: {e}\n")
            return 1
        return 0
    return 2


if __name__ == "__main__":
    sys.exit(main())
