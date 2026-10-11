#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Platform;

// Is there a newer VBoy Color? Asks GitHub for the project's releases
// (github.com/yuk27/VBoyColor), picks the newest one this app can take,
// and - when the player says so - downloads its package and hands it to the
// platform to put in place (Platform::InstallUpdate: the desktop apps
// replace their files and restart; Android hands the APK to the system's
// installer).
//
// Which releases count: a full release (v1.0.0) looks only at full
// releases; a beta (v1.0.0-beta.5) also at betas. A development build
// (v1.0.0-dev.208, built from the source) never checks on its own; asked,
// it shows the newest release and can switch to it.
class Updater
{
public:
    enum class State
    {
        Idle,
        Checking,
        UpToDate,
        Available,   // a newer release (Latest*)
        Downloading,
        Installed,   // in place, or with the installer (Status says what's next)
        Failed,      // the check or the update (Status says which)
    };

    // currentVersion: this build's (kVersionString - "v1.0.0-beta.5").
    void Init(Platform &platform, const std::string &currentVersion);

    // Asks GitHub (in the background - see Update).
    void Check();
    // Downloads the newest release's package and installs it.
    void StartUpdate();
    // Once per app frame: follows the downloads.
    void Update();

    State GetState() const { return m_state; }
    // What happened, for the menu ("" when there's nothing to say).
    const std::string &Status() const { return m_status; }
    // The newest release (once a check found one): "1.0.0 beta 6", its page.
    const std::string &LatestName() const { return m_latestName; }
    const std::string &LatestPage() const { return m_latestPage; }
    // A newer release is out (also while it downloads).
    bool UpdateAvailable() const { return m_state == State::Available || m_state == State::Downloading; }
    bool IsDevBuild() const { return m_devBuild; }
    // This platform can check at all (it downloads, and has a package).
    bool Supported() const;

    // Versions: "v1.0.0-beta.5" < "v1.0.0-beta.10" < "v1.0.0" < "v1.0.1-beta.1".
    static int CompareVersions(const std::string &a, const std::string &b);
    // "v1.0.0-beta.6" -> "1.0.0 beta 6".
    static std::string DisplayName(const std::string &version);

private:
    // Picks the newest release this app can take out of GitHub's answer.
    void Choose(const std::string &json);

    Platform *m_platform = nullptr;
    std::string m_current;
    bool m_devBuild = false, m_preRelease = false;
    State m_state = State::Idle;
    std::string m_status;
    std::string m_latestName, m_latestPage, m_latestAsset;
    uint32_t m_frames = 0;
};
