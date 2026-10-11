#pragma once

// Gamepads on PC, for both apps (the window, platform/pc2d, and the VR one,
// platform/pc - which has no window of its own). Needs glfwInit() but no
// window (DesktopGamepad.cpp).
#include <cstdint>

struct DesktopGamepad
{
    uint32_t bits = 0; // ButtonMapper::ButtonMapping[EmuButton_*] bits (DeviceGamepad)
    bool menu = false; // the Guide button, or Back + Start together
};

// Every connected pad, all at once (their buttons together). Once a frame.
DesktopGamepad PollDesktopGamepads();
