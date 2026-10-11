// Gamepads on PC (DesktopGamepad.h), from two places at once:
//
// - GLFW's joystick API: every connected pad GLFW knows the layout of (Xbox
//   pads, DualShock/DualSense, Switch Pro, 8BitDo...), on any slot - not only
//   the first one, which a PC VR setup or a wheel/stick often takes.
// - On Windows also XInput itself, read directly: GLFW notices a pad plugged
//   in after the app started only when Windows announces a new HID device,
//   and Xbox pads (USB or the wireless adapter) often aren't HID devices - so
//   they went unseen until the app restarted. XInput sees them at once.
//
// A pad both report counts once (its buttons are OR'd together).
#include "desktop/DesktopGamepad.h"
#include "input/ButtonMapping.h"

#define GLFW_INCLUDE_NONE // (no OpenGL headers - only the joystick API)
#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>

#include <chrono>
#include <initializer_list>
#endif

namespace
{
    using namespace ButtonMapper;
    constexpr float kDeadzone = 0.5f;

    void FromGlfw(DesktopGamepad &out)
    {
        for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
        {
            GLFWgamepadstate pad{};
            if (!glfwJoystickIsGamepad(jid) || !glfwGetGamepadState(jid, &pad))
                continue;
            auto button = [&](int glfwButton, int emuButton)
            {
                if (pad.buttons[glfwButton] == GLFW_PRESS)
                    out.bits |= ButtonMapping[emuButton];
            };
            button(GLFW_GAMEPAD_BUTTON_A, EmuButton_A);
            button(GLFW_GAMEPAD_BUTTON_B, EmuButton_B);
            button(GLFW_GAMEPAD_BUTTON_X, EmuButton_X);
            button(GLFW_GAMEPAD_BUTTON_Y, EmuButton_Y);
            button(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, EmuButton_LShoulder);
            button(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, EmuButton_RShoulder);
            button(GLFW_GAMEPAD_BUTTON_BACK, EmuButton_Back);
            button(GLFW_GAMEPAD_BUTTON_START, EmuButton_Enter);
            button(GLFW_GAMEPAD_BUTTON_DPAD_UP, EmuButton_Up);
            button(GLFW_GAMEPAD_BUTTON_DPAD_DOWN, EmuButton_Down);
            button(GLFW_GAMEPAD_BUTTON_DPAD_LEFT, EmuButton_Left);
            button(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, EmuButton_Right);
            auto axis = [&](int glfwAxis, int negative, int positive)
            {
                if (pad.axes[glfwAxis] < -kDeadzone)
                    out.bits |= ButtonMapping[negative];
                if (pad.axes[glfwAxis] > kDeadzone)
                    out.bits |= ButtonMapping[positive];
            };
            axis(GLFW_GAMEPAD_AXIS_LEFT_X, EmuButton_LeftStickLeft, EmuButton_LeftStickRight);
            axis(GLFW_GAMEPAD_AXIS_LEFT_Y, EmuButton_LeftStickUp, EmuButton_LeftStickDown);
            axis(GLFW_GAMEPAD_AXIS_RIGHT_X, EmuButton_RightStickLeft, EmuButton_RightStickRight);
            axis(GLFW_GAMEPAD_AXIS_RIGHT_Y, EmuButton_RightStickUp, EmuButton_RightStickDown);
            if (pad.buttons[GLFW_GAMEPAD_BUTTON_GUIDE] == GLFW_PRESS ||
                (pad.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS && pad.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS))
                out.menu = true;
        }
    }

#ifdef _WIN32
    // XInput, loaded when first needed (no import library to link; the DLL
    // Windows 8+ has first). Ordinal 100 is XInputGetStateEx - the same, plus
    // the Guide button (0x0400).
    using GetStateFn = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);
    GetStateFn LoadXInput()
    {
        static bool tried = false;
        static GetStateFn getState = nullptr;
        if (tried)
            return getState;
        tried = true;
        for (const char *dll : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"})
        {
            HMODULE module = LoadLibraryA(dll);
            if (!module)
                continue;
            getState = reinterpret_cast<GetStateFn>(reinterpret_cast<void *>(GetProcAddress(module, MAKEINTRESOURCEA(100))));
            if (!getState)
                getState = reinterpret_cast<GetStateFn>(reinterpret_cast<void *>(GetProcAddress(module, "XInputGetState")));
            if (getState)
                break;
        }
        return getState;
    }

    void FromXInput(DesktopGamepad &out)
    {
        GetStateFn getState = LoadXInput();
        if (!getState)
            return;
        // Asking an empty slot is slow (it looks for the pad each time): those
        // are asked again only once a second.
        using Clock = std::chrono::steady_clock;
        static bool connected[XUSER_MAX_COUNT] = {};
        static Clock::time_point nextLook[XUSER_MAX_COUNT] = {};
        const Clock::time_point now = Clock::now();
        for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot)
        {
            if (!connected[slot] && now < nextLook[slot])
                continue;
            XINPUT_STATE state{};
            connected[slot] = getState(slot, &state) == ERROR_SUCCESS;
            if (!connected[slot])
            {
                nextLook[slot] = now + std::chrono::seconds(1);
                continue;
            }
            const XINPUT_GAMEPAD &pad = state.Gamepad;
            auto button = [&](WORD mask, int emuButton)
            {
                if (pad.wButtons & mask)
                    out.bits |= ButtonMapping[emuButton];
            };
            button(XINPUT_GAMEPAD_A, EmuButton_A);
            button(XINPUT_GAMEPAD_B, EmuButton_B);
            button(XINPUT_GAMEPAD_X, EmuButton_X);
            button(XINPUT_GAMEPAD_Y, EmuButton_Y);
            button(XINPUT_GAMEPAD_LEFT_SHOULDER, EmuButton_LShoulder);
            button(XINPUT_GAMEPAD_RIGHT_SHOULDER, EmuButton_RShoulder);
            button(XINPUT_GAMEPAD_BACK, EmuButton_Back);
            button(XINPUT_GAMEPAD_START, EmuButton_Enter);
            button(XINPUT_GAMEPAD_DPAD_UP, EmuButton_Up);
            button(XINPUT_GAMEPAD_DPAD_DOWN, EmuButton_Down);
            button(XINPUT_GAMEPAD_DPAD_LEFT, EmuButton_Left);
            button(XINPUT_GAMEPAD_DPAD_RIGHT, EmuButton_Right);
            // (XInput's sticks: up is positive - GLFW's is negative)
            constexpr int dead = static_cast<int>(32767 * kDeadzone);
            auto axis = [&](int value, int negative, int positive)
            {
                if (value < -dead)
                    out.bits |= ButtonMapping[negative];
                if (value > dead)
                    out.bits |= ButtonMapping[positive];
            };
            axis(pad.sThumbLX, EmuButton_LeftStickLeft, EmuButton_LeftStickRight);
            axis(pad.sThumbLY, EmuButton_LeftStickDown, EmuButton_LeftStickUp);
            axis(pad.sThumbRX, EmuButton_RightStickLeft, EmuButton_RightStickRight);
            axis(pad.sThumbRY, EmuButton_RightStickDown, EmuButton_RightStickUp);
            if ((pad.wButtons & 0x0400) ||
                ((pad.wButtons & XINPUT_GAMEPAD_BACK) && (pad.wButtons & XINPUT_GAMEPAD_START)))
                out.menu = true;
        }
    }
#endif
} // namespace

DesktopGamepad PollDesktopGamepads()
{
    DesktopGamepad out;
    FromGlfw(out);
#ifdef _WIN32
    FromXInput(out);
#endif
    return out;
}
