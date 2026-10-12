// PC entry point: runs the same OpenXrApp used on Android, streamed to the
// headset via whatever OpenXR runtime is currently registered (Virtual
// Desktop, SteamVR, Oculus Link, ...). No desktop window - see the
// Rework-branch session notes for why (VD/SteamVR both stream a runtime's
// compositor output directly; a mirror window is a later nice-to-have).
#include "app/OpenXrApp.h"
#include "desktop/DesktopGamepad.h"
#include "desktop/DesktopPlatform.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
    // Unbuffered stdout/stderr so logs show up immediately even when
    // redirected to a file (buffered-until-exit output made earlier
    // debugging in this session confusing).
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    // Ignore stdin EOF (e.g. when launched from a script/IDE with redirected
    // stdin) instead of treating it as an immediate quit request - learned
    // the hard way debugging hello_xr earlier this session.
    static bool quitRequested = false;
    std::thread exitPollingThread([]
                                  {
        std::printf("Press Enter to quit...\n");
        int ch = std::getchar();
        if (ch != EOF) {
            quitRequested = true;
        } });
    exitPollingThread.detach();

    std::printf("VBoy Color VR starting...\n");

    // Gamepads (an Xbox pad and the like) - GLFW's joystick API, no window:
    // what the Quest app gets from Android's input events.
    const bool gamepads = glfwInit() == GLFW_TRUE;
    if (!gamepads)
        std::fprintf(stderr, "VBoy Color VR: no gamepad support (glfwInit failed)\n");

    // Its own settings file: the flat VBoyColor.exe uses the same States
    // folder, with keys where this app has Touch controller inputs.
    DesktopPlatform platform("settings-vr.dat");
    if (gamepads) // (and the text fields' Copy and Paste - see DesktopPlatform::getClipboard)
    {
        platform.getClipboard = []
        {
            const char *text = glfwGetClipboardString(nullptr);
            return std::string(text ? text : "");
        };
        platform.setClipboard = [](const std::string &text) { glfwSetClipboardString(nullptr, text.c_str()); };
    }
    OpenXrApp app;
    try
    {
        OpenXrApp::InitInfo info;
        info.platform = &platform;
        app.Initialize(info);
        std::printf("VBoy Color VR initialized OK\n");
    }
    catch (const std::exception &ex)
    {
        std::fprintf(stderr, "VBoy Color VR: init failed: %s\n", ex.what());
        return 1;
    }

    bool requestRestart = false;
    int exitCode = 0;
    while (!quitRequested && !platform.RestartRequested())
    {
        try
        {
            bool exitRenderLoop = false;
            app.PollEvents(exitRenderLoop, requestRestart);
            if (exitRenderLoop)
            {
                std::printf("VBoy Color VR: exitRenderLoop requested\n");
                break;
            }

            if (app.IsSessionRunning())
            {
                if (gamepads)
                {
                    glfwPollEvents(); // (pads plugged in or out)
                    const DesktopGamepad pad = PollDesktopGamepads();
                    app.SetGamepadButtonState(pad.bits);
                    app.SetGamepadMenuButtonPressed(pad.menu);
                }
                app.RenderFrame();
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        catch (const std::exception &ex)
        {
            std::fprintf(stderr, "VBoy Color VR: render loop failed: %s\n", ex.what());
            exitCode = 1;
            break;
        }
    }

    app.Shutdown();
    if (gamepads)
        glfwTerminate();
    std::printf("VBoy Color VR exiting\n");
    if (platform.RestartRequested())
        platform.Relaunch(); // (an update - see Updater)
    return exitCode;
}
