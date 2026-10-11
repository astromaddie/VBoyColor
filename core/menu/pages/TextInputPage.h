#pragma once
#include "menu/MenuPage.h"

#include <functional>
#include <memory>
#include <string>

// Typing a line of text without a keyboard - a user name, a password: the
// field on top and an on-screen keyboard under it, worked with the D-pad and
// A, a laser, a finger or the mouse; on PC a real keyboard types too (see
// AppMenu::TypeText). Done (or Enter) hands the text back and returns to the
// page that opened it; B returns without changing anything.
class TextInputPage : public MenuPage
{
public:
    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    std::string Title() const override { return m_title; }
    std::vector<MenuHint> Hints() const override;

    // Opens it for a field (then Navigate to it): its name, its text now,
    // hidden (a password: dots), and what to do with the text when done.
    void Open(const std::string &title, const std::string &text, bool hidden,
              std::function<void(const std::string &)> done);
    // From a real keyboard: a character typed (UTF-8), Backspace, Enter.
    void TypeText(const std::string &utf8);
    void TypeBackspace();
    void TypeEnter();

    // The page that opened it (B and Done go back there) - set by Open's caller.
    MenuPage *back = nullptr;

private:
    class Keyboard;
    friend class Keyboard;
    void Finish();

    std::string m_title;
    std::string m_text;
    bool m_hidden = false;
    std::function<void(const std::string &)> m_done;
    std::shared_ptr<Keyboard> m_keyboard;
};
