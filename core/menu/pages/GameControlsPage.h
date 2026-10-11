#pragma once
#include "menu/MenuPage.h"

#include <memory>
#include <string>

class EmulatorButtonMapPage;
class Platform;

// One game's own controls (Library: hold its card): its button mapping and,
// on a phone, where its touch controls are - each the general one
// (Settings) until changed here, and back to it with "Use the general ...".
// Kept with the settings: <game>.buttons (GameControls), <game>.touch
// (TouchControls).
class GameControlsPage : public MenuPage
{
public:
    MenuPage *libraryPage = nullptr;
    EmulatorButtonMapPage *buttonMapPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    std::string Title() const override { return "Controls for this game"; }
    std::string Subtitle() const override { return m_title; }
    void OnShow() override { Rebuild(); }
    void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds) override;

    // The game (RomEntry::name, and its title as shown) - call before navigating here.
    void Open(const std::string &game, const std::string &title);

private:
    void Rebuild();

    const UiMenuResources *m_res = nullptr;
    Platform *m_platform = nullptr;
    std::shared_ptr<MenuList> m_list;
    std::string m_game, m_title;
    bool m_rebuild = false;
};
