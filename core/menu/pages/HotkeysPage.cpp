#include "menu/pages/HotkeysPage.h"
#include "io/Platform.h"
#include "io/Settings.h"
#include "menu/pages/AppMenuLayout.h"

namespace
{
    constexpr int kKeyEscape = 256; // (GLFW's)

    UiIconId ActionIcon(HotkeyAction action)
    {
        switch (action)
        {
        case HotkeyAction::Menu: return UiIconId::Settings;
        case HotkeyAction::Fullscreen: return UiIconId::Scale;
        case HotkeyAction::Screenshot: return UiIconId::FlatScreen;
        case HotkeyAction::SaveState: return UiIconId::Save;
        case HotkeyAction::LoadState: return UiIconId::Load;
        case HotkeyAction::PreviousSlot:
        case HotkeyAction::NextSlot: return UiIconId::SaveSlot;
        case HotkeyAction::Reset: return UiIconId::Reset;
        case HotkeyAction::CloseGame: return UiIconId::Back;
        case HotkeyAction::Quit: return UiIconId::Exit;
        }
        return UiIconId::None;
    }
} // namespace

void HotkeysPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_settings = resources.settings;
    m_platform = resources.platform;

    auto list = MakeList(ui, resources);
    int index = 0;
    list->AddHeader("Keys");
    ++index;
    for (int i = 0; i < kHotkeyActionCount; ++i)
    {
        auto row = list->AddEntry(HotkeyActionName(static_cast<HotkeyAction>(i)), [this, i](MenuItem *) { StartCapture(i); },
                                  nullptr, nullptr, ActionIcon(static_cast<HotkeyAction>(i)));
        m_rows.push_back(row);
        m_rowIndex.push_back(index++);
    }

    list->AddHeader("F4 to F12");
    // The color pack tools and video recording on F4 to F12
    // (docs/COLOR_PACKS.md) - off, those keys are the hotkeys'.
    auto tools = list->AddEntry("Color pack and video tools", [this](MenuItem *)
                                {
                                    if (!m_settings)
                                        return;
                                    m_settings->paintingTools = !m_settings->paintingTools;
                                    RefreshLabels();
                                },
                                nullptr, nullptr, UiIconId::Palette);
    tools->toggle = [this]() { return m_settings && m_settings->paintingTools; };
    // ... and while they're on, which key does what (as platform/pc2d/Main.cpp has them).
    struct ToolKey
    {
        const char *name, *keys;
        UiIconId icon;
    };
    static constexpr ToolKey kToolKeys[] = {
        {"Touch controls preview (Shift: edit)", "F4", UiIconId::Move},
        {"Left eye, both eyes, right eye", "F5", UiIconId::Eye},
        {"Save a tile sheet", "F6", UiIconId::Save},
        {"Collect what's still uncolored", "F7", UiIconId::Palette},
        {"Color pack on / off", "F8", UiIconId::EyeOff},
        {"Tile debug view", "F9", UiIconId::Mapping},
        {"Capture to paint (Shift: right eye)", "F10", UiIconId::FlatScreen},
        {"Re-import the paintings", "F11", UiIconId::Reset},
        {"Record a video", "F12", UiIconId::Resume},
    };
    for (const ToolKey &tool : kToolKeys)
    {
        auto row = list->AddEntry(tool.name, nullptr, nullptr, nullptr, tool.icon);
        row->SetValue(tool.keys);
        m_toolRows.push_back(row);
    }

    list->AddSpacer(kGroupGap);
    auto reset = list->AddEntry("Reset hotkeys", [this](MenuItem *) { ResetAll(); }, nullptr, nullptr, UiIconId::ResetView);
    reset->centered = true;
    reset->tintIconOnSelect = true;

    m_menu.MenuItems.push_back(list);
    m_list = list;
    m_menu.BackPress = [this]()
    {
        if (m_capturing >= 0)
        {
            StopCapture();
            return;
        }
        if (settingsPage)
            Navigate(settingsPage, -1);
    };
    m_menu.XPress = [this]()
    {
        if (m_capturing >= 0 || !m_list)
            return;
        for (size_t i = 0; i < m_rowIndex.size(); ++i)
            if (m_rowIndex[i] == m_list->SelectedIndex())
                Clear(static_cast<int>(i));
    };
    m_menu.Init();
    RefreshLabels(false);
}

void HotkeysPage::OnShow()
{
    StopCapture();
    RefreshLabels(false);
}

std::vector<MenuHint> HotkeysPage::Hints() const
{
    if (m_capturing >= 0)
        return {{UiIconId::ButtonB, "Never mind (Esc)"}};
    return {{UiIconId::ButtonA, "Change"}, {UiIconId::ButtonX, "Clear"}, {UiIconId::ButtonB, "Back"}};
}

void HotkeysPage::StartCapture(int action)
{
    if (!m_settings || action < 0 || action >= kHotkeyActionCount)
        return;
    if (m_capturing >= 0 && m_capturing != action)
        RefreshLabels(false); // (another row was waiting: its keys back)
    m_capturing = action;
    m_waitForRelease = true;
    m_rows[action]->SetValue("Press the keys...");
    // Menu navigation waits meanwhile; the keys come in through SubmitKey.
    // B on a gamepad gives up too (Esc does, as a key).
    SetCaptureHook([this](uint32_t *buttonState, uint32_t *lastButtonState) -> bool
    {
        if (m_waitForRelease)
        {
            if (!buttonState[0] && !buttonState[1] && !buttonState[2])
                m_waitForRelease = false;
            return true;
        }
        const uint32_t b = ButtonMapper::ButtonMapping[ButtonMapper::EmuButton_B];
        if ((buttonState[ButtonMapper::DeviceGamepad] & b) && !(lastButtonState[ButtonMapper::DeviceGamepad] & b))
            StopCapture();
        return true;
    });
}

void HotkeysPage::StopCapture()
{
    if (m_capturing < 0)
        return;
    m_capturing = -1;
    SetCaptureHook(nullptr);
    RefreshLabels(false);
}

void HotkeysPage::SubmitKey(int key, int mods)
{
    if (m_capturing < 0 || m_waitForRelease || !m_settings)
        return;
    if (IsModifierKey(key))
        return; // (held with the key to come)
    if (key == kKeyEscape)
    {
        StopCapture();
        return;
    }
    const Hotkey hotkey{key, mods};
    // (a key another action had is this one's now)
    for (int i = 0; i < kHotkeyActionCount; ++i)
        if (i != m_capturing && m_settings->hotkeys[i].key == key && m_settings->hotkeys[i].mods == mods)
            m_settings->hotkeys[i] = {};
    m_settings->hotkeys[m_capturing] = hotkey;
    m_capturing = -1;
    SetCaptureHook(nullptr);
    RefreshLabels();
}

void HotkeysPage::Clear(int action)
{
    if (!m_settings || action < 0 || action >= kHotkeyActionCount)
        return;
    m_settings->hotkeys[action] = {};
    RefreshLabels();
}

void HotkeysPage::ResetAll()
{
    if (!m_settings)
        return;
    StopCapture();
    for (int i = 0; i < kHotkeyActionCount; ++i)
        m_settings->hotkeys[i] = DefaultHotkey(static_cast<HotkeyAction>(i));
    RefreshLabels();
}

void HotkeysPage::RefreshLabels(bool save)
{
    if (!m_settings)
        return;
    for (int i = 0; i < kHotkeyActionCount; ++i)
        if (i != m_capturing)
            m_rows[i]->SetValue(HotkeyName(m_settings->hotkeys[i]));
    for (const auto &row : m_toolRows)
        row->Visible = m_settings->paintingTools;
    if (save && m_platform)
        m_settings->Save(*m_platform);
}
