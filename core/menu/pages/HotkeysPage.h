#pragma once
#include "input/Hotkeys.h"
#include "menu/MenuPage.h"

#include <memory>
#include <vector>

struct AppSettings;
class Platform;

// Settings > Hotkeys (the window app): a key - alone or with Ctrl / Alt /
// Shift - for each of the app's actions (input/Hotkeys.h), and the color
// pack and video tools' keys (F4 to F12, docs/COLOR_PACKS.md) on or off. A
// on a row waits for the keys to press (Esc: never mind); X clears it. A key
// another action had moves to this one.
class HotkeysPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    void OnShow() override;
    std::string Title() const override { return "Hotkeys"; }
    std::string Subtitle() const override { return "Settings"; }
    std::vector<MenuHint> Hints() const override;

    // A key pressed (GLFW's code, kHotkey* modifiers) - see AppMenu::SubmitRawKey.
    void SubmitKey(int key, int mods);

private:
    void StartCapture(int action);
    void StopCapture();
    void Clear(int action);
    void ResetAll();
    void RefreshLabels(bool save = true);

    std::shared_ptr<MenuList> m_list;
    std::vector<std::shared_ptr<MenuList::Entry>> m_rows; // one per HotkeyAction
    std::vector<int> m_rowIndex;                          // their indices in the list
    // What each of F4 to F12 does - shown while the tools are on.
    std::vector<std::shared_ptr<MenuList::Entry>> m_toolRows;
    AppSettings *m_settings = nullptr;
    Platform *m_platform = nullptr;
    int m_capturing = -1;          // the action waiting for its keys
    bool m_waitForRelease = false; // (the press that started it let go first)
};
