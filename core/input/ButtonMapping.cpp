#include "input/ButtonMapping.h"

#include "emu/Emulator.h" // VBButtonBit

namespace ButtonMapper
{

uint32_t TranslateToVBBitmask(const uint32_t buttonStates[3], const MappedButtons (&vbButtons)[16])
{
    uint32_t vbBits = 0;
    for (uint32_t vbBit = 0; vbBit < 16; ++vbBit)
    {
        if (vbBit == MenuSlot)
            continue; // (the app's menu, not the game's)
        for (const MappedButton &binding : vbButtons[vbBit].Buttons)
        {
            if (!binding.IsSet)
                continue;
            if (binding.InputDevice < 0 || binding.InputDevice > 2)
                continue;
            if (buttonStates[binding.InputDevice] & ButtonMapping[binding.ButtonIndex])
            {
                vbBits |= (1u << vbBit);
                break; // either binding is enough - no need to check the other
            }
        }
    }
    return vbBits;
}

const MappedButtons &MenuBindings(const MappedButtons (&mapping)[16], const MappedButtons (&general)[16])
{
    const MappedButtons &own = mapping[MenuSlot];
    return own.Buttons[0].IsSet || own.Buttons[1].IsSet ? own : general[MenuSlot];
}

bool MenuButtonDown(const uint32_t buttonStates[3], const MappedButtons &menu)
{
    for (const MappedButton &binding : menu.Buttons)
        if (binding.IsSet && binding.InputDevice >= 0 && binding.InputDevice <= 2 && binding.ButtonIndex >= 0 &&
            binding.ButtonIndex < EmuButtonCount && (buttonStates[binding.InputDevice] & ButtonMapping[binding.ButtonIndex]))
            return true;
    return false;
}

void ApplyDefaultGamepadBindings(MappedButtons (&vbButtons)[16], bool byPosition)
{
    auto setDefault = [&](uint32_t vbBit, uint32_t emuButton)
    {
        MappedButton &b = vbButtons[vbBit].Buttons[1];
        if (!b.IsSet)
        {
            b.IsSet = true;
            b.InputDevice = DeviceGamepad;
            b.ButtonIndex = static_cast<int>(emuButton);
        }
    };
    setDefault(VBButtonBit::LeftUp, EmuButton_Up);
    setDefault(VBButtonBit::LeftDown, EmuButton_Down);
    setDefault(VBButtonBit::LeftLeft, EmuButton_Left);
    setDefault(VBButtonBit::LeftRight, EmuButton_Right);
    setDefault(VBButtonBit::RightUp, EmuButton_RightStickUp);
    setDefault(VBButtonBit::RightDown, EmuButton_RightStickDown);
    setDefault(VBButtonBit::RightLeft, EmuButton_RightStickLeft);
    setDefault(VBButtonBit::RightRight, EmuButton_RightStickRight);
    setDefault(VBButtonBit::A, byPosition ? EmuButton_B : EmuButton_A);
    setDefault(VBButtonBit::B, byPosition ? EmuButton_A : EmuButton_B);
    setDefault(VBButtonBit::L, EmuButton_LShoulder);
    setDefault(VBButtonBit::R, EmuButton_RShoulder);
    setDefault(VBButtonBit::Start, EmuButton_Enter);
    setDefault(VBButtonBit::Select, EmuButton_Back);
}

uint32_t GamepadCompanionBits(uint32_t bits)
{
    auto also = [&bits](uint32_t from, uint32_t to)
    {
        if (bits & ButtonMapping[from])
            bits |= ButtonMapping[to];
    };
    also(EmuButton_LeftStickUp, EmuButton_Up);
    also(EmuButton_LeftStickDown, EmuButton_Down);
    also(EmuButton_LeftStickLeft, EmuButton_Left);
    also(EmuButton_LeftStickRight, EmuButton_Right);
    also(EmuButton_L2, EmuButton_LShoulder);
    also(EmuButton_R2, EmuButton_RShoulder);
    return bits;
}

void MigrateGamepadAToPosition(MappedButtons (&vbButtons)[16])
{
    MappedButton &a = vbButtons[VBButtonBit::A].Buttons[1], &b = vbButtons[VBButtonBit::B].Buttons[1];
    const bool oldDefault = a.IsSet && b.IsSet && a.InputDevice == DeviceGamepad && b.InputDevice == DeviceGamepad &&
                            a.ButtonIndex == static_cast<int>(EmuButton_A) && b.ButtonIndex == static_cast<int>(EmuButton_B);
    if (oldDefault)
    {
        a.ButtonIndex = static_cast<int>(EmuButton_B);
        b.ButtonIndex = static_cast<int>(EmuButton_A);
    }
}

} // namespace ButtonMapper
