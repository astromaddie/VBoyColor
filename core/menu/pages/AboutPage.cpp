#include "menu/pages/AboutPage.h"
#include "io/Platform.h"
#include "io/Settings.h"
#include "io/Updater.h"
#include "menu/pages/AppMenuLayout.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
    // Lines of text on rounded cards, top to bottom.
    class TextCards : public MenuItem
    {
    public:
        struct Line
        {
            std::string text;
            UiFontHandle font;
            XrColor4f color;
        };
        // An empty line ends a card (the next starts after a gap).
        std::vector<Line> lines;

        float top = kContentTop;

        void Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha) override
        {
            constexpr float kLineHeight = 12.5f, kPad = 6.0f;
            float y = top + offsetY;
            size_t i = 0;
            while (i < lines.size())
            {
                size_t end = i;
                while (end < lines.size() && !lines[end].text.empty())
                    ++end;
                const float height = (end - i) * kLineHeight + kPad * 2.0f;
                XrColor4f card = kMenuCardColor;
                card.a *= alpha;
                ui.DrawQuadRounded(kContentX + offsetX, y, kContentWidth, height, card, kCardRadius);
                float lineY = y + kPad;
                for (; i < end; ++i)
                {
                    const Line &line = lines[i];
                    XrColor4f c = line.color;
                    c.a *= alpha;
                    const float textY = lineY + kLineHeight / 2.0f - ui.GetFontPHeight(line.font) / 2.0f -
                                        ui.GetFontPStart(line.font);
                    ui.DrawText(line.font, line.text, kContentX + 10.0f + offsetX, textY, 1.0f, c);
                    lineY += kLineHeight;
                }
                y += height + kGroupGap;
                ++i; // (the empty line)
            }
        }
    };
} // namespace

void AboutPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    kVersion = std::string("VBoy Color ") + kVersionString;
    m_updater = resources.updater;
    m_settings = resources.settings;
    m_platform = resources.platform;
    const UiFontHandle body = resources.bodyFont, bold = resources.bodyBoldFont, small = resources.captionFont;

    // Updates (where the app can update itself).
    float cardsTop = kContentTop;
    if (m_updater && m_updater->Supported())
    {
        m_list = MakeList(ui, resources);
        m_list->AddHeader("Updates");
        m_updateEntry = m_list->AddEntry("Check for updates", [this](MenuItem *)
                                         {
                                             if (!m_updater)
                                                 return;
                                             const Updater::State state = m_updater->GetState();
                                             if (state == Updater::State::Available ||
                                                 (state == Updater::State::Failed && !m_updater->LatestName().empty()))
                                                 m_updater->StartUpdate();
                                             else
                                                 m_updater->Check();
                                         },
                                         nullptr, nullptr, UiIconId::Load);
        auto automatic = m_list->AddEntry("Check when VBoy Color starts", [this](MenuItem *)
                                          {
                                              if (!m_settings)
                                                  return;
                                              m_settings->checkForUpdates = !m_settings->checkForUpdates;
                                              m_settings->Save(*m_platform);
                                          },
                                          nullptr, nullptr, UiIconId::Reset);
        automatic->toggle = [this]() { return m_settings && m_settings->checkForUpdates; };
        m_menu.MenuItems.push_back(m_list);
        cardsTop = kContentTop + kGroupHeaderHeight + 2.0f * kRowHeight + kGroupGap;
    }

    auto cards = std::make_shared<TextCards>();
    cards->top = cardsTop;
    cards->lines = {
        {"A free, open-source Virtual Boy emulator", bold, kMenuTextColor},
        {"for Meta Quest, PC and Android - GPL-3.0", body, kMenuTextColor},
        {"Emulation: Beetle VB (Mednafen, libretro)", body, kMenuTextColor},
        {"Not affiliated with Nintendo or Meta in any way.", small, kMenuDimTextColor},
        {"github.com/yuk27/VBoyColor", small, kMenuDimTextColor},
        {"", body, kMenuTextColor},
        {"Thanks", bold, kMenuSelectionColor},
        {"VirtualBoyGo by CidVonHighwind - where the inspiration started", body, kMenuTextColor},
        {"Red Viper - whose colors inspired our gradients", body, kMenuTextColor},
    };
    for (const auto &line : cards->lines)
        ui.EnsureGlyphsForText(line.font, line.text);
    ui.EnsureGlyphsForText(resources.cardFont, kVersion);
    m_menu.MenuItems.push_back(cards);
    // Y: the newest release's page.
    m_menu.YPress = [this]()
    {
        if (m_updater && m_platform && !m_updater->LatestPage().empty())
            m_platform->OpenUrl(m_updater->LatestPage());
    };
    m_menu.Init();
    Refresh();
}

std::vector<MenuHint> AboutPage::Hints() const
{
    std::vector<MenuHint> hints = {{UiIconId::ButtonA, "Select"}, {UiIconId::ButtonB, "Back"}};
    if (m_updater && !m_updater->LatestPage().empty())
        hints.push_back({UiIconId::ButtonY, "What's new"});
    return hints;
}

void AboutPage::Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds)
{
    MenuPage::Update(buttonState, lastButtonState, deltaSeconds);
    Refresh();
}

void AboutPage::Refresh()
{
    if (!m_updateEntry || !m_updater)
        return;
    std::string text = "Check for updates", value;
    const std::string &status = m_updater->Status();
    switch (m_updater->GetState())
    {
    case Updater::State::Idle:
        break;
    case Updater::State::Checking:
        value = "Checking...";
        break;
    case Updater::State::UpToDate:
        value = "Up to date";
        break;
    case Updater::State::Available:
        text = "Update to " + m_updater->LatestName();
        value = m_updater->IsDevBuild() ? "Install" : "Update"; // (a development build: the newest release instead)
        break;
    case Updater::State::Downloading:
        text = "Update to " + m_updater->LatestName();
        value = "Downloading...";
        break;
    case Updater::State::Installed:
        text = status;
        break;
    case Updater::State::Failed:
        text = status.empty() ? "Couldn't update" : status;
        value = "Try again";
        break;
    }
    if (m_updateEntry->text != text)
        m_updateEntry->SetText(text);
    if (m_updateEntry->value != value)
        m_updateEntry->SetValue(value);
}
