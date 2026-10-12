#pragma once

#include <cstdint>
#include <string>

// The window app's hotkeys (Settings > Hotkeys - the PC only): an action on a
// key, alone or with Ctrl / Alt / Shift held (Alt+F1). Keys are the
// platform's key codes (GLFW's in the window app), as ButtonMapper's
// DeviceKeyboard bindings are.
enum class HotkeyAction
{
    Menu = 0,     // open / close the menu
    Fullscreen,   // the window full screen and back
    Screenshot,   // the game's picture to the screenshots folder
    SaveState,    // into the Save states page's slot
    LoadState,    // from it
    PreviousSlot, // that slot, one down
    NextSlot,     // ... one up
    Reset,        // the game from power-on
    CloseGame,    // back to the library, no game running
    Quit,         // close VBoy Color
};
inline constexpr int kHotkeyActionCount = 10;
// (room in the settings file for more actions later)
inline constexpr int kHotkeySlots = 16;

// Modifier bits of a hotkey (the platform's own are translated to these).
inline constexpr int32_t kHotkeyCtrl = 1, kHotkeyAlt = 2, kHotkeyShift = 4;

struct Hotkey
{
    int32_t key = -1; // -1: none
    int32_t mods = 0; // kHotkeyCtrl | kHotkeyAlt | kHotkeyShift
    bool Bound() const { return key >= 0; }
};

// How the menu names an action ("Save state").
const char *HotkeyActionName(HotkeyAction action);
// Its default (GLFW key codes): Tab, Alt+Enter, F12, F2, F4, F6, F7, Ctrl+R,
// none, Ctrl+Q.
Hotkey DefaultHotkey(HotkeyAction action);
// A keyboard key's name ("F1", "Enter", "Q") - GLFW key codes.
std::string KeyboardKeyName(int key);
// "Ctrl+Alt+F1", or "-" for none.
std::string HotkeyName(const Hotkey &hotkey);
// A modifier key itself (Ctrl, Alt, Shift, Super - left or right): never a
// hotkey's key, only held with one.
bool IsModifierKey(int key);
// The color pack and video tools' keys (F4 to F12 - see
// platform/pc2d/Main.cpp): theirs alone (or with Shift), while Settings >
// Hotkeys > Color pack and video tools is on.
bool IsPaintingToolKey(int key);
