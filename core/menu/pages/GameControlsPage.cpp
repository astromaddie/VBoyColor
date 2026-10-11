#include "menu/pages/GameControlsPage.h"
#include "input/TouchControls.h"
#include "io/GameControls.h"
#include "io/Platform.h"
#include "menu/pages/AppMenuLayout.h"
#include "menu/pages/EmulatorButtonMapPage.h"

void GameControlsPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_res = &resources;
    m_platform = resources.platform;
    m_list = MakeList(ui, resources);
    m_menu.MenuItems.push_back(m_list);
    m_menu.Init();
    m_menu.BackPress = [this]() { if (libraryPage) Navigate(libraryPage, -1); };
}

void GameControlsPage::Open(const std::string &game, const std::string &title)
{
    m_game = game;
    m_title = title;
    m_rebuild = true;
}

void GameControlsPage::Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds)
{
    if (m_rebuild)
        Rebuild();
    MenuPage::Update(buttonState, lastButtonState, deltaSeconds);
}

void GameControlsPage::Rebuild()
{
    m_rebuild = false;
    if (!m_list || !m_platform)
        return;
    ButtonMapper::MappedButtons own[16];
    const bool ownButtons = GameControls::Load(*m_platform, m_game, own);
    m_list->Clear();

    m_list->AddEntry("Button mapping", [this](MenuItem *)
                     {
                         if (!buttonMapPage)
                             return;
                         buttonMapPage->OpenFor(m_game, m_title, this);
                         buttonMapPage->sidebarItem = sidebarItem; // (still the library's)
                         Navigate(buttonMapPage, 1);
                     }, nullptr, nullptr, UiIconId::Mapping)
        ->SetValue(ownButtons ? "Its own" : "General");
    if (ownButtons)
        m_list->AddEntry("Use the general mapping", [this](MenuItem *)
                         {
                             GameControls::Forget(*m_platform, m_game);
                             if (m_res->gameControls)
                                 m_res->gameControls->Reload(*m_platform);
                             m_rebuild = true;
                         }, nullptr, nullptr, UiIconId::ResetView);

    // The phone: where its on-screen controls are.
    if (m_res->editTouchControls)
    {
        const bool ownTouch = TouchControls::HasGameLayout(*m_platform, m_game);
        m_list->AddSpacer(kGroupGap);
        m_list->AddEntry("Touch controls", [this](MenuItem *) { m_res->editTouchControls(m_game); }, nullptr, nullptr,
                         UiIconId::Move)
            ->SetValue(ownTouch ? "Its own - Edit" : "General - Edit");
        if (ownTouch && m_res->forgetTouchControls)
            m_list->AddEntry("Use the general touch controls", [this](MenuItem *)
                             {
                                 m_res->forgetTouchControls(m_game);
                                 m_rebuild = true;
                             }, nullptr, nullptr, UiIconId::ResetView);
    }
    m_list->ResetSelection();
}
