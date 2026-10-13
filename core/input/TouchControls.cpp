#include "input/TouchControls.h"
#include "emu/Emulator.h" // VBButtonBit
#include "io/Platform.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <tuple>

namespace
{
    constexpr float kVbWidth = 384.0f, kVbHeight = 224.0f;

    constexpr uint32_t Bit(uint32_t vbBit) { return 1u << vbBit; }

    const XrColor4f kFill{1.0f, 1.0f, 1.0f, 0.13f};
    const XrColor4f kPressed{0.97f, 0.77f, 0.27f, 0.60f};

    // The controls' icons: Kenney's Input Prompts (Nintendo Switch, CC0),
    // packed by tools/pack_touch_atlas.py in this order, 4 to a row - white,
    // the letters see-through; the arms are masks of the D-pad's arms.
    enum TouchIcon
    {
        kIconDpad,
        kIconArmUp,
        kIconArmDown,
        kIconArmLeft,
        kIconArmRight,
        kIconA,
        kIconB,
        kIconL,
        kIconR,
        kIconMinus,
        kIconPlus,
        kIconHome,
        kIconCount
    };
    constexpr int kAtlasColumns = 4, kAtlasRows = (kIconCount + kAtlasColumns - 1) / kAtlasColumns;
    constexpr int kAtlasCells[3] = {64, 128, 256};
    constexpr float kIconAlpha = 0.5f, kIconPressedAlpha = 0.9f;
    const XrColor4f kIconPressedTint{0.91f, 0.20f, 0.27f, 1.0f}; // (the red of Kenney's pressed D-pad)
    const XrColor4f kLabel{1.0f, 1.0f, 1.0f, 0.78f};
    const XrColor4f kEditBox{0.97f, 0.77f, 0.27f, 0.22f};
    const XrColor4f kEditBar{0.08f, 0.09f, 0.12f, 0.88f};

    constexpr const char *kLayoutFile = "touch-layout.txt";
    // A game's own layout (TouchControls::SetGame).
    std::string GameLayoutFile(const std::string &game) { return game + ".touch"; }
    // (every label the small font draws)
    constexpr const char *kSmallGlyphs = "SELECTSTARTSMALLERBIGGERRESETDONEBACK TO GAME";

    bool InCircle(float cx, float cy, float r, float x, float y) { return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r; }

    float TextY(UiRenderer &ui, UiFontHandle font, float centerY)
    {
        return centerY - ui.GetFontPHeight(font) / 2.0f - ui.GetFontPStart(font);
    }
} // namespace

void TouchControls::Initialize(UiRenderer &ui, Platform &platform)
{
    m_ui = &ui;
    m_platform = &platform;
    m_fontBytes = platform.LoadAssetBytes("fonts/Roboto-Bold.ttf");
    if (m_fontBytes.empty())
        return;
    // (sizes for a typical phone - Layout rebakes them for the real density)
    m_font = ui.LoadFont(m_fontBytes, 52);
    m_smallFont = ui.LoadFont(m_fontBytes, 28);
    m_fontDp = 2.625f;
    ui.EnsureGlyphsForText(m_font, "ABLR");
    ui.EnsureGlyphsForText(m_smallFont, kSmallGlyphs);
    Load(platform);
}

bool TouchControls::UsesRightDpad(const std::string &romName)
{
    std::string name = romName;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const char *game : {"red alarm", "teleroboxer", "3-d tetris", "3d tetris", "golf"})
        if (name.find(game) != std::string::npos)
            return true;
    return false;
}

void TouchControls::Layout(float w, float h, float dp, bool rightDpad)
{
    m_w = w;
    m_h = h;
    m_dp = dp;
    m_rightDpad = rightDpad;
    if (m_ui && m_font.IsValid() && std::fabs(dp - m_fontDp) > 0.01f)
    {
        // (labels as sharp as the screen's density allows)
        m_fontDp = dp;
        m_ui->RebakeFont(m_font, m_fontBytes, static_cast<int>(std::lround(20.0f * dp)));
        m_ui->RebakeFont(m_smallFont, m_fontBytes, static_cast<int>(std::lround(10.5f * dp)));
        m_ui->EnsureGlyphsForText(m_font, "ABLR");
        m_ui->EnsureGlyphsForText(m_smallFont, kSmallGlyphs);
    }

    const float pad = 132.0f * dp; // a D-pad's size
    const float margin = 12.0f * dp;
    const float shoulderW = 88.0f * dp, shoulderH = 36.0f * dp;
    const float pillW = 60.0f * dp, pillH = 28.0f * dp;
    const float buttonR = 29.0f * dp;

    if (h > w)
    {
        // Portrait: the screen on top, the controller below it.
        const float s = w / kVbWidth;
        m_screenW = w;
        m_screenH = kVbHeight * s;
        m_screenX = 0.0f;
        m_screenY = std::max(24.0f * dp, std::min(h * 0.08f, h - m_screenH - pad - 120.0f * dp));
        const float top = m_screenY + m_screenH + margin;
        const float lx = w * 0.27f, rx = w * 0.73f;
        m_l = {margin, top, shoulderW, shoulderH};
        m_r = {w - margin - shoulderW, top, shoulderW, shoulderH};
        const float cy = std::min(top + shoulderH + margin + pad / 2.0f + 8.0f * dp, h - pad / 2.0f - pillH - 3 * margin);
        m_leftPad = {lx, cy, pad / 2.0f};
        m_select = {lx - pillW - 6.0f * dp, cy + pad / 2.0f + margin, pillW, pillH};
        m_start = {lx + 6.0f * dp, cy + pad / 2.0f + margin, pillW, pillH};
        m_rightPad = {rx, cy - (rightDpad ? 30.0f * dp : 0.0f), pad / 2.0f * 0.85f};
        const float by = rightDpad ? m_rightPad.y + m_rightPad.r + buttonR + 10.0f * dp : cy;
        m_b = {rx - buttonR - 8.0f * dp, by + 14.0f * dp, buttonR};
        m_a = {rx + buttonR + 8.0f * dp, by - 14.0f * dp, buttonR};
        m_menu = {w / 2.0f, top + shoulderH / 2.0f, 17.0f * dp};
        ApplyAdjust();
        return;
    }

    // Landscape: the screen between the two sides, as big as fits with the
    // controls reaching at most a little over its edges - or, on a screen too
    // square for that (a tablet), as big as the whole screen with the
    // controls over its sides.
    const float side = pad + 28.0f * dp - 48.0f * dp;
    float s = std::min((w - 2.0f * side) / kVbWidth, h / kVbHeight);
    const bool overlay = s * kVbHeight < 0.72f * h;
    if (overlay)
        s = std::min(w / kVbWidth, h / kVbHeight);
    m_screenW = std::floor(kVbWidth * s);
    m_screenH = std::floor(kVbHeight * s);
    m_screenX = std::floor((w - m_screenW) / 2.0f);
    m_screenY = std::floor((h - m_screenH) / 2.0f);

    const float lx = std::max(pad / 2.0f + margin, overlay ? 0.0f : m_screenX / 2.0f);
    const float rx = w - lx;
    m_l = {lx - shoulderW / 2.0f, margin, shoulderW, shoulderH};
    m_r = {rx - shoulderW / 2.0f, margin, shoulderW, shoulderH};

    // Left: the D-pad, Select and Start below it.
    const float below = pillH + 2.0f * margin;
    const float topRoom = margin + shoulderH + margin;
    const float ly = std::clamp(h * 0.50f, topRoom + pad / 2.0f, h - below - pad / 2.0f);
    m_leftPad = {lx, ly, pad / 2.0f};
    const float pillY = std::min(ly + pad / 2.0f + margin, h - pillH - margin);
    m_select = {lx - pillW - 5.0f * dp, pillY, pillW, pillH};
    m_start = {lx + 5.0f * dp, pillY, pillW, pillH};

    // Right: the D-pad (the games that use it), B and A below it.
    const float smallPad = pad * 0.9f;
    m_rightPad = {rx, topRoom + smallPad / 2.0f, smallPad / 2.0f};
    float by = rightDpad ? std::min(m_rightPad.y + m_rightPad.r + buttonR + 16.0f * dp, h - buttonR - margin - 14.0f * dp)
                         : std::clamp(h * 0.55f, topRoom + buttonR + 14.0f * dp, h - buttonR - margin - 14.0f * dp);
    m_b = {rx - buttonR - 7.0f * dp, by + 14.0f * dp, buttonR};
    m_a = {rx + buttonR + 7.0f * dp, by - 14.0f * dp, buttonR};

    // The menu: small, at the bottom left - under Select and Start, off the
    // game's picture.
    const float menuR = 16.0f * dp;
    m_menu = {lx, std::min(pillY + pillH + margin + menuR, h - margin - menuR), menuR};
    ApplyAdjust();
}

void TouchControls::ChooseAtlas()
{
    // The biggest control, drawn from a cell at most about 1.6 times smaller
    // (bigger is sharp; a bit smaller still smooth).
    float biggest = 0.0f;
    for (const Circle *c : {&m_leftPad, &m_rightPad, &m_a, &m_b, &m_menu})
        biggest = std::max(biggest, 2.0f * c->r);
    int atlas = 0;
    while (atlas < 2 && kAtlasCells[atlas] * 1.6f < biggest)
        ++atlas;
    m_atlas = atlas;
    if (!m_atlases[atlas].IsValid() && m_ui && m_platform)
    {
        const std::vector<uint8_t> bytes = m_platform->LoadAssetBytes("icons/touch_atlas_" + std::to_string(kAtlasCells[atlas]) + ".png");
        uint32_t w = 0, h = 0;
        if (!bytes.empty())
            m_atlases[atlas] = m_ui->LoadImage(bytes, w, h);
    }
}

void TouchControls::ApplyAdjust()
{
    const bool portrait = m_h > m_w;
    const float margin = 6.0f * m_dp;
    // A control's center, moved as the player moved it - but all of the
    // control stays on the screen (halfW, halfH: its half size), and the
    // move is kept as it ended up, so dragging past an edge and back
    // doesn't leave it stuck there.
    auto center = [&](float &x, float &y, float halfW, float halfH, Adjust &a)
    {
        const float x0 = x, y0 = y;
        x = std::clamp(x + a.dx * m_w, std::min(halfW + margin, m_w / 2.0f), std::max(m_w - halfW - margin, m_w / 2.0f));
        y = std::clamp(y + a.dy * m_h, std::min(halfH + margin, m_h / 2.0f), std::max(m_h - halfH - margin, m_h / 2.0f));
        a.dx = (x - x0) / m_w;
        a.dy = (y - y0) / m_h;
    };
    auto circle = [&](Circle &c, Adjust &a)
    {
        c.r *= a.scale;
        center(c.x, c.y, c.r, c.r, a);
    };
    auto rect = [&](Rect &r, Adjust &a)
    {
        float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f;
        r.w *= a.scale;
        r.h *= a.scale;
        center(cx, cy, r.w / 2.0f, r.h / 2.0f, a);
        r.x = cx - r.w / 2.0f;
        r.y = cy - r.h / 2.0f;
    };
    Adjust *a = m_adjust[portrait ? 1 : 0];
    circle(m_leftPad, a[kLeftPad]);
    circle(m_rightPad, a[kRightPad]);
    circle(m_a, a[kA]);
    circle(m_b, a[kB]);
    rect(m_l, a[kL]);
    rect(m_r, a[kR]);
    rect(m_select, a[kSelect]);
    rect(m_start, a[kStart]);
    circle(m_menu, a[kMenu]);

    // The edit bar: four pills along the top.
    const float gap = 8.0f * m_dp, bh = 32.0f * m_dp;
    const float bw = std::min(104.0f * m_dp, (m_w - 5.0f * gap) / 4.0f);
    const float bx = (m_w - 4.0f * bw - 3.0f * gap) / 2.0f;
    m_editSmaller = {bx, gap, bw, bh};
    m_editBigger = {bx + (bw + gap), gap, bw, bh};
    m_editReset = {bx + 2.0f * (bw + gap), gap, bw, bh};
    m_editDone = {bx + 3.0f * (bw + gap), gap, bw, bh};
    ChooseAtlas();
}

void TouchControls::ElementBox(int element, float &x, float &y, float &w, float &h) const
{
    auto ofCircle = [&](const Circle &c)
    {
        x = c.x - c.r;
        y = c.y - c.r;
        w = h = 2.0f * c.r;
    };
    auto ofRect = [&](const Rect &r)
    {
        x = r.x;
        y = r.y;
        w = r.w;
        h = r.h;
    };
    switch (element)
    {
    case kLeftPad: ofCircle(m_leftPad); break;
    case kRightPad: ofCircle(m_rightPad); break;
    case kA: ofCircle(m_a); break;
    case kB: ofCircle(m_b); break;
    case kL: ofRect(m_l); break;
    case kR: ofRect(m_r); break;
    case kSelect: ofRect(m_select); break;
    case kStart: ofRect(m_start); break;
    default: ofCircle(m_menu); break;
    }
}

int TouchControls::ElementAt(float px, float py) const
{
    // (the small ones first - they may sit over a D-pad's reach)
    static constexpr int kOrder[] = {kMenu, kA, kB, kL, kR, kSelect, kStart, kRightPad, kLeftPad};
    const float slack = 8.0f * m_dp;
    for (int e : kOrder)
    {
        if (e == kRightPad && !m_rightDpad)
            continue;
        float x, y, w, h;
        ElementBox(e, x, y, w, h);
        if (px >= x - slack && px <= x + w + slack && py >= y - slack && py <= y + h + slack)
            return e;
    }
    return -1;
}

void TouchControls::MoveElement(int element, float dx, float dy)
{
    if (element < 0 || m_w <= 0 || m_h <= 0)
        return;
    Adjust &a = AdjustOf(element);
    a.dx = std::clamp(a.dx + dx / m_w, -1.0f, 1.0f);
    a.dy = std::clamp(a.dy + dy / m_h, -1.0f, 1.0f);
}

void TouchControls::BeginEdit(Platform &platform, const std::string &game)
{
    // The layout to edit: that game's (its own, or the general one to start
    // from - saved as its own), or the general one.
    m_forGame = game;
    if (game.empty() || !Parse(platform.ReadRomsFile(GameLayoutFile(game), true)))
        Load(platform);
    m_layoutGame = game;
    m_editing = true;
    m_visible = true;
    m_selected = -1;
    m_dragId = -1;
    m_lastIds.clear();
    m_editFresh = true;
}

bool TouchControls::UpdateEdit(const std::vector<Touch> &touches)
{
    auto inRect = [](const Rect &r, float x, float y) { return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h; };
    if (m_editFresh)
    {
        // (the finger that tapped Edit in the menu may still be down - it
        // doesn't count)
        m_editFresh = false;
        m_lastIds.clear();
        for (const Touch &t : touches)
            m_lastIds.push_back(t.id);
        return false;
    }
    bool done = false;
    std::vector<int> ids;
    bool dragging = false;
    for (const Touch &t : touches)
    {
        ids.push_back(t.id);
        const bool fresh = std::find(m_lastIds.begin(), m_lastIds.end(), t.id) == m_lastIds.end();
        if (fresh)
        {
            if (inRect(m_editDone, t.x, t.y))
                done = true;
            else if (inRect(m_editReset, t.x, t.y))
            {
                for (Adjust &a : m_adjust[m_h > m_w ? 1 : 0])
                    a = Adjust{};
                m_selected = -1;
            }
            else if (inRect(m_editSmaller, t.x, t.y) || inRect(m_editBigger, t.x, t.y))
            {
                if (m_selected >= 0)
                {
                    Adjust &a = AdjustOf(m_selected);
                    a.scale = std::clamp(a.scale * (inRect(m_editBigger, t.x, t.y) ? 1.1f : 1.0f / 1.1f), 0.5f, 2.0f);
                }
            }
            else
            {
                m_selected = ElementAt(t.x, t.y);
                m_dragId = m_selected >= 0 ? t.id : -1;
                m_dragX = t.x;
                m_dragY = t.y;
            }
        }
        if (t.id == m_dragId)
        {
            dragging = true;
            MoveElement(m_selected, t.x - m_dragX, t.y - m_dragY);
            m_dragX = t.x;
            m_dragY = t.y;
        }
    }
    if (!dragging)
        m_dragId = -1;
    m_lastIds = ids;
    return done;
}

void TouchControls::DrawEdit(UiRenderer &ui) const
{
    // The controls, each with its box; the chosen one stands out.
    for (int e = 0; e < kElementCount; ++e)
    {
        if (e == kRightPad && !m_rightDpad)
            continue;
        float x, y, w, h;
        ElementBox(e, x, y, w, h);
        const float pad = 3.0f * m_dp;
        ui.DrawQuadRounded(x - pad, y - pad, w + 2 * pad, h + 2 * pad, e == m_selected ? kPressed : kEditBox, 6.0f * m_dp);
    }
    TouchControls shown = *this;
    shown.m_pressed = 0;
    shown.m_visible = true;
    shown.Draw(ui);
    const float barPad = 4.0f * m_dp;
    ui.DrawQuadRounded(m_editSmaller.x - barPad, m_editSmaller.y - barPad,
                       m_editDone.x + m_editDone.w - m_editSmaller.x + 2 * barPad, m_editDone.h + 2 * barPad, kEditBar,
                       (m_editDone.h + 2 * barPad) / 2.0f);
    DrawPill(ui, m_editSmaller, "SMALLER", false, m_smallFont);
    DrawPill(ui, m_editBigger, "BIGGER", false, m_smallFont);
    DrawPill(ui, m_editReset, "RESET", false, m_smallFont);
    DrawPill(ui, m_editDone, "DONE", true, m_smallFont);
}

void TouchControls::LayoutResume(float menuRight, float menuTop)
{
    const float gap = 10.0f * m_dp, w = 128.0f * m_dp, h = 34.0f * m_dp;
    if (m_w - menuRight >= w + 2.0f * gap)
        m_resume = {menuRight + (m_w - menuRight - w) / 2.0f, std::max(gap, menuTop + gap), w, h};
    else
        m_resume = {m_w - w - gap, gap, w, h};
}

bool TouchControls::UpdateResume(const std::vector<Touch> &touches)
{
    bool down = false;
    for (const Touch &t : touches)
        if (t.x >= m_resume.x && t.x <= m_resume.x + m_resume.w && t.y >= m_resume.y && t.y <= m_resume.y + m_resume.h)
            down = true;
    const bool tapped = down && !m_resumeDown;
    m_resumeDown = down;
    return tapped;
}

void TouchControls::DrawResume(UiRenderer &ui) const
{
    ui.DrawQuadRounded(m_resume.x, m_resume.y, m_resume.w, m_resume.h, kEditBar, m_resume.h / 2.0f);
    DrawPill(ui, m_resume, "BACK TO GAME", m_resumeDown, m_smallFont);
}

bool TouchControls::Parse(const std::vector<uint8_t> &bytes)
{
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    std::string word;
    int version = 0;
    if (!(in >> word >> version) || word != "vboycolor-touch-layout" || version != 1)
        return false;
    for (auto &orientation : m_adjust)
        for (Adjust &a : orientation)
            a = Adjust{};
    int orientation = 0, element = 0;
    Adjust a;
    while (in >> orientation >> element >> a.dx >> a.dy >> a.scale)
        if (orientation >= 0 && orientation < 2 && element >= 0 && element < kElementCount)
            m_adjust[orientation][element] = {std::clamp(a.dx, -1.0f, 1.0f), std::clamp(a.dy, -1.0f, 1.0f),
                                              std::clamp(a.scale, 0.5f, 2.0f)};
    return true;
}

void TouchControls::Load(Platform &platform)
{
    if (!Parse(platform.ReadRomsFile(kLayoutFile, true)))
        for (auto &orientation : m_adjust)
            for (Adjust &a : orientation)
                a = Adjust{};
    m_layoutGame.clear();
}

void TouchControls::Save(Platform &platform) const
{
    std::string text = "vboycolor-touch-layout 1\n";
    char line[96];
    for (int o = 0; o < 2; ++o)
        for (int e = 0; e < kElementCount; ++e)
        {
            const Adjust &a = m_adjust[o][e];
            std::snprintf(line, sizeof(line), "%d %d %.5f %.5f %.4f\n", o, e, a.dx, a.dy, a.scale);
            text += line;
        }
    platform.WriteRomsFile(m_layoutGame.empty() ? std::string(kLayoutFile) : GameLayoutFile(m_layoutGame), true,
                           text.data(), text.size());
}

void TouchControls::SetGame(Platform &platform, const std::string &game)
{
    if (game == m_forGame || m_editing)
        return;
    m_forGame = game;
    if (game.empty() || !Parse(platform.ReadRomsFile(GameLayoutFile(game), true)))
        Load(platform);
    else
        m_layoutGame = game;
}

bool TouchControls::HasGameLayout(Platform &platform, const std::string &game)
{
    const std::vector<uint8_t> bytes = platform.ReadRomsFile(GameLayoutFile(game), true);
    static const std::string kHeader = "vboycolor-touch-layout 1";
    return bytes.size() >= kHeader.size() && std::equal(kHeader.begin(), kHeader.end(), bytes.begin());
}

void TouchControls::ForgetGameLayout(Platform &platform, const std::string &game)
{
    // (no deleting through every platform's folder access: an empty file is "none")
    const char none[] = "none\n";
    platform.WriteRomsFile(GameLayoutFile(game), true, none, sizeof(none) - 1);
    if (game == m_layoutGame || game == m_forGame)
    {
        const std::string current = m_forGame;
        m_forGame = "\x01"; // (read again)
        SetGame(platform, current);
    }
}

void TouchControls::ScreenArea(float &x, float &y, float &w, float &h) const
{
    if (m_visible && !m_overGame)
    {
        x = m_screenX;
        y = m_screenY;
        w = m_screenW;
        h = m_screenH;
        return;
    }
    const float s = std::min(m_w / kVbWidth, m_h / kVbHeight);
    w = std::floor(kVbWidth * s);
    h = std::floor(kVbHeight * s);
    x = std::floor((m_w - w) / 2.0f);
    y = std::floor((m_h - h) / 2.0f);
}

uint32_t TouchControls::DpadBits(const Circle &pad, float x, float y, uint32_t up, uint32_t down, uint32_t left,
                                 uint32_t right) const
{
    const float dx = x - pad.x, dy = y - pad.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > pad.r * 1.2f || dist < pad.r * 0.16f)
        return 0;
    // 8 directions, 45 degrees each - the diagonals press both.
    const float angle = std::atan2(dy, dx); // 0 right, +pi/2 down
    const int sector = static_cast<int>(std::lround(angle / (3.14159265f / 4.0f))) & 7;
    static constexpr int kDirs[8] = {0b0001, 0b0101, 0b0100, 0b0110, 0b0010, 0b1010, 0b1000, 0b1001}; // right/left/down/up bits
    const int d = kDirs[sector];
    return ((d & 0b0001) ? right : 0) | ((d & 0b0010) ? left : 0) | ((d & 0b0100) ? down : 0) | ((d & 0b1000) ? up : 0);
}

uint32_t TouchControls::Update(const std::vector<Touch> &touches, bool &menuPressed)
{
    menuPressed = false;
    m_lastPressed = m_pressed;
    m_pressed = 0;
    if (!m_visible)
    {
        m_menuDown = false;
        return 0;
    }
    using namespace VBButtonBit;
    const float slack = 10.0f * m_dp;
    auto inRect = [slack](const Rect &r, float x, float y)
    { return x >= r.x - slack && x <= r.x + r.w + slack && y >= r.y - slack && y <= r.y + r.h + slack; };
    bool menuDown = false;
    for (const Touch &t : touches)
    {
        m_pressed |= DpadBits(m_leftPad, t.x, t.y, Bit(LeftUp), Bit(LeftDown), Bit(LeftLeft), Bit(LeftRight));
        if (m_rightDpad)
            m_pressed |= DpadBits(m_rightPad, t.x, t.y, Bit(RightUp), Bit(RightDown), Bit(RightLeft), Bit(RightRight));
        if (InCircle(m_a.x, m_a.y, m_a.r * 1.3f, t.x, t.y))
            m_pressed |= Bit(A);
        if (InCircle(m_b.x, m_b.y, m_b.r * 1.3f, t.x, t.y))
            m_pressed |= Bit(B);
        if (inRect(m_l, t.x, t.y))
            m_pressed |= Bit(L);
        if (inRect(m_r, t.x, t.y))
            m_pressed |= Bit(R);
        if (inRect(m_select, t.x, t.y))
            m_pressed |= Bit(Select);
        if (inRect(m_start, t.x, t.y))
            m_pressed |= Bit(Start);
        if (InCircle(m_menu.x, m_menu.y, m_menu.r * 1.8f, t.x, t.y))
            menuDown = true;
    }
    menuPressed = menuDown && !m_menuDown;
    m_menuDown = menuDown;
    return m_pressed;
}

void TouchControls::DrawIcon(UiRenderer &ui, int icon, float cx, float cy, float size, bool pressed) const
{
    const UiImageHandle atlas = m_atlas >= 0 ? m_atlases[m_atlas] : UiImageHandle{};
    if (!atlas.IsValid())
        return;
    const float u0 = static_cast<float>(icon % kAtlasColumns) / kAtlasColumns;
    const float v0 = static_cast<float>(icon / kAtlasColumns) / kAtlasRows;
    ui.DrawImageRegion(atlas, cx - size / 2.0f, cy - size / 2.0f, size, size, u0, v0, u0 + 1.0f / kAtlasColumns,
                       v0 + 1.0f / kAtlasRows, pressed ? kIconPressedAlpha : kIconAlpha,
                       pressed ? kIconPressedTint : XrColor4f{1.0f, 1.0f, 1.0f, 1.0f});
}

void TouchControls::DrawDpad(UiRenderer &ui, const Circle &pad, uint32_t up, uint32_t down, uint32_t left,
                             uint32_t right) const
{
    // The cross fills its cell (pack_touch_atlas.py): as wide as the pad.
    const float size = 2.0f * pad.r * (16.0f / 14.0f);
    DrawIcon(ui, kIconDpad, pad.x, pad.y, size, false);
    const int arms[4] = {kIconArmUp, kIconArmDown, kIconArmLeft, kIconArmRight};
    const uint32_t bits[4] = {up, down, left, right};
    for (int i = 0; i < 4; ++i)
        if (m_pressed & bits[i])
            DrawIcon(ui, arms[i], pad.x, pad.y, size, true);
}

void TouchControls::DrawPill(UiRenderer &ui, const Rect &r, const char *label, bool pressed, UiFontHandle font) const
{
    ui.DrawQuadRounded(r.x, r.y, r.w, r.h, pressed ? kPressed : kFill, r.h / 2.0f);
    if (font.IsValid())
        ui.DrawText(font, label, r.x + (r.w - ui.GetTextWidth(font, label)) / 2.0f, TextY(ui, font, r.y + r.h / 2.0f), 1.0f,
                    kLabel);
}

void TouchControls::Draw(UiRenderer &ui) const
{
    if (!m_visible)
        return;
    using namespace VBButtonBit;
    // (a cell's shape spans 14/16 of it - pack_touch_atlas.py's padding)
    constexpr float kCell = 16.0f / 14.0f;
    DrawDpad(ui, m_leftPad, Bit(LeftUp), Bit(LeftDown), Bit(LeftLeft), Bit(LeftRight));
    if (m_rightDpad)
        DrawDpad(ui, m_rightPad, Bit(RightUp), Bit(RightDown), Bit(RightLeft), Bit(RightRight));
    DrawIcon(ui, kIconA, m_a.x, m_a.y, 2.0f * m_a.r * kCell, m_pressed & Bit(A));
    DrawIcon(ui, kIconB, m_b.x, m_b.y, 2.0f * m_b.r * kCell, m_pressed & Bit(B));
    // L and R: their shoulder buttons, as tall as their (wider) spot and a bit more.
    for (const auto &[rect, icon, bit] : {std::tuple{&m_l, kIconL, Bit(L)}, std::tuple{&m_r, kIconR, Bit(R)}})
        DrawIcon(ui, icon, rect->x + rect->w / 2.0f, rect->y + rect->h / 2.0f, std::min(rect->w, rect->h * 1.3f) * kCell,
                 m_pressed & bit);
    // Select and Start: the Switch's - and +, named underneath.
    for (const auto &[rect, icon, bit, label] :
         {std::tuple{&m_select, kIconMinus, Bit(Select), "SELECT"}, std::tuple{&m_start, kIconPlus, Bit(Start), "START"}})
    {
        const float size = std::min(rect->w, rect->h * 1.1f);
        const float cx = rect->x + rect->w / 2.0f, cy = rect->y + rect->h / 2.0f - rect->h * 0.12f;
        DrawIcon(ui, icon, cx, cy, size * kCell, m_pressed & bit);
        if (m_smallFont.IsValid())
        {
            const float scale = std::min(1.0f, rect->h / (28.0f * m_dp)) * 0.8f;
            ui.DrawText(m_smallFont, label, cx - ui.GetTextWidth(m_smallFont, label) * scale / 2.0f,
                        cy + size / 2.0f + 1.0f * m_dp - ui.GetFontPStart(m_smallFont) * scale, scale,
                        XrColor4f{1.0f, 1.0f, 1.0f, 0.55f});
        }
    }
    DrawIcon(ui, kIconHome, m_menu.x, m_menu.y, 2.0f * m_menu.r * kCell, m_menuDown);
}
