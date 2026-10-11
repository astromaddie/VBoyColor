#include "menu/pages/TextInputPage.h"
#include "menu/pages/AppMenuLayout.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    XrColor4f WithAlpha(XrColor4f c, float alpha)
    {
        c.a *= alpha;
        return c;
    }

    float TextY(UiRenderer &ui, UiFontHandle font, float centerY)
    {
        return centerY - ui.GetFontPHeight(font) / 2.0f - ui.GetFontPStart(font);
    }

    // The keys. Letters and digits in rows of ten; the last row's special
    // keys are wider (in tenths of the row).
    enum Special
    {
        KeyChar,
        KeyShift,
        KeySymbols,
        KeySpace,
        KeyDelete,
        KeyDone
    };
    const char *const kLower[] = {"1234567890", "qwertyuiop", "asdfghjkl@", "zxcvbnm-_."};
    const char *const kUpper[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL@", "ZXCVBNM-_."};
    const char *const kSymbols[] = {"!#$%&*()+=", "[]{}<>/\\|~", ";:'\",?^`", ""};
    constexpr int kSpecialCount = 5;
    constexpr Special kBottom[kSpecialCount] = {KeyShift, KeySymbols, KeySpace, KeyDelete, KeyDone};
    constexpr float kBottomWidth[kSpecialCount] = {1.6f, 1.6f, 3.6f, 1.6f, 1.6f}; // (tenths of the row: 10)

    // The field's place and the keyboard's, in the menu's logical units.
    constexpr float kFieldY = kContentTop + 2.0f, kFieldH = 18.0f;
    constexpr float kKeysY = kFieldY + kFieldH + 8.0f, kKeyH = 22.0f, kKeyGap = 3.0f;
} // namespace

class TextInputPage::Keyboard : public MenuItem
{
public:
    Keyboard(TextInputPage &page, const UiMenuResources &resources) : m_page(page), m_res(resources)
    {
        Selectable = true;
        ScrollTimeH = ScrollTimeV = 0.11f;
    }

    struct Key
    {
        Special kind = KeyChar;
        std::string text; // a character key's
        float x = 0, y = 0, w = 0, h = kKeyH;
    };

    // The keys as laid out now (shift and symbols change the letters).
    std::vector<std::vector<Key>> Rows() const
    {
        std::vector<std::vector<Key>> rows;
        const char *const *letters = m_symbols ? kSymbols : (m_shift ? kUpper : kLower);
        const float unit = (kContentWidth - 9.0f * kKeyGap) / 10.0f;
        for (int r = 0; r < 4; ++r)
        {
            std::vector<Key> row;
            const std::string chars = letters[r];
            if (chars.empty())
                continue;
            // A shorter row is centered.
            const float rowW = chars.size() * unit + (chars.size() - 1) * kKeyGap;
            float x = kContentX + (kContentWidth - rowW) / 2.0f;
            for (char c : chars)
            {
                Key key;
                key.text = std::string(1, c);
                key.x = x;
                key.y = kKeysY + static_cast<float>(rows.size()) * (kKeyH + kKeyGap);
                key.w = unit;
                row.push_back(key);
                x += unit + kKeyGap;
            }
            rows.push_back(row);
        }
        std::vector<Key> bottom;
        float x = kContentX;
        const float total = kContentWidth - (kSpecialCount - 1) * kKeyGap;
        for (int i = 0; i < kSpecialCount; ++i)
        {
            Key key;
            key.kind = kBottom[i];
            key.x = x;
            key.y = kKeysY + static_cast<float>(rows.size()) * (kKeyH + kKeyGap);
            key.w = total * kBottomWidth[i] / 10.0f;
            bottom.push_back(key);
            x += key.w + kKeyGap;
        }
        rows.push_back(bottom);
        return rows;
    }

    std::string Label(const Key &key) const
    {
        switch (key.kind)
        {
        case KeyShift:
            return m_shift ? "SHIFT" : "Shift";
        case KeySymbols:
            return m_symbols ? "abc" : "#+=";
        case KeySpace:
            return "Space";
        case KeyDelete:
            return "Delete";
        case KeyDone:
            return "Done";
        default:
            return key.text;
        }
    }

    void Press(const Key &key)
    {
        switch (key.kind)
        {
        case KeyShift:
            m_shift = !m_shift;
            m_symbols = false;
            break;
        case KeySymbols:
            m_symbols = !m_symbols;
            break;
        case KeySpace:
            m_page.TypeText(" ");
            break;
        case KeyDelete:
            m_page.TypeBackspace();
            break;
        case KeyDone:
            m_page.Finish();
            break;
        default:
            m_page.TypeText(key.text);
            break;
        }
        ClampCursor();
    }

    void ClampCursor()
    {
        const auto rows = Rows();
        m_row = std::clamp(m_row, 0, static_cast<int>(rows.size()) - 1);
        m_col = std::clamp(m_col, 0, static_cast<int>(rows[m_row].size()) - 1);
    }

    // Up/Down: to the key in the next row nearest the one now.
    void MoveRow(int dir)
    {
        const auto rows = Rows();
        const Key &now = rows[m_row][m_col];
        const float center = now.x + now.w / 2.0f;
        m_row = (m_row + dir + static_cast<int>(rows.size())) % static_cast<int>(rows.size());
        int best = 0;
        float bestDistance = 1e9f;
        for (int i = 0; i < static_cast<int>(rows[m_row].size()); ++i)
        {
            const Key &k = rows[m_row][i];
            const float d = std::abs(k.x + k.w / 2.0f - center);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = i;
            }
        }
        m_col = best;
    }

    int PressedUp() override
    {
        MoveRow(-1);
        return 1;
    }
    int PressedDown() override
    {
        MoveRow(1);
        return 1;
    }
    int PressedLeft() override
    {
        const auto rows = Rows();
        m_col = (m_col + static_cast<int>(rows[m_row].size()) - 1) % static_cast<int>(rows[m_row].size());
        return 1; // (wraps around - never out to the sidebar)
    }
    int PressedRight() override
    {
        const auto rows = Rows();
        m_col = (m_col + 1) % static_cast<int>(rows[m_row].size());
        return 1;
    }
    int PressedEnter() override
    {
        const auto rows = Rows();
        Press(rows[m_row][m_col]);
        return 1;
    }

    void ResetSelection() override
    {
        m_row = 1;
        m_col = 0;
        m_shift = m_symbols = false;
    }

    bool HandlePointer(const MenuPointer &pointer) override
    {
        const auto rows = Rows();
        for (int r = 0; r < static_cast<int>(rows.size()); ++r)
            for (int c = 0; c < static_cast<int>(rows[r].size()); ++c)
            {
                const Key &k = rows[r][c];
                if (pointer.x < k.x - kKeyGap / 2.0f || pointer.x >= k.x + k.w + kKeyGap / 2.0f ||
                    pointer.y < k.y - kKeyGap / 2.0f || pointer.y >= k.y + k.h + kKeyGap / 2.0f)
                    continue;
                if (pointer.moved || pointer.clicked)
                {
                    m_row = r;
                    m_col = c;
                }
                if (pointer.clicked)
                    Press(k);
                return true;
            }
        return false;
    }

    void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds) override
    {
        MenuItem::Update(buttonState, lastButtonState, deltaSeconds);
        m_blink += deltaSeconds;
    }

    void Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha) override
    {
        const UiFontHandle body = m_res.bodyFont, bold = m_res.bodyBoldFont;

        // The field: the text (or dots), the end of it in view, and a caret.
        const float fx = kContentX + offsetX, fy = kFieldY + offsetY;
        ui.DrawQuadRounded(fx, fy, kContentWidth, kFieldH, WithAlpha(kMenuCardColor, alpha), kCardRadius);
        ui.DrawQuadRounded(fx, fy + kFieldH - 1.2f, kContentWidth, 1.2f, WithAlpha(kMenuSelectionColor, alpha * 0.8f), 0.6f);
        std::string shown;
        if (m_page.m_hidden)
            for (size_t i = 0; i < CharCount(m_page.m_text); ++i)
                shown += "\xE2\x80\xA2";
        else
            shown = m_page.m_text;
        const float room = kContentWidth - 16.0f;
        while (!shown.empty() && ui.GetTextWidth(body, shown) > room)
        {
            size_t cut = 1;
            while (cut < shown.size() && (static_cast<unsigned char>(shown[cut]) & 0xC0) == 0x80)
                ++cut;
            shown.erase(0, cut);
        }
        const float textX = fx + 8.0f, centerY = fy + kFieldH / 2.0f;
        ui.DrawText(body, shown, textX, TextY(ui, body, centerY), 1.0f, WithAlpha(kMenuTextColor, alpha));
        if (std::fmod(m_blink, 1.0f) < 0.6f)
            ui.DrawQuad(textX + ui.GetTextWidth(body, shown) + 1.0f, centerY - 5.0f, 0.9f, 10.0f,
                        WithAlpha(kMenuSelectionColor, alpha));

        // The keys.
        const auto rows = Rows();
        for (int r = 0; r < static_cast<int>(rows.size()); ++r)
            for (int c = 0; c < static_cast<int>(rows[r].size()); ++c)
            {
                const Key &k = rows[r][c];
                const float x = k.x + offsetX, y = k.y + offsetY;
                const bool sel = Selected && r == m_row && c == m_col;
                const bool special = k.kind != KeyChar;
                const bool on = (k.kind == KeyShift && m_shift) || (k.kind == KeySymbols && m_symbols);
                XrColor4f fill = special ? XrColor4f{0.17f, 0.18f, 0.24f, 1.0f} : kMenuCardColor;
                if (k.kind == KeyDone)
                    fill = {0.32f, 0.26f, 0.12f, 1.0f};
                if (sel)
                {
                    ui.DrawQuadRounded(x - 1.2f, y - 1.2f, k.w + 2.4f, k.h + 2.4f, WithAlpha(kMenuSelectionColor, alpha), 5.0f);
                    ui.DrawQuadRounded(x, y, k.w, k.h, WithAlpha(fill, alpha), 4.0f);
                    ui.DrawQuadRounded(x, y, k.w, k.h, WithAlpha(kMenuSelectionFillColor, alpha), 4.0f);
                }
                else
                    ui.DrawQuadRounded(x, y, k.w, k.h, WithAlpha(fill, alpha), 4.0f);
                const std::string label = Label(k);
                const UiFontHandle font = sel ? bold : (special ? m_res.captionBoldFont : body);
                const XrColor4f color = sel || on || k.kind == KeyDone ? kMenuSelectionColor : kMenuTextColor;
                ui.DrawText(font, label, x + (k.w - ui.GetTextWidth(font, label)) / 2.0f, TextY(ui, font, y + k.h / 2.0f), 1.0f,
                            WithAlpha(color, alpha));
            }
    }

    static size_t CharCount(const std::string &utf8)
    {
        size_t n = 0;
        for (unsigned char c : utf8)
            if ((c & 0xC0) != 0x80)
                ++n;
        return n;
    }

    TextInputPage &m_page;
    const UiMenuResources &m_res;
    int m_row = 1, m_col = 0;
    bool m_shift = false, m_symbols = false;
    float m_blink = 0;
};

void TextInputPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    for (UiFontHandle font : {resources.bodyFont, resources.bodyBoldFont, resources.captionBoldFont})
        ui.EnsureGlyphsForText(font, "\xE2\x80\xA2!#$%&*()+=[]{}<>/\\|~;:'\",?^`@-_.SHIFTabc");
    m_keyboard = std::make_shared<Keyboard>(*this, resources);
    m_menu.MenuItems.push_back(m_keyboard);
    m_menu.BackPress = [this]()
    {
        if (back)
            Navigate(back, -1);
    };
    // X deletes, Y shifts.
    m_menu.XPress = [this]() { TypeBackspace(); };
    m_menu.YPress = [this]()
    {
        m_keyboard->m_shift = !m_keyboard->m_shift;
        m_keyboard->m_symbols = false;
    };
    m_menu.Init();
}

std::vector<MenuHint> TextInputPage::Hints() const
{
    return {{UiIconId::ButtonA, "Type"}, {UiIconId::ButtonX, "Delete"}, {UiIconId::ButtonY, "Shift"}, {UiIconId::ButtonB, "Back"}};
}

void TextInputPage::Open(const std::string &title, const std::string &text, bool hidden,
                         std::function<void(const std::string &)> done)
{
    m_title = title;
    m_text = text;
    m_hidden = hidden;
    m_done = std::move(done);
    if (m_keyboard)
    {
        m_keyboard->ResetSelection();
        m_keyboard->m_blink = 0;
    }
}

void TextInputPage::TypeText(const std::string &utf8)
{
    if (m_text.size() + utf8.size() <= 128)
        m_text += utf8;
    if (m_keyboard)
        m_keyboard->m_blink = 0;
}

void TextInputPage::TypeBackspace()
{
    // (a whole character, however many bytes)
    while (!m_text.empty() && (static_cast<unsigned char>(m_text.back()) & 0xC0) == 0x80)
        m_text.pop_back();
    if (!m_text.empty())
        m_text.pop_back();
    if (m_keyboard)
        m_keyboard->m_blink = 0;
}

void TextInputPage::TypeEnter() { Finish(); }

void TextInputPage::Finish()
{
    if (m_done)
        m_done(m_text);
    if (back)
        Navigate(back, -1);
}
