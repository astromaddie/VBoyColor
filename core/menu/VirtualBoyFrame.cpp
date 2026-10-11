#include "menu/VirtualBoyFrame.h"

#include "menu/pages/AppMenuLayout.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
    // Sizes, as parts of the screen's short side, at most: the dark gap round
    // the menu, the eyeshade, the body round it, and the room left round the
    // body.
    constexpr float kPad = 0.022f, kRubber = 0.058f, kMaxBody = 0.3f, kMargin = 0.025f;
    // How wide the body's rounded edge is, at most (of the screen's short side).
    constexpr float kMaxBevel = 0.05f;
    // The menu panel's own corner radius (AppMenu::kPanelCornerRadiusPx).
    constexpr float kMenuCorner = 8.0f;
    // The picture's longest side, at most (it's drawn scaled up).
    constexpr int kMaxTexture = 1280;

    float RoundRect(float px, float py, float cx, float cy, float hw, float hh, float r)
    {
        const float qx = std::fabs(px - cx) - (hw - r), qy = std::fabs(py - cy) - (hh - r);
        return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
    }

    struct Vec3
    {
        float x, y, z;
    };
    Vec3 Normalized(Vec3 v)
    {
        const float n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return {v.x / n, v.y / n, v.z / n};
    }
    // The light: from above, a little left, in front.
    const Vec3 kLight = Normalized({-0.25f, -0.75f, 0.62f});

    // A rounded edge's light (d: the distance to the edge, < 0 inside; bevel:
    // how wide the rounding is; nx, ny: the edge's outward direction):
    // the brightness (1 on the flat) and a shine.
    void Bevel(float d, float bevel, float nx, float ny, float shine, float &light, float &spec)
    {
        const float t = std::clamp(-d / bevel, 0.0f, 1.0f); // 0 at the edge .. 1 on the flat
        const float s = 1.0f - t;
        const float nz = std::sqrt(std::max(0.0f, 1.0f - s * s));
        const Vec3 n{nx * s, ny * s, nz};
        light = std::clamp(n.x * kLight.x + n.y * kLight.y + n.z * kLight.z, 0.0f, 1.0f) / kLight.z;
        const Vec3 half = Normalized({kLight.x, kLight.y, kLight.z + 1.0f});
        spec = std::pow(std::clamp(n.x * half.x + n.y * half.y + n.z * half.z, 0.0f, 1.0f), 40.0f) * shine;
    }

    float Cover(float d) { return std::clamp(0.5f - d, 0.0f, 1.0f); }
    float Smooth01(float v) { return std::clamp(v, 0.0f, 1.0f); }

    struct Rgb
    {
        float r, g, b;
    };
    Rgb Mix(Rgb under, Rgb over, float a)
    {
        return {under.r + (over.r - under.r) * a, under.g + (over.g - under.g) * a, under.b + (over.b - under.b) * a};
    }
} // namespace

VirtualBoyFrame::Geometry VirtualBoyFrame::Layout(float w, float h)
{
    Geometry g;
    // The menu as big as it fits (as without the frame); the frame in the
    // room left on its sides (landscape: left and right - above and below,
    // it runs off the screen) or above and below it (portrait).
    g.menuScale = std::max(1.0f, std::min(w / static_cast<float>(kMenuWidth), h / static_cast<float>(kMenuHeight)));
    const float mw = kMenuWidth * g.menuScale, mh = kMenuHeight * g.menuScale;
    g.menuX = std::floor((w - mw) / 2.0f);
    g.menuY = std::floor((h - mh) / 2.0f);
    g.cx = g.menuX + mw / 2.0f;
    g.cy = g.menuY + mh / 2.0f;
    g.sides = (w - mw) >= (h - mh);

    // Along: across the room (x for the sides); across: the other way.
    const float room = g.sides ? (w - mw) / 2.0f : (h - mh) / 2.0f;
    const float menuAlong = (g.sides ? mw : mh) / 2.0f, screenAlong = (g.sides ? w : h) / 2.0f;
    const float screenAcross = (g.sides ? h : w) / 2.0f;
    const float u = std::min(w, h);
    const float margin = std::min(kMargin * u, 0.1f * room);
    const float pad = std::min(kPad * u, 0.08f * room);
    g.rubber = std::min(kRubber * u, 0.3f * room);
    g.body = std::max(room - pad - g.rubber - margin, 0.0f);

    float cavityAlong = menuAlong + pad;
    g.cavityR = kMenuCorner * g.menuScale + pad;
    float cavityAcross = screenAcross + g.cavityR + 2.0f; // (its corners off the screen)
    // The eyeshade: its corners just past the screen's, so its outer edge
    // curves in a little there - a goggle's.
    float rubberAlong = cavityAlong + g.rubber;
    float rubberAcross = screenAcross + g.rubber;
    g.rubberR = std::min(2.5f * g.rubber, rubberAlong);
    // (with a lot of room - a phone held upright - it ends well before the screen does)
    float bodyAlong = std::min(screenAlong - margin, rubberAlong + kMaxBody * u);
    g.bodyR = std::min(0.6f * g.body + 0.2f * g.rubber, bodyAlong);
    float bodyAcross = screenAcross + g.bodyR + 2.0f;
    g.cavityHW = g.sides ? cavityAlong : cavityAcross;
    g.cavityHH = g.sides ? cavityAcross : cavityAlong;
    g.rubberHW = g.sides ? rubberAlong : rubberAcross;
    g.rubberHH = g.sides ? rubberAcross : rubberAlong;
    g.bodyHW = g.sides ? bodyAlong : bodyAcross;
    g.bodyHH = g.sides ? bodyAcross : bodyAlong;
    // The nose's notch (shown only below the menu - portrait).
    g.noseR = 0.95f * g.rubber;
    g.noseY = g.cy + g.rubberHH + 0.25f * g.rubber;
    g.shadow = std::max(0.05f * u, 1.0f);
    return g;
}

std::vector<uint8_t> VirtualBoyFrame::Make(const Geometry &screen, float w, float h, int texW, int texH)
{
    // Everything in the picture's pixels.
    const float k = texW / w;
    Geometry g = screen;
    for (float *v : {&g.cx, &g.cavityHW, &g.cavityHH, &g.cavityR, &g.rubberHW, &g.rubberHH, &g.rubberR, &g.rubber,
                     &g.noseY, &g.noseR, &g.bodyHW, &g.bodyHH, &g.bodyR, &g.body, &g.shadow})
        *v *= k;
    g.cy = screen.cy * texH / h;
    g.noseY = screen.noseY * texH / h;
    const float pad = (screen.sides ? screen.cavityHW - kMenuWidth * screen.menuScale / 2.0f
                                    : screen.cavityHH - kMenuHeight * screen.menuScale / 2.0f) * k;

    auto bodyD = [&](float x, float y) { return RoundRect(x, y, g.cx, g.cy, g.bodyHW, g.bodyHH, g.bodyR); };
    auto rubberOutline = [&](float x, float y) { return RoundRect(x, y, g.cx, g.cy, g.rubberHW, g.rubberHH, g.rubberR); };
    auto rubberD = [&](float x, float y)
    {
        const float outline = rubberOutline(x, y);
        return g.sides ? outline : std::max(outline, -(std::hypot(x - g.cx, y - g.noseY) - g.noseR));
    };
    auto cavityD = [&](float x, float y) { return RoundRect(x, y, g.cx, g.cy, g.cavityHW, g.cavityHH, g.cavityR); };
    auto normal = [](auto &&f, float x, float y, float &nx, float &ny)
    {
        nx = f(x + 1.0f, y) - f(x - 1.0f, y);
        ny = f(x, y + 1.0f) - f(x, y - 1.0f);
        const float n = std::hypot(nx, ny) + 1e-9f;
        nx /= n;
        ny /= n;
    };

    constexpr Rgb kRoom{12, 11, 13}, kRed{205, 37, 57}, kRubberColor{34, 33, 37}, kInside{4, 4, 6};
    std::vector<uint8_t> bgra(static_cast<size_t>(texW) * texH * 4);
    for (int iy = 0; iy < texH; ++iy)
        for (int ix = 0; ix < texW; ++ix)
        {
            const float x = ix + 0.5f, y = iy + 0.5f;
            uint8_t *p = &bgra[(static_cast<size_t>(iy) * texW + ix) * 4];
            if (cavityD(x, y) <= -(0.9f * pad + 4.0f)) // (well inside: the menu's there - quick)
            {
                p[0] = static_cast<uint8_t>(kInside.b);
                p[1] = static_cast<uint8_t>(kInside.g);
                p[2] = static_cast<uint8_t>(kInside.r);
                p[3] = 255;
                continue;
            }
            // The room, the body's shadow on it.
            Rgb c = kRoom;
            const float onRoom = std::pow(Smooth01(1.0f - std::max(bodyD(x, y - 0.4f * g.shadow), 0.0f) / g.shadow), 2.0f);
            c = Mix(c, {0, 0, 0}, 0.6f * onRoom);

            // The red body: plastic, lit from above, darker down.
            const float db = bodyD(x, y);
            if (db < 1.0f)
            {
                float nx, ny, light, spec;
                normal(bodyD, x, y, nx, ny);
                Bevel(db, std::min(0.45f * g.body, kMaxBevel * std::min(texW, texH)), nx, ny, 0.35f, light, spec);
                const float down = 1.08f - 0.22f * Smooth01(y / texH);
                // (the eyeshade's shadow on it)
                const float under = std::pow(
                    Smooth01(1.0f - std::max(rubberOutline(x, y - 0.15f * g.rubber), 0.0f) / (0.6f * g.rubber)), 2.0f);
                const float f = light * down * (1.0f - 0.45f * under);
                const Rgb red{kRed.r * f + 255.0f * spec, kRed.g * f + 255.0f * spec, kRed.b * f + 255.0f * spec};
                c = Mix(c, red, Cover(db));
            }

            // The eyeshade: black rubber, its inner lip curving into the dark.
            const float dr = rubberD(x, y);
            if (dr < 1.0f)
            {
                float nx, ny, light, spec;
                normal(rubberD, x, y, nx, ny);
                Bevel(dr, 0.6f * g.rubber, nx, ny, 0.25f, light, spec);
                const float lip = Smooth01(1.0f + cavityD(x, y) / (0.5f * g.rubber));
                const float f = light * (1.0f - 0.5f * lip);
                const Rgb rubber{kRubberColor.r * f + 255.0f * spec, kRubberColor.g * f + 255.0f * spec,
                                 kRubberColor.b * f + 255.0f * spec};
                c = Mix(c, rubber, Cover(dr));
            }

            // The dark inside (the menu goes over it).
            const float dc = cavityD(x, y);
            if (dc < 1.0f)
            {
                const float depth = Smooth01(1.0f + dc / (0.9f * pad + 4.0f));
                const float f = 1.0f + 1.5f * (1.0f - depth);
                c = Mix(c, {kInside.r * f, kInside.g * f, kInside.b * f}, Cover(dc));
            }

            p[0] = static_cast<uint8_t>(std::clamp(c.b, 0.0f, 255.0f) + 0.5f);
            p[1] = static_cast<uint8_t>(std::clamp(c.g, 0.0f, 255.0f) + 0.5f);
            p[2] = static_cast<uint8_t>(std::clamp(c.r, 0.0f, 255.0f) + 0.5f);
            p[3] = 255;
        }
    return bgra;
}

void VirtualBoyFrame::Update(UiRenderer &ui, float w, float h)
{
    const int iw = static_cast<int>(w), ih = static_cast<int>(h);
    if (iw <= 0 || ih <= 0)
        return;

    // A picture finished: upload it.
    if (m_making.valid() && m_making.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        Made made = m_making.get();
        // (a texture of this size already made, for a screen size no longer shown, is reused)
        for (Made &old : m_made)
            if (old.texW == made.texW && old.texH == made.texH && (old.w != iw || old.h != ih))
            {
                made.image = old.image;
                old = Made{};
                break;
            }
        if (!made.image.IsValid())
            made.image = ui.CreateStreamingImage(static_cast<uint32_t>(made.texW), static_cast<uint32_t>(made.texH),
                                                 VK_FORMAT_B8G8R8A8_SRGB);
        ui.UpdateStreamingImage(made.image, made.bgra.data(), made.bgra.size());
        made.bgra.clear();
        made.bgra.shrink_to_fit();
        m_made.erase(std::remove_if(m_made.begin(), m_made.end(), [](const Made &m) { return !m.image.IsValid(); }),
                     m_made.end());
        m_made.push_back(std::move(made));
    }

    m_shown = -1;
    for (size_t i = 0; i < m_made.size(); ++i)
        if (m_made[i].w == iw && m_made[i].h == ih)
            m_shown = static_cast<int>(i);
    if (m_shown >= 0 || (m_making.valid() && m_makingW == iw && m_makingH == ih))
        return;
    if (m_making.valid())
        return; // (another size first)

    // Make it.
    const float fit = std::min(1.0f, static_cast<float>(kMaxTexture) / std::max(w, h));
    const int texW = std::max(1, static_cast<int>(std::lround(w * fit))), texH = std::max(1, static_cast<int>(std::lround(h * fit)));
    m_makingW = iw;
    m_makingH = ih;
    m_fadeIn = 0.0f;
    m_making = std::async(std::launch::async,
                          [iw, ih, texW, texH]
                          {
                              Made made;
                              made.w = iw;
                              made.h = ih;
                              made.texW = texW;
                              made.texH = texH;
                              const float fw = static_cast<float>(iw), fh = static_cast<float>(ih);
                              made.bgra = Make(Layout(fw, fh), fw, fh, texW, texH);
                              return made;
                          });
}

void VirtualBoyFrame::Draw(UiRenderer &ui, float w, float h, float alpha)
{
    if (m_shown < 0)
        return;
    m_fadeIn = std::min(1.0f, m_fadeIn + 0.125f);
    const Made &made = m_made[static_cast<size_t>(m_shown)];
    ui.DrawImage(made.image, 0.0f, 0.0f, w, h, {1.0f, 1.0f, 1.0f, alpha * m_fadeIn});
}
