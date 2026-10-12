#include "menu/pages/TextInputPage.h"
#include "io/Platform.h"
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
    // keys are wider (in tenths of the row). Above them, the field's row: the
    // eye (inside the field, at its end - a password's), Copy and Paste.
    enum Special
    {
        KeyChar,
        KeyShift,
        KeySymbols,
        KeySpace,
        KeyDelete,
        KeyDone,
        KeyReveal,
        KeyCopy,
        KeyPaste
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
    constexpr float kToolW = 32.0f; // Copy, Paste
    constexpr float kEyeW = 20.0f;
    constexpr float kNoticeSeconds = 2.0f;
    constexpr size_t kMaxBytes = 128;
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

    bool HasClipboard() const { return m_res.platform && m_res.platform->HasClipboard(); }
    // The field's row has keys: the eye (a password's), Copy and Paste.
    bool HasTools() const { return m_page.m_hidden || HasClipboard(); }
    // The field: the row's width, less Copy and Paste.
    float FieldWidth() const { return kContentWidth - (HasClipboard() ? 2.0f * (kToolW + kKeyGap) : 0.0f); }

    // The keys as laid out now (shift and symbols change the letters).
    std::vector<std::vector<Key>> Rows() const
    {
        std::vector<std::vector<Key>> rows;
        if (HasTools())
        {
            std::vector<Key> tools;
            const float fieldRight = kContentX + FieldWidth();
            if (m_page.m_hidden)
            {
                Key eye;
                eye.kind = KeyReveal;
                eye.x = fieldRight - kEyeW;
                eye.y = kFieldY;
                eye.w = kEyeW;
                eye.h = kFieldH;
                tools.push_back(eye);
            }
            if (HasClipboard())
                for (Special kind : {KeyCopy, KeyPaste})
                {
                    Key key;
                    key.kind = kind;
                    key.x = fieldRight + kKeyGap + (kind == KeyPaste ? kToolW + kKeyGap : 0.0f);
                    key.y = kFieldY;
                    key.w = kToolW;
                    key.h = kFieldH;
                    tools.push_back(key);
                }
            rows.push_back(tools);
        }
        const char *const *letters = m_symbols ? kSymbols : (m_shift ? kUpper : kLower);
        const float unit = (kContentWidth - 9.0f * kKeyGap) / 10.0f;
        int keyRows = 0; // (the keyboard's rows so far)
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
                key.y = kKeysY + static_cast<float>(keyRows) * (kKeyH + kKeyGap);
                key.w = unit;
                row.push_back(key);
                x += unit + kKeyGap;
            }
            rows.push_back(row);
            ++keyRows;
        }
        std::vector<Key> bottom;
        float x = kContentX;
        const float total = kContentWidth - (kSpecialCount - 1) * kKeyGap;
        for (int i = 0; i < kSpecialCount; ++i)
        {
            Key key;
            key.kind = kBottom[i];
            key.x = x;
            key.y = kKeysY + static_cast<float>(keyRows) * (kKeyH + kKeyGap);
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
        case KeyCopy:
            return "Copy";
        case KeyPaste:
            return "Paste";
        case KeyReveal:
            return ""; // (an icon)
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
        case KeyReveal:
            m_page.m_reveal = !m_page.m_reveal;
            break;
        case KeyCopy:
            m_page.Copy();
            break;
        case KeyPaste:
            m_page.Paste();
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
        m_shift = m_symbols = false;
        m_row = HasTools() ? 2 : 1; // (on the q)
        m_col = 0;
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
        m_page.m_noticeTime = std::max(0.0f, m_page.m_noticeTime - deltaSeconds);
    }

    void Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha) override
    {
        const UiFontHandle body = m_res.bodyFont, bold = m_res.bodyBoldFont;

        // The field: the text (or dots), the end of it in view, and a caret.
        const float fx = kContentX + offsetX, fy = kFieldY + offsetY, fw = FieldWidth();
        ui.DrawQuadRounded(fx, fy, fw, kFieldH, WithAlpha(kMenuCardColor, alpha), kCardRadius);
        ui.DrawQuadRounded(fx, fy + kFieldH - 1.2f, fw, 1.2f, WithAlpha(kMenuSelectionColor, alpha * 0.8f), 0.6f);
        std::string shown;
        if (m_page.m_hidden && !m_page.m_reveal)
            for (size_t i = 0; i < CharCount(m_page.m_text); ++i)
                shown += "\xE2\x80\xA2";
        else
            shown = m_page.m_text;
        const float room = fw - 16.0f - (m_page.m_hidden ? kEyeW : 0.0f);
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
                if (k.kind == KeyReveal)
                {
                    // The eye, at the field's end: crossed out while the
                    // password is dots, open (and lit) while it's shown.
                    if (sel)
                    {
                        ui.DrawQuadRounded(x - 1.2f, y - 1.2f, k.w + 2.4f, k.h + 2.4f, WithAlpha(kMenuSelectionColor, alpha), 5.0f);
                        ui.DrawQuadRounded(x, y, k.w, k.h, WithAlpha(kMenuCardColor, alpha), 4.0f);
                        ui.DrawQuadRounded(x, y, k.w, k.h, WithAlpha(kMenuSelectionFillColor, alpha), 4.0f);
                    }
                    if (m_res.icons)
                    {
                        const float size = 14.0f;
                        const XrColor4f tint = sel || m_page.m_reveal ? kMenuSelectionColor : kMenuDimTextColor;
                        m_res.icons->Draw(ui, m_page.m_reveal ? UiIconId::Eye : UiIconId::EyeOff, x + (k.w - size) / 2.0f,
                                          y + (k.h - size) / 2.0f, size, alpha, tint);
                    }
                    continue;
                }
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
                // (Copy can't while a password is dots)
                const bool unavailable = k.kind == KeyCopy && m_page.m_hidden && !m_page.m_reveal;
                const XrColor4f color = sel || on || k.kind == KeyDone ? kMenuSelectionColor
                                        : unavailable                  ? kMenuDimTextColor
                                                                       : kMenuTextColor;
                ui.DrawText(font, label, x + (k.w - ui.GetTextWidth(font, label)) / 2.0f, TextY(ui, font, y + k.h / 2.0f), 1.0f,
                            WithAlpha(color, alpha));
            }

        // A notice under the keys for a moment ("Pasted"), fading out.
        if (m_page.m_noticeTime > 0 && !m_page.m_notice.empty())
        {
            const Key &last = rows.back().front();
            const UiFontHandle font = m_res.captionFont;
            const float fade = std::min(1.0f, m_page.m_noticeTime / 0.4f);
            ui.DrawText(font, m_page.m_notice, kContentX + offsetX + (kContentWidth - ui.GetTextWidth(font, m_page.m_notice)) / 2.0f,
                        TextY(ui, font, last.y + last.h + 9.0f + offsetY), 1.0f, WithAlpha(kMenuDimTextColor, alpha * fade));
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
    m_ui = &ui;
    m_res = &resources;
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
    m_reveal = false;
    m_noticeTime = 0;
    m_done = std::move(done);
    if (m_keyboard)
    {
        m_keyboard->ResetSelection();
        m_keyboard->m_blink = 0;
    }
}

void TextInputPage::TypeText(const std::string &utf8)
{
    if (m_text.size() + utf8.size() <= kMaxBytes)
    {
        m_text += utf8;
        if (m_ui && m_res) // (a real keyboard's accented letters too)
            m_ui->EnsureGlyphsForText(m_res->bodyFont, utf8);
    }
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

Platform *TextInputPage::ClipboardPlatform() const
{
    return m_res && m_res->platform && m_res->platform->HasClipboard() ? m_res->platform : nullptr;
}

void TextInputPage::Paste()
{
    Platform *platform = ClipboardPlatform();
    if (!platform)
        return;
    // Its first line, without the spaces around it (a copied password often
    // brings a line break or a space along) or any control characters.
    const std::string clip = platform->GetClipboardText();
    size_t begin = 0;
    while (begin < clip.size() && (clip[begin] == ' ' || clip[begin] == '\t' || clip[begin] == '\r' || clip[begin] == '\n'))
        ++begin;
    size_t end = clip.find_first_of("\r\n", begin);
    if (end == std::string::npos)
        end = clip.size();
    while (end > begin && (clip[end - 1] == ' ' || clip[end - 1] == '\t'))
        --end;
    std::string line;
    for (size_t i = begin; i < end; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(clip[i]);
        if (c >= 0x20 && c != 0x7F)
            line += static_cast<char>(c);
    }
    if (line.empty())
    {
        Notice("Nothing to paste");
        return;
    }
    // As much as fits, in whole characters.
    const size_t room = kMaxBytes - std::min(kMaxBytes, m_text.size());
    if (line.size() > room)
    {
        size_t cut = room;
        while (cut > 0 && (static_cast<unsigned char>(line[cut]) & 0xC0) == 0x80)
            --cut;
        line.resize(cut);
    }
    if (line.empty())
    {
        Notice("The field is full");
        return;
    }
    m_text += line;
    if (m_ui && m_res)
        m_ui->EnsureGlyphsForText(m_res->bodyFont, line);
    if (m_keyboard)
        m_keyboard->m_blink = 0;
    Notice("Pasted");
}

void TextInputPage::Copy()
{
    Platform *platform = ClipboardPlatform();
    if (!platform)
        return;
    if (m_hidden && !m_reveal)
    {
        Notice("Show the password to copy it");
        return;
    }
    if (m_text.empty())
    {
        Notice("Nothing to copy");
        return;
    }
    platform->SetClipboardText(m_text, m_hidden);
    Notice("Copied");
}

void TextInputPage::Notice(const std::string &text)
{
    m_notice = text;
    m_noticeTime = kNoticeSeconds;
}

void TextInputPage::Finish()
{
    if (m_done)
        m_done(m_text);
    if (back)
        Navigate(back, -1);
}
