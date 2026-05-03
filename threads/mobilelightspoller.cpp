#include "mobilelightspoller.h"

#include "../storage/common.h"

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <iterator>
namespace {
void normalizeLEDOrder(std::vector<LEDData> &leds) {
  std::sort(leds.begin(), leds.end(), [](const LEDData &a, const LEDData &b) {
    return a.redPin < b.redPin;
  });
}

void normalizeScheduleOrder(std::vector<Schedule> &schedules) {
  std::sort(schedules.begin(), schedules.end(),
            [](const Schedule &a, const Schedule &b) {
              if (a.themeID != b.themeID)
                return a.themeID < b.themeID;
              return a.name < b.name;
            });
}

void normalizeThemeOrder(std::vector<Theme> &themes) {
  std::sort(themes.begin(), themes.end(),
            [](const Theme &a, const Theme &b) { return a.id < b.id; });
}

void normalizePatternOrder(std::vector<Pattern> &patterns) {
  std::sort(patterns.begin(), patterns.end(),
            [](const Pattern &a, const Pattern &b) { return a.id < b.id; });
}

void normalizeTeamOrder(std::vector<TeamRecord> &teams) {
  std::sort(teams.begin(), teams.end(), [](const TeamRecord &a,
                                           const TeamRecord &b) {
    if (a.id != b.id)
      return a.id < b.id;
    return a.teamCode < b.teamCode;
  });
}

bool themesEqual(const std::vector<Theme> &a, const std::vector<Theme> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].id != b[i].id || a[i].name != b[i].name ||
        a[i].fileName != b[i].fileName ||
        a[i].colors.size() != b[i].colors.size()) {
      return false;
    }
    for (size_t c = 0; c < a[i].colors.size(); ++c) {
      if (a[i].colors[c].r != b[i].colors[c].r ||
          a[i].colors[c].g != b[i].colors[c].g ||
          a[i].colors[c].b != b[i].colors[c].b) {
        return false;
      }
    }
  }
  return true;
}

bool patternsEqual(const std::vector<Pattern> &a,
                   const std::vector<Pattern> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].id != b[i].id || a[i].speed != b[i].speed)
      return false;
  }
  return true;
}

bool teamsEqual(const std::vector<TeamRecord> &a,
                const std::vector<TeamRecord> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].id != b[i].id || a[i].name != b[i].name ||
        a[i].league != b[i].league || a[i].teamCode != b[i].teamCode ||
        a[i].homeAway != b[i].homeAway ||
        a[i].nextGameUrlTemplate != b[i].nextGameUrlTemplate ||
        a[i].nextGameParser != b[i].nextGameParser ||
        a[i].liveGameUrlTemplate != b[i].liveGameUrlTemplate ||
        a[i].liveGameParser != b[i].liveGameParser ||
        a[i].apiTeamId != b[i].apiTeamId || a[i].enabled != b[i].enabled ||
        a[i].displayOrder != b[i].displayOrder ||
        a[i].themeName != b[i].themeName || a[i].themeID != b[i].themeID ||
        a[i].iconPath != b[i].iconPath ||
        a[i].nextGameUtc != b[i].nextGameUtc ||
        a[i].lastHomeScore != b[i].lastHomeScore ||
        a[i].lastAwayScore != b[i].lastAwayScore ||
        a[i].scoreAnimationDelaySeconds != b[i].scoreAnimationDelaySeconds ||
        a[i].lastGameId != b[i].lastGameId ||
        a[i].lastCheckedUtc != b[i].lastCheckedUtc ||
        a[i].nextOpponentCode != b[i].nextOpponentCode ||
        a[i].nextOpponentName != b[i].nextOpponentName ||
        a[i].colors.size() != b[i].colors.size()) {
      return false;
    }
    for (size_t c = 0; c < a[i].colors.size(); ++c) {
      if (a[i].colors[c].colorRole != b[i].colors[c].colorRole ||
          a[i].colors[c].r != b[i].colors[c].r ||
          a[i].colors[c].g != b[i].colors[c].g ||
          a[i].colors[c].b != b[i].colors[c].b ||
          a[i].colors[c].displayOrder != b[i].colors[c].displayOrder) {
        return false;
      }
    }
  }
  return true;
}
} // namespace

MobileLightsPoller::MobileLightsPoller(const std::vector<LEDData> &ledInfo,
                                       const Options &options,
                                       const std::vector<Schedule> &schedule,
                                       std::string baseUrl)
    : m_ledInfo(ledInfo), m_options(options), m_schedule(schedule),
      m_baseUrl(normalizeBaseUrl(std::move(baseUrl))) {
  normalizeLEDOrder(m_ledInfo);
  normalizeScheduleOrder(m_schedule);
  m_apiUrl = m_baseUrl + "index.php";
  m_allUrl = m_apiUrl + "?action=get_all";
  m_ledUrl = m_apiUrl + "?action=update&domain=led";
  m_optUrl = m_apiUrl + "?action=update&domain=options";
  m_schUrl = m_apiUrl + "?action=update&domain=schedules";
  m_themesUrl = m_apiUrl + "?action=update&domain=themes";
  m_patternsUrl = m_apiUrl + "?action=update&domain=patterns";
  m_teamsUrl = m_apiUrl + "?action=update&domain=teams";
  m_dispatcher.connect(sigc::mem_fun(*this, &MobileLightsPoller::onDispatch));
}

MobileLightsPoller::~MobileLightsPoller() { stop(); }

void MobileLightsPoller::start() {
  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true)) {
    return;
  }

  m_thread = std::thread(&MobileLightsPoller::threadMain, this);
}

void MobileLightsPoller::stop() {
  bool expected = true;
  if (!m_running.compare_exchange_strong(expected, false)) {
    return;
  }

  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void MobileLightsPoller::pushLedUpdate(const std::vector<LEDData> &ledInfo) {
  std::vector<LEDData> snapshot = ledInfo;
  normalizeLEDOrder(snapshot);
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ledInfo = snapshot;
    m_pendingLedEcho = true;
  }

  const std::string url = m_ledUrl;
  LOG_INFO() << "MobileLightsPoller: local LED push started";
  std::thread([url, snapshot]() {
    HttpHelper http;
    const std::string response = http.sendLEDs(url, snapshot, DEVICE);
    if (response.empty()) {
      LOG_WARN() << "MobileLightsPoller: local LED push returned empty response";
    }
    LOG_INFO() << "MobileLightsPoller: local LED push completed response="
               << response;
  }).detach();
}

void MobileLightsPoller::pushOptionsUpdate(const Options &options) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options = options;
    m_pendingOptionsEcho = true;
  }

  const std::string url = m_optUrl;
  LOG_INFO() << "MobileLightsPoller: local options push started";
  std::thread([url, options]() {
    HttpHelper http;
    const std::string response = http.sendOptions(url, options, DEVICE);
    if (response.empty()) {
      LOG_WARN()
          << "MobileLightsPoller: local options push returned empty response";
    }
    LOG_INFO() << "MobileLightsPoller: local options push completed response="
               << response;
  }).detach();
}

void MobileLightsPoller::pushScheduleUpdate(
    const std::vector<Schedule> &schedule) {
  std::vector<Schedule> snapshot = schedule;
  normalizeScheduleOrder(snapshot);
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_schedule = snapshot;
    m_pendingScheduleEcho = true;
  }

  const std::string url = m_schUrl;
  LOG_INFO() << "MobileLightsPoller: local schedule push started";
  std::thread([url, snapshot]() {
    HttpHelper http;
    const std::string response = http.sendSchedules(url, snapshot, DEVICE);
    if (response.empty()) {
      LOG_WARN()
          << "MobileLightsPoller: local schedule push returned empty response";
    }
    LOG_INFO() << "MobileLightsPoller: local schedule push completed response="
               << response;
  }).detach();
}

void MobileLightsPoller::pushThemesUpdate(const std::vector<Theme> &themes) {
  std::vector<Theme> snapshot = themes;
  normalizeThemeOrder(snapshot);
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_themes = snapshot;
    m_pendingThemesEcho = true;
    m_themesInitialized = true;
  }

  const std::string url = m_themesUrl;
  LOG_INFO() << "MobileLightsPoller: local themes push started";
  std::thread([url, snapshot]() {
    HttpHelper http;
    const std::string response = http.sendThemes(url, snapshot, DEVICE);
    if (response.empty()) {
      LOG_WARN()
          << "MobileLightsPoller: local themes push returned empty response";
    }
    LOG_INFO() << "MobileLightsPoller: local themes push completed response="
               << response;
  }).detach();
}

void MobileLightsPoller::pushPatternsUpdate(
    const std::vector<Pattern> &patterns) {
  std::vector<Pattern> snapshot = patterns;
  normalizePatternOrder(snapshot);
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patterns = snapshot;
    m_pendingPatternsEcho = true;
    m_patternsInitialized = true;
  }

  const std::string url = m_patternsUrl;
  LOG_INFO() << "MobileLightsPoller: local patterns push started";
  std::thread([url, snapshot]() {
    HttpHelper http;
    const std::string response = http.sendPatterns(url, snapshot, DEVICE);
    if (response.empty()) {
      LOG_WARN()
          << "MobileLightsPoller: local patterns push returned empty response";
    }
    LOG_INFO() << "MobileLightsPoller: local patterns push completed response="
               << response;
  }).detach();
}

void MobileLightsPoller::pushTeamsUpdate(
    const std::vector<TeamRecord> &teams) {
  std::vector<TeamRecord> snapshot = teams;
  normalizeTeamOrder(snapshot);
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_teams = snapshot;
    m_pendingTeamsEcho = true;
    m_teamsInitialized = true;
  }

  const std::string url = m_teamsUrl;
  LOG_INFO() << "MobileLightsPoller: local teams push started";
  std::thread([url, snapshot]() {
    HttpHelper http;
    const std::string response = http.sendTeams(url, snapshot, DEVICE);
    if (response.empty()) {
      LOG_WARN()
          << "MobileLightsPoller: local teams push returned empty response";
    }
    LOG_INFO() << "MobileLightsPoller: local teams push completed response="
               << response;
  }).detach();
}

void MobileLightsPoller::setExtendedSnapshots(
    const std::vector<Theme> &themes, const std::vector<Pattern> &patterns,
    const std::vector<TeamRecord> &teams) {
  std::vector<Theme> themesCopy = themes;
  std::vector<Pattern> patternsCopy = patterns;
  std::vector<TeamRecord> teamsCopy = teams;
  normalizeThemeOrder(themesCopy);
  normalizePatternOrder(patternsCopy);
  normalizeTeamOrder(teamsCopy);

  std::lock_guard<std::mutex> lock(m_mutex);
  m_themes = std::move(themesCopy);
  m_patterns = std::move(patternsCopy);
  m_teams = std::move(teamsCopy);
  m_themesInitialized = true;
  m_patternsInitialized = true;
  m_teamsInitialized = true;
}

void MobileLightsPoller::requestStartupSync(
    const std::vector<LEDData> &ledInfo, const Options &options,
    const std::vector<Schedule> &schedule, const std::vector<Theme> &themes,
    const std::vector<Pattern> &patterns,
    const std::vector<TeamRecord> &teams) {
  std::vector<LEDData> ledCopy = ledInfo;
  std::vector<Schedule> scheduleCopy = schedule;
  std::vector<Theme> themesCopy = themes;
  std::vector<Pattern> patternsCopy = patterns;
  std::vector<TeamRecord> teamsCopy = teams;
  normalizeLEDOrder(ledCopy);
  normalizeScheduleOrder(scheduleCopy);
  normalizeThemeOrder(themesCopy);
  normalizePatternOrder(patternsCopy);
  normalizeTeamOrder(teamsCopy);

  std::lock_guard<std::mutex> lock(m_mutex);
  m_ledInfo = std::move(ledCopy);
  m_options = options;
  m_schedule = std::move(scheduleCopy);
  m_themes = std::move(themesCopy);
  m_patterns = std::move(patternsCopy);
  m_teams = std::move(teamsCopy);
  m_themesInitialized = true;
  m_patternsInitialized = true;
  m_teamsInitialized = true;
  m_pendingLedEcho = true;
  m_pendingOptionsEcho = true;
  m_pendingScheduleEcho = true;
  m_pendingThemesEcho = true;
  m_pendingPatternsEcho = true;
  m_pendingTeamsEcho = true;
  m_startupSyncPending = true;
}

Options MobileLightsPoller::optionsSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_options;
}

std::vector<LEDData> MobileLightsPoller::ledInfoSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_ledInfo;
}

std::vector<Schedule> MobileLightsPoller::scheduleSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_schedule;
}

std::vector<Theme> MobileLightsPoller::themesSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_themes;
}

std::vector<Pattern> MobileLightsPoller::patternsSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_patterns;
}

std::vector<TeamRecord> MobileLightsPoller::teamsSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_teams;
}

sigc::signal<void> &MobileLightsPoller::signal_changed() {
  return m_signalChanged;
}

sigc::signal<void, Options> &MobileLightsPoller::signal_options_changed() {
  return m_signalOptionsChanged;
}

sigc::signal<void, std::vector<LEDData>> &
MobileLightsPoller::signal_leds_changed() {
  return m_signalLEDsChanged;
}

sigc::signal<void, std::vector<Schedule>> &
MobileLightsPoller::signal_schedules_changed() {
  return m_signalSchedulesChanged;
}

sigc::signal<void, std::vector<Theme>> &
MobileLightsPoller::signal_themes_changed() {
  return m_signalThemesChanged;
}

sigc::signal<void, std::vector<Pattern>> &
MobileLightsPoller::signal_patterns_changed() {
  return m_signalPatternsChanged;
}

sigc::signal<void, std::vector<TeamRecord>> &
MobileLightsPoller::signal_teams_changed() {
  return m_signalTeamsChanged;
}

sigc::signal<void> &MobileLightsPoller::signal_startup_sync_complete() {
  return m_signalStartupSyncComplete;
}

void MobileLightsPoller::threadMain() {
  while (m_running.load()) {
    try {
      bool startupSyncPending = false;
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        startupSyncPending = m_startupSyncPending;
      }

      if (startupSyncPending) {
        LOG_INFO() << "Startup sync: pushing local state to server";
        if (performStartupSync()) {
          LOG_INFO() << "Startup sync complete";
          m_startupSyncCompletePending.store(true);
          m_dispatcher.emit();
        }
      }

      {
        std::lock_guard<std::mutex> lock(m_mutex);
        startupSyncPending = m_startupSyncPending;
      }
      if (startupSyncPending) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        continue;
      }

      bool anyChanged = false;
      LOG_INFO() << "MobileLightsPoller: poll cycle started";

      if (pollAll()) {
        anyChanged = true;
      }

      if (anyChanged) {
        m_changedPending.store(true);
        m_dispatcher.emit();
      }
      LOG_INFO() << "MobileLightsPoller: poll cycle completed changed="
                 << (anyChanged ? "true" : "false");
    } catch (const std::exception &e) {
      LOG_ERROR() << "MobileLightsPoller exception: " << e.what();
    } catch (...) {
      LOG_ERROR() << "MobileLightsPoller unknown exception";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

bool MobileLightsPoller::performStartupSync() {
  std::vector<LEDData> leds;
  Options options;
  std::vector<Schedule> schedules;
  std::vector<Theme> themes;
  std::vector<Pattern> patterns;
  std::vector<TeamRecord> teams;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_startupSyncPending)
      return false;

    leds = m_ledInfo;
    options = m_options;
    schedules = m_schedule;
    themes = m_themes;
    patterns = m_patterns;
    teams = m_teams;
  }

  HttpHelper http;
  bool ok = true;
  ok &= !http.sendLEDs(m_ledUrl, leds, DEVICE).empty();
  ok &= !http.sendOptions(m_optUrl, options, DEVICE).empty();
  ok &= !http.sendSchedules(m_schUrl, schedules, DEVICE).empty();
  ok &= !http.sendThemes(m_themesUrl, themes, DEVICE).empty();
  ok &= !http.sendPatterns(m_patternsUrl, patterns, DEVICE).empty();
  ok &= !http.sendTeams(m_teamsUrl, teams, DEVICE).empty();

  if (!ok) {
    LOG_WARN() << "Startup sync: server unreachable, will retry";
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_startupSyncPending = false;
  return true;
}

bool MobileLightsPoller::pollAll() {
  const std::string response = m_http.get(m_allUrl);
  if (response.empty())
    return false;

  json j = json::parse(response, nullptr, false);
  if (j.is_discarded() || !j.is_object())
    return false;

  bool anyChanged = false;
  Options newOptions{};
  std::vector<LEDData> newLEDs;
  std::vector<Schedule> newSchedules;
  std::vector<Theme> newThemes;
  std::vector<Pattern> newPatterns;
  std::vector<TeamRecord> newTeams;

  if (j.contains("options") &&
      parseOptionsJson(j["options"].dump(), newOptions)) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (newOptions == m_options && m_pendingOptionsEcho) {
      m_pendingOptionsEcho = false;
      LOG_INFO() << "MobileLightsPoller: local options echo ignored and cleared";
    } else if (newOptions != m_options) {
      m_options = newOptions;
      m_pendingOptionsEcho = false;
      m_optionsChangedPending.store(true);
      anyChanged = true;
      LOG_INFO() << "MobileLightsPoller: remote options changes detected";
    }
  }

  if (j.contains("leds") && parseLEDsJson(j["leds"].dump(), newLEDs)) {
    normalizeLEDOrder(newLEDs);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (newLEDs == m_ledInfo && m_pendingLedEcho) {
      m_pendingLedEcho = false;
      LOG_INFO() << "MobileLightsPoller: local LED echo ignored and cleared";
    } else if (newLEDs != m_ledInfo) {
      m_ledInfo = newLEDs;
      m_pendingLedEcho = false;
      m_ledsChangedPending.store(true);
      anyChanged = true;
      LOG_INFO() << "MobileLightsPoller: remote LED changes detected";
    }
  }

  if (j.contains("schedules") &&
      parseSchedulesJson(j["schedules"].dump(), newSchedules)) {
    normalizeScheduleOrder(newSchedules);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (newSchedules == m_schedule && m_pendingScheduleEcho) {
      m_pendingScheduleEcho = false;
      LOG_INFO()
          << "MobileLightsPoller: local schedule echo ignored and cleared";
    } else if (newSchedules != m_schedule) {
      m_schedule = newSchedules;
      m_pendingScheduleEcho = false;
      m_schedulesChangedPending.store(true);
      anyChanged = true;
      LOG_INFO() << "MobileLightsPoller: remote schedule changes detected";
    }
  }

  if (j.contains("themes") && parseThemesJson(j["themes"].dump(), newThemes)) {
    normalizeThemeOrder(newThemes);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (themesEqual(newThemes, m_themes) && m_pendingThemesEcho) {
      m_pendingThemesEcho = false;
      LOG_INFO() << "MobileLightsPoller: local themes echo ignored and cleared";
    } else if (!themesEqual(newThemes, m_themes)) {
      m_themes = newThemes;
      m_pendingThemesEcho = false;
      m_themesChangedPending.store(true);
      anyChanged = true;
      LOG_INFO() << "MobileLightsPoller: remote themes changes detected";
    }
  }

  if (j.contains("patterns") &&
      parsePatternsJson(j["patterns"].dump(), newPatterns)) {
    normalizePatternOrder(newPatterns);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (patternsEqual(newPatterns, m_patterns) && m_pendingPatternsEcho) {
      m_pendingPatternsEcho = false;
      LOG_INFO()
          << "MobileLightsPoller: local patterns echo ignored and cleared";
    } else if (!patternsEqual(newPatterns, m_patterns)) {
      m_patterns = newPatterns;
      m_pendingPatternsEcho = false;
      m_patternsChangedPending.store(true);
      anyChanged = true;
      LOG_INFO() << "MobileLightsPoller: remote patterns changes detected";
    }
  }

  if (j.contains("teams") && parseTeamsJson(j["teams"].dump(), newTeams)) {
    normalizeTeamOrder(newTeams);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (teamsEqual(newTeams, m_teams) && m_pendingTeamsEcho) {
      m_pendingTeamsEcho = false;
      LOG_INFO() << "MobileLightsPoller: local teams echo ignored and cleared";
    } else if (!teamsEqual(newTeams, m_teams)) {
      m_teams = newTeams;
      m_pendingTeamsEcho = false;
      m_teamsChangedPending.store(true);
      anyChanged = true;
      LOG_INFO() << "MobileLightsPoller: remote teams changes detected";
    }
  }

  return anyChanged;
}

bool MobileLightsPoller::pollOptions() {
  const std::string response =
      m_http.get(m_apiUrl + "?action=get&domain=options");
  if (response.empty()) {
    return false;
  }

  Options newOptions{};
  if (!parseOptionsJson(response, newOptions)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  if (newOptions == m_options && m_pendingOptionsEcho) {
    m_pendingOptionsEcho = false;
    LOG_INFO() << "MobileLightsPoller: local options echo ignored and cleared";
    return false;
  }

  if (newOptions != m_options) {
    m_options = newOptions;
    m_pendingOptionsEcho = false;
    m_optionsChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: remote options changes detected";
    return true;
  }

  return false;
}

bool MobileLightsPoller::pollLEDs() {
  const std::string response = m_http.get(m_apiUrl + "?action=get&domain=led");
  if (response.empty()) {
    return false;
  }

  std::vector<LEDData> newLEDs;
  if (!parseLEDsJson(response, newLEDs)) {
    return false;
  }
  normalizeLEDOrder(newLEDs);

  std::lock_guard<std::mutex> lock(m_mutex);
  if (newLEDs == m_ledInfo && m_pendingLedEcho) {
    m_pendingLedEcho = false;
    LOG_INFO() << "MobileLightsPoller: local LED echo ignored and cleared";
    return false;
  }

  if (newLEDs != m_ledInfo) {
    m_ledInfo = newLEDs;
    m_pendingLedEcho = false;
    m_ledsChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: remote LED changes detected";
    return true;
  }

  return false;
}

bool MobileLightsPoller::pollSchedules() {
  const std::string response =
      m_http.get(m_apiUrl + "?action=get&domain=schedules");
  if (response.empty()) {
    return false;
  }

  std::vector<Schedule> newSchedules;
  if (!parseSchedulesJson(response, newSchedules)) {
    return false;
  }
  normalizeScheduleOrder(newSchedules);

  std::lock_guard<std::mutex> lock(m_mutex);
  if (newSchedules == m_schedule && m_pendingScheduleEcho) {
    m_pendingScheduleEcho = false;
    LOG_INFO()
        << "MobileLightsPoller: local schedule echo ignored and cleared";
    return false;
  }

  if (newSchedules != m_schedule) {
    m_schedule = newSchedules;
    m_pendingScheduleEcho = false;
    m_schedulesChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: remote schedule changes detected";
    return true;
  }

  return false;
}

bool MobileLightsPoller::pollThemes() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_themesInitialized)
      return false;
  }

  const std::string response =
      m_http.get(m_apiUrl + "?action=get&domain=themes");
  if (response.empty()) {
    return false;
  }

  std::vector<Theme> newThemes;
  if (!parseThemesJson(response, newThemes)) {
    return false;
  }
  normalizeThemeOrder(newThemes);

  std::lock_guard<std::mutex> lock(m_mutex);
  if (themesEqual(newThemes, m_themes) && m_pendingThemesEcho) {
    m_pendingThemesEcho = false;
    LOG_INFO() << "MobileLightsPoller: local themes echo ignored and cleared";
    return false;
  }

  if (!themesEqual(newThemes, m_themes)) {
    m_themes = newThemes;
    m_pendingThemesEcho = false;
    m_themesChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: remote themes changes detected";
    return true;
  }

  return false;
}

bool MobileLightsPoller::pollPatterns() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_patternsInitialized)
      return false;
  }

  const std::string response =
      m_http.get(m_apiUrl + "?action=get&domain=patterns");
  if (response.empty()) {
    return false;
  }

  std::vector<Pattern> newPatterns;
  if (!parsePatternsJson(response, newPatterns)) {
    return false;
  }
  normalizePatternOrder(newPatterns);

  std::lock_guard<std::mutex> lock(m_mutex);
  if (patternsEqual(newPatterns, m_patterns) && m_pendingPatternsEcho) {
    m_pendingPatternsEcho = false;
    LOG_INFO()
        << "MobileLightsPoller: local patterns echo ignored and cleared";
    return false;
  }

  if (!patternsEqual(newPatterns, m_patterns)) {
    m_patterns = newPatterns;
    m_pendingPatternsEcho = false;
    m_patternsChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: remote patterns changes detected";
    return true;
  }

  return false;
}

bool MobileLightsPoller::pollTeams() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_teamsInitialized)
      return false;
  }

  const std::string response =
      m_http.get(m_apiUrl + "?action=get&domain=teams");
  if (response.empty()) {
    return false;
  }

  std::vector<TeamRecord> newTeams;
  if (!parseTeamsJson(response, newTeams)) {
    return false;
  }
  normalizeTeamOrder(newTeams);

  std::lock_guard<std::mutex> lock(m_mutex);
  if (teamsEqual(newTeams, m_teams) && m_pendingTeamsEcho) {
    m_pendingTeamsEcho = false;
    LOG_INFO() << "MobileLightsPoller: local teams echo ignored and cleared";
    return false;
  }

  if (!teamsEqual(newTeams, m_teams)) {
    m_teams = newTeams;
    m_pendingTeamsEcho = false;
    m_teamsChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: remote teams changes detected";
    return true;
  }

  return false;
}

void MobileLightsPoller::onDispatch() {
  const bool emitChanged = m_changedPending.exchange(false);
  const bool emitOptions = m_optionsChangedPending.exchange(false);
  const bool emitLEDs = m_ledsChangedPending.exchange(false);
  const bool emitSchedules = m_schedulesChangedPending.exchange(false);
  const bool emitThemes = m_themesChangedPending.exchange(false);
  const bool emitPatterns = m_patternsChangedPending.exchange(false);
  const bool emitTeams = m_teamsChangedPending.exchange(false);
  const bool emitStartupSyncComplete =
      m_startupSyncCompletePending.exchange(false);

  Options optionsCopy;
  std::vector<LEDData> ledsCopy;
  std::vector<Schedule> schedulesCopy;
  std::vector<Theme> themesCopy;
  std::vector<Pattern> patternsCopy;
  std::vector<TeamRecord> teamsCopy;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (emitOptions) {
      optionsCopy = m_options;
    }
    if (emitLEDs) {
      ledsCopy = m_ledInfo;
    }
    if (emitSchedules) {
      schedulesCopy = m_schedule;
    }
    if (emitThemes) {
      themesCopy = m_themes;
    }
    if (emitPatterns) {
      patternsCopy = m_patterns;
    }
    if (emitTeams) {
      teamsCopy = m_teams;
    }
  }

  if (emitChanged) {
    m_signalChanged.emit();
  }
  if (emitOptions) {
    m_signalOptionsChanged.emit(optionsCopy);
  }
  if (emitLEDs) {
    m_signalLEDsChanged.emit(ledsCopy);
  }
  if (emitSchedules) {
    m_signalSchedulesChanged.emit(schedulesCopy);
  }
  if (emitThemes) {
    m_signalThemesChanged.emit(themesCopy);
  }
  if (emitPatterns) {
    m_signalPatternsChanged.emit(patternsCopy);
  }
  if (emitTeams) {
    m_signalTeamsChanged.emit(teamsCopy);
  }
  if (emitStartupSyncComplete) {
    m_signalStartupSyncComplete.emit();
  }
}

namespace {
int jsonInt(const json &j, const char *key, int fallback = 0) {
  if (!j.contains(key) || j[key].is_null()) {
    return fallback;
  }

  const auto &v = j[key];

  if (v.is_number_integer()) {
    return v.get<int>();
  }

  if (v.is_number_float()) {
    return static_cast<int>(v.get<double>());
  }

  if (v.is_string()) {
    try {
      return std::stoi(v.get<std::string>());
    } catch (...) {
      return fallback;
    }
  }

  return fallback;
}

int jsonIntAny(const json &j, std::initializer_list<const char *> keys,
               int fallback = 0) {
  for (const char *key : keys) {
    if (j.contains(key) && !j[key].is_null()) {
      return jsonInt(j, key, fallback);
    }
  }
  return fallback;
}

float jsonFloat(const json &j, const char *key, float fallback = 0.0f) {
  if (!j.contains(key) || j[key].is_null()) {
    return fallback;
  }

  const auto &v = j[key];

  if (v.is_number()) {
    return v.get<float>();
  }

  if (v.is_string()) {
    try {
      return std::stof(v.get<std::string>());
    } catch (...) {
      return fallback;
    }
  }

  return fallback;
}

std::string jsonString(const json &j, const char *key,
                       const std::string &fallback = "") {
  if (!j.contains(key) || j[key].is_null()) {
    return fallback;
  }

  const auto &v = j[key];

  if (v.is_string()) {
    return v.get<std::string>();
  }

  if (v.is_number_integer()) {
    return std::to_string(v.get<int>());
  }

  if (v.is_number_float()) {
    return std::to_string(v.get<double>());
  }

  return fallback;
}

std::string jsonStringAny(const json &j, std::initializer_list<const char *> keys,
                          const std::string &fallback = "") {
  for (const char *key : keys) {
    if (j.contains(key) && !j[key].is_null()) {
      return jsonString(j, key, fallback);
    }
  }
  return fallback;
}
} // namespace
bool MobileLightsPoller::parseOptionsJson(const std::string &response,
                                          Options &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid options JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("options")) {
    j = j["options"];
  }

  if (j.is_null()) {
    LOG_ERROR() << "MobileLightsPoller: options JSON data is null";
    return false;
  }

  if (j.is_array()) {
    Options parsed{};
    for (const auto &item : j) {
      const std::string name = jsonString(item, "name", "");
      const int value = jsonInt(item, "value", 0);
      if (name == "auto" || name == "sensor")
        parsed.sensor = value;
      else if (name == "on")
        parsed.on = value;
      else if (name == "theme")
        parsed.theme = value;
      else if (name == "ptrn" || name == "pattern")
        parsed.ptrn = value;
      else if (name == "bluetooth")
        parsed.bluetooth = value;
    }
    out = parsed;
    return true;
  }

  if (!j.is_object()) {
    LOG_ERROR() << "MobileLightsPoller: options JSON not object";
    return false;
  }

  out.sensor = jsonIntAny(j, {"auto", "sensor"}, 0);
  out.on = jsonInt(j, "on", 0);
  out.theme = jsonInt(j, "theme", 0);
  out.ptrn = jsonIntAny(j, {"pattern", "ptrn"}, 0);
  out.bluetooth = jsonInt(j, "bluetooth", 0);

  return true;
}

bool MobileLightsPoller::parseLEDsJson(const std::string &response,
                                       std::vector<LEDData> &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid leds JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("leds")) {
    j = j["leds"];
  }

  if (!j.is_array()) {
    LOG_ERROR() << "MobileLightsPoller: leds JSON not array";
    LOG_ERROR() << "MobileLightsPoller leds response: " << response;
    return false;
  }

  std::vector<LEDData> temp;
  for (const auto &item : j) {
    LEDData led;
    led.name = jsonStringAny(item, {"led_name", "name"}, "");
    led.group = jsonIntAny(item, {"led_group", "group"}, 0);
    led.redPin = jsonIntAny(item, {"pin_red", "red_pin", "redPin"}, 0);
    led.grnPin = jsonIntAny(item, {"pin_grn", "grn_pin", "grnPin"}, 0);
    led.bluPin = jsonIntAny(item, {"pin_blu", "blu_pin", "bluPin"}, 0);
    led.redVal = jsonIntAny(item, {"red_value", "redVal"}, 0);
    led.grnVal = jsonIntAny(item, {"grn_value", "grnVal"}, 0);
    led.bluVal = jsonIntAny(item, {"blu_value", "bluVal"}, 0);
    temp.push_back(led);
  }

  out = std::move(temp);
  return true;
}

bool MobileLightsPoller::parseSchedulesJson(const std::string &response,
                                            std::vector<Schedule> &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid schedules JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("schedules")) {
    j = j["schedules"];
  }

  if (!j.is_array()) {
    LOG_ERROR() << "MobileLightsPoller: schedules JSON not array";
    LOG_ERROR() << "MobileLightsPoller schedules response: " << response;
    return false;
  }

  std::vector<Schedule> temp;
  for (const auto &item : j) {
    Schedule sch;
    sch.name = jsonStringAny(item, {"theme_name", "name"}, "");
    sch.themeID = jsonIntAny(item, {"theme_id", "themeID"}, 0);
    sch.enabled = jsonIntAny(item, {"theme_enabled", "enabled"}, 0);
    sch.sDate = jsonStringAny(item, {"start_date", "sDate"}, "");
    sch.sTime = jsonStringAny(item, {"start_time", "sTime"}, "");
    sch.eDate = jsonStringAny(item, {"end_date", "eDate"}, "");
    sch.eTime = jsonStringAny(item, {"end_time", "eTime"}, "");
    temp.push_back(sch);
  }

  out = std::move(temp);
  return true;
}

bool MobileLightsPoller::parseThemesJson(const std::string &response,
                                         std::vector<Theme> &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid themes JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("themes")) {
    j = j["themes"];
  }

  if (!j.is_array()) {
    LOG_ERROR() << "MobileLightsPoller: themes JSON not array";
    return false;
  }

  std::vector<Theme> temp;
  for (const auto &item : j) {
    const int themeId = jsonIntAny(item, {"theme_id", "id"}, 0);
    auto it = std::find_if(temp.begin(), temp.end(),
                           [themeId](const Theme &t) { return t.id == themeId; });
    if (it == temp.end()) {
      Theme theme;
      theme.id = themeId;
      theme.name = jsonString(item, "name", "");
      theme.fileName = jsonStringAny(item, {"fileName", "filename"}, "");
      temp.push_back(theme);
      it = std::prev(temp.end());
    }

    if (item.contains("colors") && item["colors"].is_array()) {
      for (const auto &color : item["colors"]) {
        RGB_Color c;
        c.r = static_cast<uint8_t>(jsonInt(color, "r", 0));
        c.g = static_cast<uint8_t>(jsonInt(color, "g", 0));
        c.b = static_cast<uint8_t>(jsonInt(color, "b", 0));
        it->colors.push_back(c);
      }
    } else if (item.contains("color_index") || item.contains("r") ||
               item.contains("g") || item.contains("b")) {
      RGB_Color c;
      c.r = static_cast<uint8_t>(jsonInt(item, "r", 0));
      c.g = static_cast<uint8_t>(jsonInt(item, "g", 0));
      c.b = static_cast<uint8_t>(jsonInt(item, "b", 0));
      const int index = jsonInt(item, "color_index",
                                static_cast<int>(it->colors.size()));
      if (index >= 0) {
        if (static_cast<size_t>(index) >= it->colors.size())
          it->colors.resize(index + 1);
        it->colors[index] = c;
      }
    }
  }

  out = std::move(temp);
  normalizeThemeOrder(out);
  return true;
}

bool MobileLightsPoller::parsePatternsJson(const std::string &response,
                                           std::vector<Pattern> &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid patterns JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("patterns")) {
    j = j["patterns"];
  }

  if (!j.is_array()) {
    LOG_ERROR() << "MobileLightsPoller: patterns JSON not array";
    return false;
  }

  std::vector<Pattern> temp;
  for (const auto &item : j) {
    Pattern pattern;
    pattern.id = jsonInt(item, "id", 0);
    pattern.speed = jsonInt(item, "speed", 50);
    temp.push_back(pattern);
  }

  out = std::move(temp);
  normalizePatternOrder(out);
  return true;
}

bool MobileLightsPoller::parseTeamsJson(const std::string &response,
                                        std::vector<TeamRecord> &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid teams JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("teams")) {
    j = j["teams"];
  }

  if (!j.is_array()) {
    LOG_ERROR() << "MobileLightsPoller: teams JSON not array";
    return false;
  }

  std::vector<TeamRecord> temp;
  for (const auto &item : j) {
    TeamRecord team;
    team.id = jsonInt(item, "id", 0);
    team.name = jsonString(item, "name", "");
    team.league = jsonString(item, "league", "");
    team.teamCode = jsonString(item, "team_code", "");
    team.homeAway = jsonString(item, "home_away", "home");
    team.nextGameUrlTemplate = jsonString(item, "next_game_url_template", "");
    team.nextGameParser = jsonString(item, "next_game_parser", "");
    team.liveGameUrlTemplate = jsonString(item, "live_game_url_template", "");
    team.liveGameParser = jsonString(item, "live_game_parser", "");
    team.apiTeamId = jsonString(item, "api_team_id", "");
    team.enabled = jsonInt(item, "enabled", 1);
    team.displayOrder = jsonInt(item, "display_order", 0);
    team.themeName = jsonString(item, "theme_name", "");
    team.themeID = jsonInt(item, "theme_id", 0);
    team.iconPath = jsonString(item, "icon_path", "");
    team.nextGameUtc = jsonString(item, "next_game_utc", "");
    team.lastHomeScore = jsonInt(item, "last_home_score", -1);
    team.lastAwayScore = jsonInt(item, "last_away_score", -1);
    team.scoreAnimationDelaySeconds =
        jsonInt(item, "score_animation_delay_seconds", 0);
    team.lastGameId = jsonString(item, "last_game_id", "");
    team.lastCheckedUtc = jsonString(item, "last_checked_utc", "");
    team.nextOpponentCode = jsonString(item, "next_opponent_code", "");
    team.nextOpponentName = jsonString(item, "next_opponent_name", "");
    if (item.contains("colors") && item["colors"].is_array()) {
      for (const auto &colorJson : item["colors"]) {
        TeamColor color;
        color.id = jsonInt(colorJson, "id", 0);
        color.teamId = jsonInt(colorJson, "team_id", team.id);
        color.colorRole = jsonString(colorJson, "color_role", "");
        color.r = jsonInt(colorJson, "r", 0);
        color.g = jsonInt(colorJson, "g", 0);
        color.b = jsonInt(colorJson, "b", 0);
        color.displayOrder = jsonInt(colorJson, "display_order", 0);
        team.colors.push_back(color);
      }
      std::sort(team.colors.begin(), team.colors.end(),
                [](const TeamColor &a, const TeamColor &b) {
                  if (a.displayOrder != b.displayOrder)
                    return a.displayOrder < b.displayOrder;
                  return a.colorRole < b.colorRole;
                });
    }
    temp.push_back(team);
  }

  out = std::move(temp);
  normalizeTeamOrder(out);
  return true;
}

std::string MobileLightsPoller::normalizeBaseUrl(std::string baseUrl) {
  if (baseUrl.empty()) {
    baseUrl = appConfig().apiBaseUrl;
  }
  if (baseUrl.back() != '/') {
    baseUrl.push_back('/');
  }
  return baseUrl;
}
