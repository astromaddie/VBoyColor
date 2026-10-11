#pragma once
#include "input/ButtonMapping.h"
#include "menu/MenuPage.h"

#include <memory>
#include <string>
#include <vector>

class MenuList;
struct AppSettings;
class Platform;

// Per-button controller input mapping, laid out like the reference: one row
// per Virtual Boy button, showing its icon and two side-by-side binding
// columns (primary + secondary). Left/Right move the highlight between the
// two columns; pressing a column starts a "press any button now" capture
// (see Menu::CaptureHook) that binds that slot in AppSettings::vbButtons -
// consumed by ButtonMapper::TranslateToVBBitmask (either binding triggers
// the VB button).
class GameControls;

class EmulatorButtonMapPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    std::string Title() const override { return "Button mapping"; }
    std::string Subtitle() const override { return m_game.empty() ? "Settings" : m_gameTitle; }

    // Edit a game's own mapping (game: RomEntry::name; it starts from the
    // general one until it has its own; back: where B goes), or "" the
    // general one - call before navigating here.
    void OpenFor(const std::string &game, const std::string &title, MenuPage *back);
    std::vector<MenuHint> Hints() const override
    {
        return {{UiIconId::ButtonA, "Change"}, {UiIconId::ButtonB, "Back"}};
    }

private:
    // buttonIndex: row into kButtons; column: 0 = primary, 1 = secondary.
    void StartCapture(int buttonIndex, int column);
    void ResetMapping();
    void ApplyDefaultMapping(bool overwrite);
    void RefreshLabels(bool save = true);
    // The mapping being edited: the general one or m_gameButtons.
    using Mapping = ButtonMapper::MappedButtons[16];
    Mapping &Target();

    std::shared_ptr<MenuList> m_list; // kept for GetActiveColumn() at press time
    std::vector<std::shared_ptr<MenuList::Entry>> m_rowEntries; // one per button
    AppSettings *m_settings = nullptr;
    Platform *m_platform = nullptr;
    ButtonMappingProfile m_mappingProfile = ButtonMappingProfile::Vr;
    GameControls *m_gameControls = nullptr;
    std::string m_game, m_gameTitle; // the game whose own mapping this is ("" the general one)
    MenuPage *m_gameBack = nullptr;
    ButtonMapper::MappedButtons m_gameButtons[16] = {};
};
