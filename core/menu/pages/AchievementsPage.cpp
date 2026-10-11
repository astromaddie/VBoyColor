#include "menu/pages/AchievementsPage.h"
#include "achievements/Achievements.h"
#include "emu/Emulator.h"
#include "io/Platform.h"
#include "menu/pages/AppMenuLayout.h"
#include "menu/pages/TextInputPage.h"

#include <string>
#include <vector>

namespace
{
    constexpr float kAchievementRowHeight = 32.0f;
    constexpr const char *kCreateAccountUrl = "https://retroachievements.org/createaccount.php";

    std::string Dots(size_t count)
    {
        std::string dots;
        for (size_t i = 0; i < count && i < 24; ++i)
            dots += "\xE2\x80\xA2";
        return dots;
    }
} // namespace

void AchievementsPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_ui = &ui;
    m_achievements = resources.emulator ? &resources.emulator->GetAchievements() : nullptr;
    m_platform = resources.platform;
    m_list = MakeList(ui, resources);
    m_list->SetSubtitleFont(resources.captionFont);
    ui.EnsureGlyphsForText(resources.bodyFont, "\xE2\x80\xA2");
    m_menu.MenuItems.push_back(m_list);
    m_menu.Init();
    Rebuild();
}

void AchievementsPage::Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds)
{
    MenuPage::Update(buttonState, lastButtonState, deltaSeconds);
    if (!m_achievements)
        return;
    // (rebuilt here, never from inside one of the rows' own callbacks)
    if (m_rebuild || m_achievements->Version() != m_shownVersion ||
        static_cast<int>(m_achievements->GetLogin()) != m_shownLogin)
        Rebuild();
}

void AchievementsPage::Rebuild()
{
    m_rebuild = false;
    if (!m_achievements || !m_list)
        return;
    m_shownVersion = m_achievements->Version();
    const Achievements::Login login = m_achievements->GetLogin();
    // (a login or logout changes the rows: the highlight goes back to the top -
    // never onto "Log out" right after logging in)
    const int selected = m_shownLogin == static_cast<int>(login) ? m_list->SelectedIndex() : 0;
    m_shownLogin = static_cast<int>(login);
    m_list->Clear();
    Achievements &ra = *m_achievements;

    // ---- The account ----
    if (login == Achievements::Login::LoggedIn)
    {
        m_list->AddHeader("Account");
        auto user = m_list->AddEntry(ra.UserName(), nullptr, nullptr, nullptr, UiIconId::Trophy);
        user->SetValue(std::to_string(ra.Score()) + " points");
        auto notices = m_list->AddEntry("Show unlocks over the game", [this](MenuItem *)
                                        { m_achievements->SetShowNotices(!m_achievements->ShowNotices()); });
        notices->toggle = [this]() { return m_achievements->ShowNotices(); };
        notices->reserveIconSpace = true;
        auto logout = m_list->AddEntry("Log out", [this](MenuItem *)
                                       {
                                           m_achievements->LogOut();
                                           m_rebuild = true;
                                       });
        logout->reserveIconSpace = true;
    }
    else
    {
        if (m_user.empty())
            m_user = ra.UserName(); // (the last one that logged in)
        m_list->AddHeader("Log in to RetroAchievements");
        auto user = m_list->AddEntry("User name", [this](MenuItem *)
                                     {
                                         if (!textInput)
                                             return;
                                         textInput->back = this;
                                         textInput->Open("User name", m_user, false, [this](const std::string &text)
                                                         {
                                                             m_user = text;
                                                             m_rebuild = true;
                                                         });
                                         Navigate(textInput, 1);
                                     });
        user->SetValue(m_user.empty() ? "Enter" : m_user);
        user->opensPage = true;
        auto password = m_list->AddEntry("Password", [this](MenuItem *)
                                         {
                                             if (!textInput)
                                                 return;
                                             textInput->back = this;
                                             textInput->Open("Password", m_password, true, [this](const std::string &text)
                                                             {
                                                                 m_password = text;
                                                                 m_rebuild = true;
                                                             });
                                             Navigate(textInput, 1);
                                         });
        password->SetValue(m_password.empty() ? "Enter" : Dots(m_password.size()));
        password->opensPage = true;
        const bool busy = login == Achievements::Login::LoggingIn;
        m_list->AddSpacer(0.0f);
        auto go = m_list->AddEntry(busy ? "Logging in..." : "Log in", [this](MenuItem *)
                                   {
                                       if (m_user.empty() || m_password.empty() ||
                                           m_achievements->GetLogin() != Achievements::Login::LoggedOut)
                                           return;
                                       m_achievements->LogIn(m_user, m_password);
                                       m_password.clear(); // (kept nowhere - the server's token is)
                                       m_rebuild = true;
                                   });
        go->centered = true;
        const std::string error = ra.LoginError();
        if (!busy && !error.empty())
        {
            auto why = m_list->AddEntry(error);
            why->reserveIconSpace = true;
            why->dim = true;
        }
        m_list->AddHeader("New to RetroAchievements?");
        auto create = m_list->AddEntry("Make an account at retroachievements.org", [this](MenuItem *)
                                       {
                                           if (m_platform)
                                               m_platform->OpenUrl(kCreateAccountUrl);
                                       });
        create->opensPage = true;
    }

    // ---- The game ----
    auto note = [&](const std::string &header, const std::string &text, const std::string &subtitle = "")
    {
        m_list->AddHeader(header);
        auto row = m_list->AddEntry(text);
        row->reserveIconSpace = true;
        if (!subtitle.empty())
        {
            row->rowHeight = 26.0f;
            row->SetSubtitle(subtitle);
        }
    };
    switch (ra.GetGame())
    {
    case Achievements::Game::None:
        if (login == Achievements::Login::LoggedIn)
            note("This game", "Play a game to see its achievements here");
        else
            note("This game", "Log in to earn achievements as you play",
                 "Softcore: save states and the rest stay on - unlocks count as softcore");
        break;
    case Achievements::Game::Loading:
        note("This game", "Loading the achievements...");
        break;
    case Achievements::Game::Unknown:
        note("This game", "RetroAchievements doesn't know this ROM",
             "Achievements need the version of the game the site lists (a No-Intro ROM)");
        break;
    case Achievements::Game::Failed:
    {
        m_list->AddHeader("This game");
        auto row = m_list->AddEntry("Couldn't load the achievements - try again", [this](MenuItem *)
                                    {
                                        m_achievements->RetryGame();
                                        m_rebuild = true;
                                    });
        row->reserveIconSpace = true;
        if (!ra.GameError().empty())
        {
            row->rowHeight = 26.0f;
            row->SetSubtitle(ra.GameError());
        }
        break;
    }
    case Achievements::Game::Movie:
        note("This game", "No achievements while a recorded run plays");
        break;
    case Achievements::Game::Loaded:
    {
        uint32_t unlocked = 0, total = 0, points = 0, totalPoints = 0;
        const std::vector<Achievements::Item> items = ra.Items(unlocked, total, points, totalPoints);
        m_list->AddHeader(ra.GameTitle() + "  \xC2\xB7  " + std::to_string(unlocked) + " of " + std::to_string(total) +
                          " unlocked, " + std::to_string(points) + " of " + std::to_string(totalPoints) + " points");
        if (items.empty())
        {
            auto row = m_list->AddEntry("This game has no achievements yet");
            row->reserveIconSpace = true;
        }
        std::string group;
        for (const Achievements::Item &item : items)
        {
            if (item.group != group)
            {
                group = item.group;
                m_list->AddHeader(group);
            }
            auto row = m_list->AddEntry(item.title);
            row->rowHeight = kAchievementRowHeight;
            row->SetSubtitle(item.progress.empty() || item.unlocked ? item.description
                                                                     : item.description + "  (" + item.progress + ")");
            row->SetValue(std::to_string(item.points));
            row->dim = !item.unlocked;
            const int badge = item.badge;
            Achievements *achievements = m_achievements;
            row->leftDraw = [achievements, badge](UiRenderer &ui, float x, float y, float size, float alpha)
            {
                float u0, v0, u1, v1;
                if (achievements->BadgeUv(badge, u0, v0, u1, v1))
                    ui.DrawImageRegion(achievements->BadgeAtlas(), x, y, size, size, u0, v0, u1, v1, alpha);
                else
                    ui.DrawQuadRounded(x, y, size, size, {0.2f, 0.215f, 0.28f, alpha}, 3.0f);
            };
        }
        break;
    }
    }
    m_list->SelectNear(selected);
}
