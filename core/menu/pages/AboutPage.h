#pragma once
#include "menu/MenuPage.h"

#include <memory>

class Updater;
struct AppSettings;
class Platform;

// About VBoy Color: updates on top - Check for updates (Update to ... when a
// newer one is out; Y opens its page, What's new) and the switch for
// checking when the app starts - then what it is, its license and emulation
// core, that it isn't affiliated with Nintendo or Meta, and thanks.
class AboutPage : public MenuPage
{
public:
    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    std::string Title() const override { return "About"; }
    std::string Subtitle() const override { return kVersion; }
    std::vector<MenuHint> Hints() const override;
    void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds) override;

private:
    void Refresh();

    std::string kVersion;
    Updater *m_updater = nullptr;
    AppSettings *m_settings = nullptr;
    Platform *m_platform = nullptr;
    std::shared_ptr<MenuList> m_list;
    std::shared_ptr<MenuList::Entry> m_updateEntry;
};
