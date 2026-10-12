#include "input/Hotkeys.h"

namespace
{
    // (GLFW's key codes, without including GLFW in core/)
    constexpr int kKeyTab = 258, kKeyEnter = 257, kKeyF1 = 290;
    constexpr int kKeyLeftShift = 340, kKeyRightSuper = 347;
} // namespace

const char *HotkeyActionName(HotkeyAction action)
{
    switch (action)
    {
    case HotkeyAction::Menu: return "Open the menu";
    case HotkeyAction::Fullscreen: return "Full screen";
    case HotkeyAction::Screenshot: return "Screenshot";
    case HotkeyAction::SaveState: return "Save state";
    case HotkeyAction::LoadState: return "Load state";
    case HotkeyAction::PreviousSlot: return "Previous slot";
    case HotkeyAction::NextSlot: return "Next slot";
    case HotkeyAction::Reset: return "Reset game";
    case HotkeyAction::CloseGame: return "Close game";
    case HotkeyAction::Quit: return "Quit";
    }
    return "";
}

Hotkey DefaultHotkey(HotkeyAction action)
{
    switch (action)
    {
    case HotkeyAction::Menu: return {kKeyTab, 0};
    case HotkeyAction::Fullscreen: return {kKeyEnter, kHotkeyAlt};
    case HotkeyAction::Screenshot: return {kKeyF1 + 11, 0}; // F12
    case HotkeyAction::SaveState: return {kKeyF1 + 1, 0};   // F2
    case HotkeyAction::LoadState: return {kKeyF1 + 3, 0};   // F4
    case HotkeyAction::PreviousSlot: return {kKeyF1 + 5, 0}; // F6
    case HotkeyAction::NextSlot: return {kKeyF1 + 6, 0};    // F7
    case HotkeyAction::Reset: return {'R', kHotkeyCtrl};
    case HotkeyAction::CloseGame: return {};
    case HotkeyAction::Quit: return {'Q', kHotkeyCtrl};
    }
    return {};
}

std::string KeyboardKeyName(int key)
{
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return std::string(1, static_cast<char>(key));
    switch (key)
    {
    case 32: return "Space";
    case 39: return "'";
    case 44: return ",";
    case 45: return "-";
    case 46: return ".";
    case 47: return "/";
    case 59: return ";";
    case 61: return "=";
    case 91: return "[";
    case 92: return "\\";
    case 93: return "]";
    case 96: return "`";
    case 256: return "Escape";
    case 257: return "Enter";
    case 258: return "Tab";
    case 259: return "Backspace";
    case 260: return "Insert";
    case 261: return "Delete";
    case 262: return "Right Arrow";
    case 263: return "Left Arrow";
    case 264: return "Down Arrow";
    case 265: return "Up Arrow";
    case 266: return "Page Up";
    case 267: return "Page Down";
    case 268: return "Home";
    case 269: return "End";
    case 280: return "Caps Lock";
    case 281: return "Scroll Lock";
    case 282: return "Num Lock";
    case 283: return "Print Screen";
    case 284: return "Pause";
    case 330: return "Num .";
    case 331: return "Num /";
    case 332: return "Num *";
    case 333: return "Num -";
    case 334: return "Num +";
    case 335: return "Num Enter";
    case 336: return "Num =";
    case 348: return "Menu";
    default:
        if (key >= kKeyF1 && key <= kKeyF1 + 24)
            return "F" + std::to_string(key - kKeyF1 + 1);
        if (key >= 320 && key <= 329)
            return "Num " + std::to_string(key - 320);
        return "Key " + std::to_string(key);
    }
}

std::string HotkeyName(const Hotkey &hotkey)
{
    if (!hotkey.Bound())
        return "-";
    std::string name;
    if (hotkey.mods & kHotkeyCtrl)
        name += "Ctrl+";
    if (hotkey.mods & kHotkeyAlt)
        name += "Alt+";
    if (hotkey.mods & kHotkeyShift)
        name += "Shift+";
    return name + KeyboardKeyName(hotkey.key);
}

bool IsModifierKey(int key) { return key >= kKeyLeftShift && key <= kKeyRightSuper; }

bool IsPaintingToolKey(int key) { return key >= kKeyF1 + 3 && key <= kKeyF1 + 11; }
