#include "io/Updater.h"
#include "io/MiniJson.h"
#include "io/Platform.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
    // The project's releases, newest first (GitHub's API needs no sign-in for
    // this - 60 requests an hour per address, far more than one check a start).
    constexpr const char *kReleasesUrl = "https://api.github.com/repos/yuk27/VBoyColor/releases?per_page=15";

    struct Version
    {
        int core[3] = {0, 0, 0};
        std::vector<std::string> pre; // "beta", "5" - empty for a full release
        bool valid = false;
    };

    Version Parse(std::string text)
    {
        Version v;
        if (!text.empty() && (text[0] == 'v' || text[0] == 'V'))
            text.erase(0, 1);
        const size_t dash = text.find('-');
        const std::string core = text.substr(0, dash);
        int n = 0, part = 0;
        bool digits = false;
        for (char c : core + ".")
        {
            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                n = n * 10 + (c - '0');
                digits = true;
            }
            else if (c == '.' && digits && part < 3)
            {
                v.core[part++] = n;
                n = 0;
                digits = false;
            }
            else
                return v;
        }
        v.valid = part == 3;
        if (dash != std::string::npos)
        {
            std::string pre = text.substr(dash + 1);
            const size_t dirty = pre.find("-dirty");
            if (dirty != std::string::npos)
                pre.erase(dirty);
            size_t start = 0;
            for (;;)
            {
                const size_t dot = pre.find('.', start);
                v.pre.push_back(pre.substr(start, dot - start));
                if (dot == std::string::npos)
                    break;
                start = dot + 1;
            }
        }
        return v;
    }

    bool Numeric(const std::string &s)
    {
        if (s.empty())
            return false;
        for (char c : s)
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return false;
        return true;
    }
} // namespace

int Updater::CompareVersions(const std::string &a, const std::string &b)
{
    const Version x = Parse(a), y = Parse(b);
    for (int i = 0; i < 3; ++i)
        if (x.core[i] != y.core[i])
            return x.core[i] < y.core[i] ? -1 : 1;
    // A pre-release comes before its full release.
    if (x.pre.empty() != y.pre.empty())
        return x.pre.empty() ? 1 : -1;
    for (size_t i = 0; i < x.pre.size() && i < y.pre.size(); ++i)
    {
        const std::string &p = x.pre[i], &q = y.pre[i];
        if (p == q)
            continue;
        const bool pn = Numeric(p), qn = Numeric(q);
        if (pn && qn)
            return std::stoll(p) < std::stoll(q) ? -1 : 1;
        if (pn != qn)
            return pn ? -1 : 1; // (numbers before words)
        return p < q ? -1 : 1;
    }
    if (x.pre.size() != y.pre.size())
        return x.pre.size() < y.pre.size() ? -1 : 1;
    return 0;
}

std::string Updater::DisplayName(const std::string &version)
{
    const Version v = Parse(version);
    if (!v.valid)
        return version;
    std::string name = std::to_string(v.core[0]) + "." + std::to_string(v.core[1]) + "." + std::to_string(v.core[2]);
    for (const std::string &part : v.pre)
        name += " " + part;
    return name;
}

void Updater::Init(Platform &platform, const std::string &currentVersion)
{
    m_platform = &platform;
    m_current = currentVersion;
    const Version v = Parse(currentVersion);
    m_preRelease = !v.pre.empty();
    m_devBuild = m_preRelease && v.pre[0] == "dev";
}

bool Updater::Supported() const
{
    return m_platform && *m_platform->UpdatePackage();
}

void Updater::Check()
{
    if (!Supported() || m_state == State::Checking || m_state == State::Downloading || m_state == State::Installed)
        return;
    if (!m_platform->StartDownload(kReleasesUrl, Platform::kDownloadUpdates))
        return; // (busy - try again)
    m_state = State::Checking;
    m_status.clear();
}

void Updater::StartUpdate()
{
    if (m_latestAsset.empty() || m_state == State::Downloading || m_state == State::Checking)
        return;
    if (!m_platform->StartDownload(m_latestAsset, Platform::kDownloadUpdates))
        return;
    m_state = State::Downloading;
    m_status.clear();
}

void Updater::Choose(const std::string &json)
{
    JsonValue releases;
    if (!ParseJson(json, releases) || releases.type != JsonValue::Type::Array)
    {
        m_state = State::Failed;
        m_status = "Couldn't check";
        return;
    }
    const std::string package = m_platform->UpdatePackage();
    std::string bestTag, bestName, bestPage, bestAsset;
    for (const JsonValue &release : releases.array)
    {
        if (release.Bool("draft") || (release.Bool("prerelease") && !m_preRelease))
            continue;
        const std::string tag = release.Str("tag_name");
        if (!Parse(tag).valid || (!bestTag.empty() && CompareVersions(tag, bestTag) <= 0))
            continue;
        // This app's package among its downloads.
        std::string asset;
        if (const JsonValue *assets = release.Get("assets"))
            for (const JsonValue &a : assets->array)
            {
                const std::string name = a.Str("name");
                auto endsWith = [&](const char *end)
                {
                    const size_t n = std::strlen(end);
                    return name.size() >= n && name.compare(name.size() - n, n, end) == 0;
                };
                const bool fits = package == "windows"  ? name.find("-windows-") != std::string::npos && endsWith(".zip")
                                  : package == "linux"  ? name.find("-linux-") != std::string::npos && endsWith(".tar.gz")
                                  : package == "android" ? endsWith(".apk")
                                                         : false;
                if (fits)
                    asset = a.Str("browser_download_url");
            }
        if (asset.empty())
            continue;
        bestTag = tag;
        bestPage = release.Str("html_url");
        bestAsset = asset;
    }
    if (bestTag.empty())
    {
        m_state = State::UpToDate;
        m_status.clear();
        return;
    }
    m_latestName = DisplayName(bestTag);
    m_latestPage = bestPage;
    m_latestAsset = bestAsset;
    // A development build is "newer" than any release of its version - it
    // gets the newest release offered anyway, when asked.
    const bool newer = m_devBuild || CompareVersions(bestTag, m_current) > 0;
    m_state = newer ? State::Available : State::UpToDate;
    m_status = newer && m_devBuild ? "This is a development build" : "";
}

void Updater::Update()
{
    if (!m_platform)
        return;
    if (m_state == State::Checking || m_state == State::Downloading)
    {
        std::vector<uint8_t> bytes;
        const int state = m_platform->PollDownload(bytes, Platform::kDownloadUpdates);
        if (state == 0)
            return;
        if (m_state == State::Checking)
        {
            if (state < 0)
            {
                m_state = State::Failed;
                m_status = "Couldn't check - no connection?";
            }
            else
                Choose(std::string(bytes.begin(), bytes.end()));
            return;
        }
        if (state < 0)
        {
            m_state = State::Failed;
            m_status = "The download failed";
            return;
        }
        bool installed = false;
        m_status = m_platform->InstallUpdate(bytes, installed);
        m_state = installed ? State::Installed : State::Failed;
        return;
    }
    // (Android's installer answers later - asked now and then)
    if (++m_frames % 30 != 0)
        return;
    const std::string later = m_platform->TakeUpdateStatus();
    if (!later.empty())
        m_status = later;
}
