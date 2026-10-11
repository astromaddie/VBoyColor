#pragma once
#include "menu/MenuPage.h"

#include <cstdint>
#include <memory>
#include <string>

class Achievements;
class Emulator;
class Platform;
class TextInputPage;

// RetroAchievements: logging in (user name and password, typed on
// TextInputPage) or who's logged in, whether unlocks show over the game -
// and the game's achievements, unlocked and locked, each with its badge,
// what it asks and its points.
class AchievementsPage : public MenuPage
{
public:
    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    std::string Title() const override { return "Achievements"; }
    std::string Subtitle() const override { return "RetroAchievements"; }
    void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds) override;
    void OnShow() override { m_rebuild = true; }

    TextInputPage *textInput = nullptr; // (set by AppMenu)

private:
    void Rebuild();

    UiRenderer *m_ui = nullptr;
    Achievements *m_achievements = nullptr;
    Platform *m_platform = nullptr;
    std::shared_ptr<MenuList> m_list;
    bool m_rebuild = true;
    uint64_t m_shownVersion = 0;
    int m_shownLogin = -1;
    // Typed, not yet logged in with (the password is forgotten once it's sent).
    std::string m_user, m_password;
};
