/* Host test for the Toggle Control Layout button decision.
   No Android NDK or Imagine headers:

   g++ -std=c++20 -Wall -Werror -I EmuFramework/include EmuFramework/tests/duoModeButtonTest.cc -o duoModeButtonTest && ./duoModeButtonTest
*/

#include <emuframework/DuoModeButton.hh>
#include <cstdlib>
#include <iostream>

namespace
{

int fails{};

void expect(bool cond, const char *msg)
{
	if(!cond)
	{
		std::cerr << "FAIL " << msg << '\n';
		++fails;
	}
}

using EmuEx::DuoModeButtonSync;
using EmuEx::decideDuoModeButton;
using EmuEx::duoModeButtonShouldExist;

}

int main()
{
	expect(duoModeButtonShouldExist(true, false), "default (not removed) shows the button");
	expect(!duoModeButtonShouldExist(true, true), "user removal hides the button");
	expect(!duoModeButtonShouldExist(false, false), "two-player off hides the button");

	// Old config: the removed field is absent, so userRemoved is false.
	// Keep does not rebuild, so a moved position and HIDDEN state stay.
	expect(decideDuoModeButton(true, false, true) == DuoModeButtonSync::Keep,
		"moved or hidden button is kept");
	expect(decideDuoModeButton(true, false, true, false) == DuoModeButtonSync::Keep,
		"relaunch does not reset position or state");
	expect(decideDuoModeButton(true, false, false) == DuoModeButtonSync::Create,
		"old config without the element still creates it");

	// User removed the button and that choice was written.
	expect(decideDuoModeButton(true, true, false) == DuoModeButtonSync::Keep,
		"removal stays removed across relaunch");
	expect(decideDuoModeButton(true, true, true) == DuoModeButtonSync::Erase,
		"a stale button is dropped when removal was saved");

	// Reset UI Groups forces a fresh default button even if one is present
	// or the user had removed it.
	expect(decideDuoModeButton(true, false, true, true) == DuoModeButtonSync::Create,
		"reset UI groups replaces a moved or hidden button");
	expect(decideDuoModeButton(true, true, false, true) == DuoModeButtonSync::Create,
		"reset UI groups brings a removed button back");

	// Two-player off erases it. Turning it back on clears userRemoved,
	// so the next call is (true, false, false) and the button is created.
	expect(decideDuoModeButton(false, false, true) == DuoModeButtonSync::Erase,
		"two-player off removes the button");
	expect(decideDuoModeButton(false, false, false) == DuoModeButtonSync::Keep,
		"two-player off with no button does nothing");
	expect(decideDuoModeButton(true, false, false) == DuoModeButtonSync::Create,
		"two-player back on creates the button");

	if(fails)
	{
		std::cerr << fails << " failure(s)\n";
		return EXIT_FAILURE;
	}
	std::cout << "duoModeButtonTest ok\n";
	return EXIT_SUCCESS;
}
