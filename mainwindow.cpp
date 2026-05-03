#include "mainwindow.h"

#include "debug/debugpage.h"
#include "debug/debugsportspage.h"
#include "drivers/i2c/bme280.h"
#include "drivers/network/httphelper.h"
#include "engine/baseballgamedayanimation.h"
#include "engine/footballgamedayanimation.h"
#include "engine/hockeygamedayanimation.h"
#include "gui/bluetoothcontrols.h"
#include "gui/bluetoothdevicedetail.h"
#include "gui/deltaall.h"
#include "gui/deltagroup.h"
#include "gui/home.h"
#include "gui/hardwareconfigpage.h"
#include "gui/imgbutton.h"
#include "gui/patterns.h"
#include "gui/settings.h"
#include "gui/themes.h"
#include "utils/teamassets.h"
#include "utils/ui_metrics.h"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <gtkmm.h>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <array>
#include <cstdlib>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

static bool setBluetoothRfkillBlocked(bool blocked) {
  const int rc =
      std::system(blocked ? "rfkill block bluetooth >/dev/null 2>&1"
                          : "rfkill unblock bluetooth >/dev/null 2>&1");
  return rc == 0;
}

namespace {
constexpr uint8_t kAlternatePatternId = 8;

std::string sportsDbPath() { return runtimeSettingsPath() + "/lights.db"; }

constexpr int kDefaultHardwareLeds = 19;
constexpr int kDefaultHardwareShiftRegs = 8;
constexpr int kMaxHardwareLeds = 32;
constexpr int kMaxHardwareShiftRegs = 16;

std::string hardwareConfigPath() {
  return runtimeSettingsPath() + "/hardware_config.txt";
}

std::pair<int, int> readHardwareConfigDefaults() {
  int numLeds = kDefaultHardwareLeds;
  int numShiftRegs = kDefaultHardwareShiftRegs;

  std::ifstream file(hardwareConfigPath());
  std::string line;
  while (std::getline(file, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    try {
      if (key == "num_leds")
        numLeds = std::stoi(value);
      else if (key == "num_shift_regs")
        numShiftRegs = std::stoi(value);
    } catch (...) {
    }
  }

  numLeds = std::max(1, std::min(kMaxHardwareLeds, numLeds));
  numShiftRegs = std::max(1, std::min(kMaxHardwareShiftRegs, numShiftRegs));
  return {numLeds, numShiftRegs};
}

void writeHardwareConfigDefaults(int numLeds, int numShiftRegs) {
  std::error_code ec;
  std::filesystem::create_directories(runtimeSettingsPath(), ec);
  std::ofstream file(hardwareConfigPath(), std::ios::trunc);
  if (!file)
    return;
  file << "num_leds=" << numLeds << "\n";
  file << "num_shift_regs=" << numShiftRegs << "\n";
}

bool parseUtcToLocal(const std::string &utcText, std::tm &localOut);

std::string uppercaseCopy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return s;
}

std::string lowercaseCopy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

std::string animationBasePath() {
  const char *user = std::getenv("USER");
  const bool prodUser = user && std::string(user) == "lights";
  const std::string prodPath = "/home/lights/.local/share/lights/animation";
  const std::string devPath = "/home/dev/.lightcontroller/animation";
  const std::string chosen = prodUser ? prodPath : devPath;

  std::error_code ec;
  std::filesystem::create_directories(chosen, ec);
  if (ec)
    LOG_WARN() << "Failed to create animation directory " << chosen << ": "
               << ec.message();
  return chosen;
}

std::string gameDayAnimationRoot() {
#if (UBUNTU == 1)
  return "/home/dev/.lightcontroller/animations";
#else
  return "/home/lights/.local/share/lights/animations";
#endif
}

std::string mappedSportsAssetId(const std::string &league,
                                const std::string &idOrCode) {
  const std::string key = uppercaseCopy(idOrCode);
  if (key.empty())
    return "";

  const std::string lg = uppercaseCopy(league);
  const std::vector<std::pair<std::string, std::string>> mlb = {
      {"108", "108"}, {"LAA", "108"}, {"109", "109"}, {"ARI", "109"},
      {"110", "110"}, {"BAL", "110"}, {"111", "111"}, {"BOS", "111"},
      {"112", "112"}, {"CHC", "112"}, {"113", "113"}, {"CIN", "113"},
      {"114", "114"}, {"CLE", "114"}, {"115", "115"}, {"COL", "115"},
      {"116", "116"}, {"DET", "116"}, {"117", "117"}, {"HOU", "117"},
      {"118", "118"}, {"KC", "118"},  {"KCR", "118"}, {"119", "119"},
      {"LAD", "119"}, {"120", "120"}, {"WSH", "120"}, {"WAS", "120"},
      {"121", "121"}, {"NYM", "121"}, {"133", "133"}, {"OAK", "133"},
      {"ATH", "133"}, {"134", "134"}, {"PIT", "134"}, {"135", "135"},
      {"SD", "135"},  {"SDP", "135"}, {"136", "136"}, {"SEA", "136"},
      {"137", "137"}, {"SF", "137"},  {"SFG", "137"}, {"138", "138"},
      {"STL", "138"}, {"139", "139"}, {"TB", "139"},  {"TBR", "139"},
      {"140", "140"}, {"TEX", "140"}, {"141", "141"}, {"TOR", "141"},
      {"142", "142"}, {"MIN", "142"}, {"143", "143"}, {"PHI", "143"},
      {"144", "144"}, {"ATL", "144"}, {"145", "145"}, {"CWS", "145"},
      {"CHW", "145"}, {"146", "146"}, {"MIA", "146"}, {"147", "147"},
      {"NYY", "147"}, {"158", "158"}, {"MIL", "158"}};
  const std::vector<std::pair<std::string, std::string>> nfl = {
      {"1", "ATL"},  {"ATL", "ATL"}, {"2", "BUF"},  {"BUF", "BUF"},
      {"3", "CHI"},  {"CHI", "CHI"}, {"4", "CIN"},  {"CIN", "CIN"},
      {"5", "CLE"},  {"CLE", "CLE"}, {"6", "DAL"},  {"DAL", "DAL"},
      {"7", "DEN"},  {"DEN", "DEN"}, {"8", "DET"},  {"DET", "DET"},
      {"9", "GB"},   {"GB", "GB"},   {"10", "TEN"}, {"TEN", "TEN"},
      {"11", "IND"}, {"IND", "IND"}, {"12", "KC"},  {"KC", "KC"},
      {"13", "LV"},  {"LV", "LV"},   {"OAK", "LV"}, {"14", "LAR"},
      {"LAR", "LAR"}, {"15", "MIA"}, {"MIA", "MIA"}, {"16", "MIN"},
      {"MIN", "MIN"}, {"17", "NE"},  {"NE", "NE"},   {"18", "NO"},
      {"NO", "NO"},  {"19", "NYG"}, {"NYG", "NYG"}, {"20", "NYJ"},
      {"NYJ", "NYJ"}, {"21", "PHI"}, {"PHI", "PHI"}, {"22", "ARI"},
      {"ARI", "ARI"}, {"23", "PIT"}, {"PIT", "PIT"}, {"24", "LAC"},
      {"LAC", "LAC"}, {"SD", "LAC"}, {"25", "SF"},  {"SF", "SF"},
      {"26", "SEA"}, {"SEA", "SEA"}, {"27", "TB"},  {"TB", "TB"},
      {"28", "WAS"}, {"WAS", "WAS"}, {"WSH", "WAS"}, {"29", "CAR"},
      {"CAR", "CAR"}, {"30", "JAX"}, {"JAX", "JAX"}, {"33", "BAL"},
      {"BAL", "BAL"}, {"34", "HOU"}, {"HOU", "HOU"}};
  const std::vector<std::pair<std::string, std::string>> nhl = {
      {"1", "1"},   {"NJD", "1"}, {"2", "2"},   {"NYI", "2"},
      {"3", "3"},   {"NYR", "3"}, {"4", "4"},   {"PHI", "4"},
      {"5", "5"},   {"PIT", "5"}, {"6", "6"},   {"BOS", "6"},
      {"7", "7"},   {"BUF", "7"}, {"8", "8"},   {"MTL", "8"},
      {"9", "9"},   {"OTT", "9"}, {"10", "10"}, {"TOR", "10"},
      {"12", "12"}, {"CAR", "12"}, {"13", "13"}, {"FLA", "13"},
      {"14", "14"}, {"TBL", "14"}, {"TB", "14"}, {"15", "15"},
      {"WSH", "15"}, {"WAS", "15"}, {"16", "16"}, {"CHI", "16"},
      {"17", "17"}, {"DET", "17"}, {"18", "18"}, {"NSH", "18"},
      {"19", "19"}, {"STL", "19"}, {"20", "20"}, {"CGY", "20"},
      {"21", "21"}, {"COL", "21"}, {"22", "22"}, {"EDM", "22"},
      {"23", "23"}, {"VAN", "23"}, {"24", "24"}, {"ANA", "24"},
      {"25", "25"}, {"DAL", "25"}, {"26", "26"}, {"LAK", "26"},
      {"28", "28"}, {"SJS", "28"}, {"29", "29"}, {"CBJ", "29"},
      {"30", "30"}, {"MIN", "30"}, {"52", "52"}, {"WPG", "52"},
      {"54", "54"}, {"VGK", "54"}, {"55", "55"}, {"SEA", "55"},
      {"59", "59"}, {"UTA", "59"}};

  const auto *map = lg == "MLB" ? &mlb : (lg == "NFL" ? &nfl : &nhl);
  for (const auto &entry : *map) {
    if (entry.first == key)
      return entry.second;
  }
  return key;
}

std::string gameDayTeamAssetPath(const std::string &league,
                                 const std::string &idOrCode,
                                 const std::string &suffix) {
  const std::string lg = lowercaseCopy(league);
  const std::string assetId = mappedSportsAssetId(league, idOrCode);
  if (lg.empty() || assetId.empty())
    return "";

  const std::filesystem::path path =
      std::filesystem::path(gameDayAnimationRoot()) / "team_assets" / lg /
      (assetId + suffix);
  if (std::filesystem::exists(path))
    return path.string();

  LOG_WARN() << "Missing game-day asset " << path.string();
  return "";
}

std::string gameDayTeamAssetPath(const TeamRecord &team,
                                 const std::string &suffix) {
  const std::string lg = uppercaseCopy(team.league);
  std::string id = team.apiTeamId;
  if (lg == "NFL")
    id = !team.teamCode.empty() ? team.teamCode : team.apiTeamId;
  else if (lg == "NHL" && (id.empty() || uppercaseCopy(id) == "SEA"))
    id = !team.teamCode.empty() ? team.teamCode : id;
  else if (id.empty())
    id = team.teamCode;
  return gameDayTeamAssetPath(team.league, id, suffix);
}

std::pair<std::string, std::string> splitCityTeam(const std::string &fullName,
                                                  const std::string &fallback) {
  if (fullName.empty())
    return {fallback, fallback};

  const auto pos = fullName.find_last_of(' ');
  if (pos == std::string::npos)
    return {fullName, fullName};

  return {fullName.substr(0, pos), fullName.substr(pos + 1)};
}

std::vector<RGB_Color> colorsForSide(const TeamRecord &team, bool homeSide) {
  const std::string prefix = homeSide ? "home_" : "away_";
  std::vector<TeamColor> ordered;
  for (const auto &color : team.colors) {
    if (color.colorRole.rfind(prefix, 0) == 0)
      ordered.push_back(color);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const TeamColor &a, const TeamColor &b) {
              return a.displayOrder < b.displayOrder;
            });

  std::vector<RGB_Color> out;
  for (const auto &color : ordered)
    out.push_back({static_cast<uint8_t>(std::max(0, std::min(255, color.r))),
                   static_cast<uint8_t>(std::max(0, std::min(255, color.g))),
                   static_cast<uint8_t>(std::max(0, std::min(255, color.b)))});
  return out;
}

std::vector<RGB_Color> allTeamColors(const TeamRecord &team) {
  std::vector<TeamColor> ordered = team.colors;
  std::sort(ordered.begin(), ordered.end(),
            [](const TeamColor &a, const TeamColor &b) {
              return a.displayOrder < b.displayOrder;
            });

  std::vector<RGB_Color> out;
  for (const auto &color : ordered)
    out.push_back({static_cast<uint8_t>(std::max(0, std::min(255, color.r))),
                   static_cast<uint8_t>(std::max(0, std::min(255, color.g))),
                   static_cast<uint8_t>(std::max(0, std::min(255, color.b)))});
  return out;
}

std::vector<RGB_Color> neutralOpponentColors() {
  return {{238, 241, 245}, {166, 174, 186}};
}

std::vector<RGB_Color> fallbackOpponentColors(const std::string &league,
                                              const std::string &teamCode) {
  const std::string key = uppercaseCopy(league) + ":" + uppercaseCopy(teamCode);
  if (key == "MLB:HOU")
    return {{0, 45, 98}, {235, 110, 31}};
  if (key == "MLB:TEX")
    return {{0, 50, 120}, {192, 17, 31}};
  if (key == "MLB:OAK" || key == "MLB:ATH")
    return {{0, 56, 49}, {239, 178, 30}};
  if (key == "MLB:LAA")
    return {{186, 0, 33}, {0, 50, 99}};
  if (key == "MLB:NYY")
    return {{12, 35, 64}, {196, 206, 211}};
  if (key == "MLB:BOS")
    return {{189, 48, 57}, {12, 35, 64}};
  if (key == "MLB:TOR")
    return {{19, 74, 142}, {232, 41, 28}};

  if (key == "NFL:SF")
    return {{170, 0, 0}, {173, 153, 93}};
  if (key == "NFL:LAR")
    return {{0, 53, 148}, {255, 209, 0}};
  if (key == "NFL:ARI")
    return {{151, 35, 63}, {0, 0, 0}};
  if (key == "NFL:DAL")
    return {{0, 53, 148}, {134, 147, 151}};
  if (key == "NFL:GB")
    return {{24, 48, 40}, {255, 184, 28}};
  if (key == "NFL:DEN")
    return {{251, 79, 20}, {0, 34, 68}};
  if (key == "NFL:KC")
    return {{227, 24, 55}, {255, 184, 28}};

  if (key == "NHL:VAN")
    return {{0, 32, 91}, {0, 122, 62}};
  if (key == "NHL:VGK")
    return {{185, 151, 91}, {51, 63, 72}};
  if (key == "NHL:EDM")
    return {{4, 30, 66}, {252, 76, 2}};
  if (key == "NHL:CGY")
    return {{200, 16, 46}, {241, 190, 72}};
  if (key == "NHL:LAK")
    return {{17, 17, 17}, {162, 170, 173}};
  if (key == "NHL:SJS")
    return {{0, 109, 117}, {234, 114, 0}};
  if (key == "NHL:ANA")
    return {{252, 76, 2}, {185, 151, 91}};

  return {};
}

std::string formatGameTimeDisplay(const TeamRecord &team,
                                  const GameInfo &gameInfo) {
  if (!gameInfo.standardTime.empty()) {
    if (gameInfo.standardTime.front() == '@')
      return gameInfo.standardTime;
    return "@ " + gameInfo.standardTime;
  }

  const std::string utc =
      !gameInfo.dateTimeUTC.empty() ? gameInfo.dateTimeUTC : team.nextGameUtc;
  std::tm local{};
  if (!parseUtcToLocal(utc, local))
    return "@ TBD";

  char buf[32];
  std::strftime(buf, sizeof(buf), "%I:%M %p", &local);
  std::string text = buf;
  if (!text.empty() && text.front() == '0')
    text.erase(text.begin());
  return "@ " + text;
}

std::string scheduleNameForTeam(const TeamRecord &team) {
  return "TEAM_" + team.league + "_" + team.teamCode;
}

std::string nowUtcString() {
  std::time_t now = std::time(nullptr);
  std::tm utc{};
  gmtime_r(&now, &utc);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return buf;
}

bool parseUtcToLocal(const std::string &utcText, std::tm &localOut) {
  if (utcText.empty())
    return false;

  std::string s = utcText;
  if (!s.empty() && s.back() == 'Z')
    s.pop_back();

  std::tm utc{};
  std::istringstream ss(s);
  ss >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%S");
  if (ss.fail()) {
    ss.clear();
    ss.str(s);
    ss >> std::get_time(&utc, "%Y-%m-%d");
    if (ss.fail())
      return false;
  }

  utc.tm_isdst = 0;
  std::time_t t = timegm(&utc);
  if (t == -1)
    return false;

  localtime_r(&t, &localOut);
  return true;
}

std::string mmddFromTm(const std::tm &tm) {
  char buf[8];
  std::strftime(buf, sizeof(buf), "%m/%d", &tm);
  return buf;
}

bool isTodayLocal(const std::tm &tm) {
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  return tm.tm_year == local.tm_year && tm.tm_mon == local.tm_mon &&
         tm.tm_mday == local.tm_mday;
}

bool isSportsTerminalState(const std::string &state) {
  std::string s = state;
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s == "final" || s == "off" || s == "complete" ||
         s == "completed" || s == "post" || s == "closed" ||
         s == "ended" || s == "postponed" || s == "cancelled" ||
         s == "canceled";
}

bool isSportsLiveState(const std::string &state) {
  std::string s = state;
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s == "live" || s == "inprogress" || s == "in_progress" ||
         s == "in progress" || s == "active" || s == "critical" ||
         s == "gameon";
}

std::string bluetoothToastMessage(const std::string &status) {
  constexpr const char *pairedPrefix = "Paired: ";
  constexpr const char *connectedPrefix = "Connected: ";

  if (status.rfind(pairedPrefix, 0) == 0)
    return "Pairing successful: " + status.substr(std::strlen(pairedPrefix));

  if (status.rfind(connectedPrefix, 0) == 0)
    return status;

  return "";
}

const char *teensyFileStatusName(uint8_t status) {
  switch (status) {
  case TeensyClient::FILE_IDLE:
    return "IDLE";
  case TeensyClient::FILE_RECEIVING:
    return "RECEIVING";
  case TeensyClient::FILE_SUCCESS:
    return "SUCCESS";
  case TeensyClient::FILE_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

bool themeRowsEqual(const Theme &a, const Theme &b) {
  if (a.id != b.id || a.name != b.name || a.fileName != b.fileName ||
      a.colors.size() != b.colors.size()) {
    return false;
  }

  for (size_t i = 0; i < a.colors.size(); ++i) {
    if (a.colors[i].r != b.colors[i].r || a.colors[i].g != b.colors[i].g ||
        a.colors[i].b != b.colors[i].b) {
      return false;
    }
  }

  return true;
}

bool themeUploadQueued(const std::deque<Theme> &queue, int themeId) {
  return std::any_of(queue.begin(), queue.end(), [themeId](const Theme &theme) {
    return theme.id == themeId;
  });
}

std::string trimCopy(std::string s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

std::string shellQuote(const std::string &value) {
  std::string out = "'";
  for (char c : value) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  out += "'";
  return out;
}

std::string configuredAudioValue(const char *envName,
                                 const std::string &fileName,
                                 const std::string &configValue) {
  if (!configValue.empty())
    return configValue;

  if (const char *env = std::getenv(envName)) {
    const std::string value = trimCopy(env);
    if (!value.empty())
      return value;
  }

  std::ifstream file(runtimeSettingsPath() + "/" + fileName);
  if (!file)
    return "";

  std::string line;
  std::getline(file, line);
  return trimCopy(line);
}

std::vector<std::string> pactlNames(const std::string &kind) {
  std::vector<std::string> names;
  FILE *pipe = popen(("pactl list short " + kind + " 2>/dev/null").c_str(), "r");
  if (!pipe)
    return names;

  char line[512];
  while (fgets(line, sizeof(line), pipe)) {
    std::istringstream ss(line);
    std::string index;
    std::string name;
    if (!(ss >> index >> name))
      continue;
    names.push_back(name);
  }

  pclose(pipe);
  return names;
}

std::string lowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return value;
}

bool looksLikeHdmiSink(const std::string &sink) {
  const std::string s = lowerCopy(sink);
  return s.find("hdmi") != std::string::npos;
}

bool looksLikeHatSink(const std::string &sink) {
  const std::string s = lowerCopy(sink);
  if (looksLikeHdmiSink(s))
    return false;

  return s.find("hat") != std::string::npos ||
         s.find("hifiberry") != std::string::npos ||
         s.find("iqaudio") != std::string::npos ||
         s.find("audioinjector") != std::string::npos ||
         s.find("dac") != std::string::npos ||
         s.find("snd_rpi") != std::string::npos ||
         s.find("soc_sound") != std::string::npos;
}

std::optional<std::string> firstMatchingName(const std::vector<std::string> &names,
                                             bool (*predicate)(const std::string &)) {
  for (const auto &name : names) {
    if (predicate(name))
      return name;
  }
  return std::nullopt;
}

std::string configuredLightShowMonitor() {
  return configuredAudioValue("LIGHTSHOW_MONITOR", "lightshow_monitor.txt",
                              appConfig().lightShowMonitor);
}

std::string configuredBluetoothSink() {
  return configuredAudioValue("LIGHTS_BLUETOOTH_SINK", "bluetooth_sink.txt",
                              appConfig().bluetoothSink);
}

std::string configuredDoorbellSink() {
  return configuredAudioValue("LIGHTS_DOORBELL_SINK", "doorbell_sink.txt",
                              appConfig().doorbellSink);
}

std::string chooseBluetoothSink() {
  const std::string configured = configuredBluetoothSink();
  if (!configured.empty())
    return configured;

  const auto sinks = pactlNames("sinks");
  if (auto hat = firstMatchingName(sinks, looksLikeHatSink))
    return *hat;

  return "";
}

std::string chooseDoorbellSink() {
  const std::string configured = configuredDoorbellSink();
  if (!configured.empty())
    return configured;

  const auto sinks = pactlNames("sinks");
  if (auto hdmi = firstMatchingName(sinks, looksLikeHdmiSink))
    return *hdmi;

  return "";
}

std::string monitorForSink(const std::string &sink) {
  if (sink.empty())
    return "";

  const std::string expected = sink + ".monitor";
  const auto sources = pactlNames("sources");
  for (const auto &source : sources) {
    if (source == expected)
      return source;
  }

  for (const auto &source : sources) {
    if (source.find(sink) != std::string::npos &&
        source.find(".monitor") != std::string::npos) {
      return source;
    }
  }

  return "";
}

std::string chooseLightShowMonitor() {
  const std::string configured = configuredLightShowMonitor();
  if (!configured.empty())
    return configured;

  const std::string hatSink = chooseBluetoothSink();
  if (hatSink.empty())
    return "";

  return monitorForSink(hatSink);
}

bool setBluetoothPlaybackSink(const std::string &sink) {
  if (sink.empty())
    return false;

  return std::system(("pactl set-default-sink " + shellQuote(sink) +
                      " >/dev/null 2>&1")
                         .c_str()) == 0;
}

bool playDoorbellSoundOnSink(const std::string &sink) {
  if (sink.empty())
    return false;

  const std::string soundPath = runtimeSettingsPath() + "/sounds/doorbell.ogg";
  const std::string cmd = "paplay --device=" + shellQuote(sink) + " " +
                          shellQuote(soundPath) + " >/dev/null 2>&1 &";
  return std::system(cmd.c_str()) == 0;
}
} // namespace

MainWindow::MainWindow()
    : m_teensyClient(appConfig().i2cBus),
      m_powerSwitch(appConfig().gpioChip, appConfig().teensySwitchPin,
                    appConfig().shiftRegisterSwitchPin),
      gpio(appConfig().gpioChip), m_btControl(runtimeSettingsPath()),
      m_ampSwitch(appConfig().gpioChip, appConfig().ampSwitchPin) {
  Logger::useStdOutAndFile(LOG_FILE_MSTR, true);
  LOG_INFO() << "Logger initialized";
  LOG_INFO() << "MainWindow ctor begin";

  fullscreen();

  loadSettings();
  buildShell();
  startThreads();
  startConnections();

  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
             Gdk::TOUCH_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK |
             Gdk::SCROLL_MASK);

  signal_event_after().connect(
      sigc::mem_fun(*this, &MainWindow::onAnyEventAfter));

  showHomePage();
  resetIdleClockTimer();
  startAsyncStartupTasks();

  LOG_INFO() << "MainWindow ctor complete";
}

void MainWindow::initializeStartupState() {

  m_bluetoothState = 0;

  if (!m_ampSwitch.setEnabled(false)) {
    LOG_WARN() << "Failed to turn amp off at startup: "
               << m_ampSwitch.lastError();
  } else {
    m_ampEnabled = false;
  }
}

bool MainWindow::setLightsPowerEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(m_powerMutex);

  if (!m_powerSwitch.setEnabled(enabled)) {
    LOG_WARN() << "Failed to set lights power "
               << (enabled ? "on: " : "off: ") << m_powerSwitch.lastError();
    m_lightsPowerEnabled = false;
    return false;
  }

  m_lightsPowerEnabled = enabled;
  m_teensyClient.on.store(enabled, std::memory_order_relaxed);

  if (!enabled) {
    m_teensyClient.closeBus();
    return true;
  }

  const bool ready = waitForTeensyReady();
  if (!ready) {
    m_teensyClient.on.store(false, std::memory_order_relaxed);
    m_teensyClient.closeBus();
    m_lightsPowerEnabled = false;
  } else {
    startNextThemeUpload();
  }

  return ready;
}

bool MainWindow::waitForTeensyReady() {
  m_teensyClient.on.store(true, std::memory_order_relaxed);

  for (int attempt = 0; attempt < 20; ++attempt) {
    if (!m_teensyClient.openBus()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      continue;
    }

    bool ready = false;
    if (m_teensyClient.readWakeReady(ready) && ready)
      return true;

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }

  m_teensyClient.on.store(false, std::memory_order_relaxed);
  m_teensyClient.closeBus();
  return false;
}

bool MainWindow::lightsAreOn() const { return m_lightsPowerEnabled; }

void MainWindow::updateLightsDependentUi() {
  if (m_homePage)
    m_homePage->set_lights_actions_enabled(lightsAreOn());
}

bool MainWindow::requireLightsOnForHomeAction() {
  if (lightsAreOn())
    return true;

  showShortToast("Turn Lights On");
  updateLightsDependentUi();
  return false;
}

void MainWindow::loadSettings() {
  const std::string settingsPath = runtimeSettingsPath();
  LOG_INFO() << "Loading settings from " << settingsPath;

  ensureCoreSchema(settingsPath + "/lights.db");

  m_ledInfo = readLEDInfo(settingsPath);
  m_options = readOptions(settingsPath);
  LOG_INFO() << "Loaded option auto=" << m_options.sensor;
  LOG_INFO() << "Loaded option on=" << m_options.on;
  LOG_INFO() << "Loaded option bluetooth=" << m_options.bluetooth;
  m_schedule = readSchedule(settingsPath);
  ensureSportsSchema(sportsDbPath());

  std::time_t now = std::time(nullptr);
  std::tm local_tm{};
  localtime_r(&now, &local_tm);
  const int year = local_tm.tm_year + 1900;

  updateMoveableHolidayDates(year);
  LOG_INFO() << "Settings loaded. leds=" << m_ledInfo.size()
             << " schedule_entries=" << m_schedule.size();
}

void MainWindow::startAsyncStartupTasks() {
  initializeStartupState();

  m_startupCacheDispatcher.connect(
      sigc::mem_fun(*this, &MainWindow::onStartupCacheLoaded));
  m_startupHardwareDispatcher.connect(
      sigc::mem_fun(*this, &MainWindow::onStartupHardwareFinished));

  Glib::signal_idle().connect_once([this]() {
    if (m_shuttingDown)
      return;

    startStartupCacheLoad();
    startStartupHardwareApply();
  });
}

void MainWindow::startStartupCacheLoad() {
  if (m_startupCacheThread.joinable())
    return;

  m_startupCacheThread = std::thread([this]() {
    std::vector<Theme> themes;
    std::vector<Pattern> patterns;
    std::vector<TeamRecord> teams;
    bool btInitOk = false;

    if (!m_shuttingDown) {
      themes = readThemeColors(runtimeSettingsPath());
      patterns = readPatternSpeeds(runtimeSettingsPath());
      teams = readTeams(sportsDbPath());

      LOG_INFO() << "BT startup init: rfkill unblock bluetooth";
      setBluetoothRfkillBlocked(false);
      btInitOk = m_btControl.init();
    }

    {
      std::lock_guard<std::mutex> lock(m_startupMutex);
      m_startupThemes = std::move(themes);
      m_startupPatterns = std::move(patterns);
      m_startupTeams = std::move(teams);
      m_startupBtInitOk = btInitOk;
    }

    if (!m_shuttingDown)
      m_startupCacheDispatcher.emit();
  });
}

void MainWindow::onStartupCacheLoaded() {
  if (m_startupCacheThread.joinable())
    m_startupCacheThread.join();

  bool btInitOk = false;

  {
    std::lock_guard<std::mutex> lock(m_startupMutex);

    if (!m_themesLoaded) {
      m_themes = std::move(m_startupThemes);
      m_themesLoaded = true;
    }

    if (!m_patternsLoaded) {
      m_pattern = std::move(m_startupPatterns);
      m_patternsLoaded = true;
    }

    if (!m_teamsLoaded) {
      m_teams = std::move(m_startupTeams);
      m_teamsLoaded = true;
    }

    btInitOk = m_startupBtInitOk;
  }

  LOG_INFO() << "Startup cache loaded. themes=" << m_themes.size()
             << " patterns=" << m_pattern.size()
             << " teams=" << m_teams.size();

  if (m_mobileLightsPoller) {
    m_mobileLightsPoller->setExtendedSnapshots(m_themes, m_pattern, m_teams);
  }

  if (!btInitOk) {
    LOG_ERROR() << "BTControl init failed";
  } else if (!m_shuttingDown) {
    LOG_INFO() << "BTControl initialized";
    m_btInitialized = true;
    startBluetoothTransition(m_options.bluetooth != 0);
  }

  if (m_dailySportsPoller && m_teamsLoaded)
    LOG_INFO() << "DailySportsPoller ready after startup cache load";

  m_startupCacheComplete = true;
  refreshStartupFinalLightStateIfReady();
  maybeStartStartupSync();
}

void MainWindow::startStartupHardwareApply() {
  if (m_startupHardwareThread.joinable())
    return;

  const Options startupOptions = m_options;

  m_startupHardwareThread = std::thread([this, startupOptions]() {
    const bool autoEnabled = startupOptions.sensor != 0;
    bool desiredPowerOn = startupOptions.on != 0;
    bool sensorRead = false;
    bool sensorWantedOn = desiredPowerOn;

    LOG_INFO() << "Startup light control: auto=" << startupOptions.sensor
               << " on=" << startupOptions.on;

    if (autoEnabled)
      LOG_INFO() << "Startup auto sensor will be evaluated after schedules";
    else
      LOG_INFO() << "Startup lights are DB-controlled unless schedule/sports overrides";

    bool powerReady = false;
    if (!m_shuttingDown)
      powerReady = setLightsPowerEnabled(desiredPowerOn);

    {
      std::lock_guard<std::mutex> lock(m_startupMutex);
      m_startupAutoEnabled = autoEnabled;
      m_startupDesiredPowerOn = desiredPowerOn;
      m_startupPowerReady = powerReady;
      m_startupSensorRead = sensorRead;
      m_startupSensorWantedOn = sensorWantedOn;
    }

    if (!m_shuttingDown)
      m_startupHardwareDispatcher.emit();
  });
}

void MainWindow::onStartupHardwareFinished() {
  if (m_startupHardwareThread.joinable())
    m_startupHardwareThread.join();

  bool desiredPowerOn = false;
  bool powerReady = false;
  bool autoEnabled = false;
  bool sensorRead = false;
  bool sensorWantedOn = false;

  {
    std::lock_guard<std::mutex> lock(m_startupMutex);
    autoEnabled = m_startupAutoEnabled;
    desiredPowerOn = m_startupDesiredPowerOn;
    powerReady = m_startupPowerReady;
    sensorRead = m_startupSensorRead;
    sensorWantedOn = m_startupSensorWantedOn;
  }

  if (sensorRead) {
    m_options.on = sensorWantedOn ? 1 : 0;
  }

  if (desiredPowerOn && !powerReady)
    LOG_WARN() << "Teensy was powered, but readiness check failed at startup";

  LOG_INFO() << "Final startup lights state: requested="
             << (desiredPowerOn ? "ON" : "OFF")
             << " ready=" << (powerReady ? "true" : "false")
             << " actual="
             << ((desiredPowerOn && powerReady) ? "ON" : "OFF");

  if (autoEnabled && !m_shuttingDown) {
    LOG_INFO() << "Starting light sensor thread because auto=1";
    m_lightSensorThread.start();
  } else {
    LOG_INFO() << "Light sensor thread not started because auto=0";
  }

  m_startupHardwareComplete = true;
  refreshStartupFinalLightStateIfReady();
  maybeStartStartupSync();
}

void MainWindow::refreshStartupFinalLightStateIfReady() {
  if (!m_startupCacheComplete || !m_startupHardwareComplete)
    return;

  LOG_INFO() << "Refreshing Home UI from final startup light priority state";
  applyCurrentScheduleState(true);
  updateLightShowState();
  updateLightsDependentUi();
}

void MainWindow::maybeStartStartupSync() {
  if (m_startupSyncStarted || !m_startupCacheComplete ||
      !m_startupHardwareComplete || !m_mobileLightsPoller) {
    return;
  }

  m_startupSyncStarted = true;
  m_mobileLightsPoller->requestStartupSync(m_ledInfo, m_options, m_schedule,
                                           m_themes, m_pattern, m_teams);
  m_mobileLightsPoller->start();
}

void MainWindow::ensureThemesLoaded() {
  if (m_themesLoaded)
    return;

  std::lock_guard<std::mutex> lock(m_startupMutex);
  if (m_themesLoaded)
    return;

  m_themes = readThemeColors(runtimeSettingsPath());
  m_themesLoaded = true;
}

void MainWindow::ensurePatternsLoaded() {
  if (m_patternsLoaded)
    return;

  std::lock_guard<std::mutex> lock(m_startupMutex);
  if (m_patternsLoaded)
    return;

  m_pattern = readPatternSpeeds(runtimeSettingsPath());
  m_patternsLoaded = true;
}

void MainWindow::ensureTeamsLoaded() {
  if (m_teamsLoaded)
    return;

  std::lock_guard<std::mutex> lock(m_startupMutex);
  if (m_teamsLoaded)
    return;

  m_teams = readTeams(sportsDbPath());
  m_teamsLoaded = true;
}

void MainWindow::startThreads() {
  LOG_INFO() << "Starting ClockThread";
  ClockThread::instance().start();
  ClockThread::instance().setSchedules(m_schedule);

  LOG_INFO() << "Starting DoorbellThread";
  m_doorbellThread.start();
  m_mobileLightsPoller =
      std::make_unique<MobileLightsPoller>(m_ledInfo, m_options, m_schedule);
  m_environmentThread.start();

  m_dailySportsPoller =
      std::make_unique<DailySportsPoller>(runtimeSettingsPath());

  m_liveGamePoller = std::make_unique<LiveGamePoller>(runtimeSettingsPath());
}

void MainWindow::startConnections() {
  LOG_INFO() << "Connecting MainWindow signals";

  m_newHourConn =
      ClockThread::instance().signal_new_hour().connect([this](int hour) {
        LOG_INFO() << "Top of the hour: " << hour;

        if (hour == 0 && m_dailySportsPoller && m_teamsLoaded)
          m_dailySportsPoller->runOnceAsync(m_teams);

        triggerHourlyGameDayAnimation();
      });
  m_newMinuteConn =
      ClockThread::instance().signal_new_minute().connect([this](int minute) {
        LOG_INFO() << "Top of the minute: " << minute;
      });
  m_newYearConn =
      ClockThread::instance().signal_new_year().connect([this](int year) {
        LOG_INFO() << "New year detected: " << year;
        onNewYear(year);
      });
  m_scheduleStartedConn = ClockThread::instance().signal_schedule_started().connect(
      sigc::mem_fun(*this, &MainWindow::onScheduleStarted));

  m_scheduleEndedConn = ClockThread::instance().signal_schedule_ended().connect(
      sigc::mem_fun(*this, &MainWindow::onScheduleEnded));

  m_doorbellConn = m_doorbellThread.signal_doorbell_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onDoorbellChanged));

  m_mobileOptionsConn = m_mobileLightsPoller->signal_options_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileOptionsChanged));

  m_mobileLedsConn = m_mobileLightsPoller->signal_leds_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileLEDsChanged));

  m_mobileSchedulesConn = m_mobileLightsPoller->signal_schedules_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileSchedulesChanged));

  m_mobileThemesConn = m_mobileLightsPoller->signal_themes_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileThemesChanged));

  m_mobilePatternsConn = m_mobileLightsPoller->signal_patterns_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobilePatternsChanged));

  m_mobileTeamsConn = m_mobileLightsPoller->signal_teams_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileTeamsChanged));

  m_mobileStartupSyncConn =
      m_mobileLightsPoller->signal_startup_sync_complete().connect(
          sigc::mem_fun(*this, &MainWindow::onStartupSyncComplete));

  m_btUiDispatcher.connect(
      sigc::mem_fun(*this, &MainWindow::onBluetoothWorkerFinished));
  m_btManualUiDispatcher.connect(
      sigc::mem_fun(*this, &MainWindow::onBluetoothManualWorkerFinished));

  m_powerChangedConn = m_powerSwitch.signal_power_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onPowerSwitchChanged));

  m_btPowerChangedConn = m_btControl.signal_power_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onBluetoothPowerChanged));

  m_lightSensorConn = m_lightSensorThread.signal_sensor_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onLightSensorChanged));

  m_environmentConn = m_environmentThread.signal_environment_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onEnvironmentChanged));

  if (m_dailySportsPoller) {
    m_sportsGamesConn = m_dailySportsPoller->signal_games_checked().connect(
        sigc::mem_fun(*this, &MainWindow::onSportsGamesChecked));
  }

  if (m_liveGamePoller) {
    m_sportsLiveUpdateConn = m_liveGamePoller->signal_live_update().connect(
        sigc::mem_fun(*this, &MainWindow::onSportsLiveUpdate));
    m_sportsHomeScoreConn = m_liveGamePoller->signal_home_score().connect(
        sigc::mem_fun(*this, &MainWindow::onSportsHomeScore));
    m_sportsGameFinishedConn = m_liveGamePoller->signal_game_finished().connect(
        sigc::mem_fun(*this, &MainWindow::onSportsGameFinished));
  }

}

void MainWindow::buildShell() {
  LOG_INFO() << "Building shell";

  add(m_overlay);

  buildOverlay();
  buildStack();
  buildPages();
  connectPageSignals();
}

void MainWindow::buildOverlay() {
  LOG_INFO() << "Building overlay";

  m_overlay.add(m_stack);
  m_overlay.add_overlay(m_toast);

  m_toast.hideMessage();

  auto cancelBtn = Gtk::manage(
      new ImageButton(std::string(ICON_PATH) + "/cancel.png", 72, 0, 0));
  cancelBtn->set_can_focus(false);
  cancelBtn->get_style_context()->add_class("destructive-action");
  cancelBtn->signal_clicked().connect(
      sigc::mem_fun(*this, &MainWindow::cancelRestartCountdown));
}

void MainWindow::buildStack() {
  LOG_INFO() << "Building stack";

  m_stack.set_transition_type(Gtk::STACK_TRANSITION_TYPE_NONE);
  m_stack.set_transition_duration(250);
  m_stack.set_hexpand(true);
  m_stack.set_vexpand(true);
}

void MainWindow::buildPages() {
  LOG_INFO() << "Building persistent pages";

  m_homePage = Gtk::manage(new Home());
  m_clockPage = Gtk::manage(new ClockScreen());

  m_stack.add(*m_homePage, "home");
  m_stack.add(*m_clockPage, "clock");

  LOG_INFO() << "Persistent pages added: home, clock";
}

void MainWindow::connectPageSignals() {
  LOG_INFO() << "Connecting page signals";

  if (m_homePage) {
    m_homePage->signal_delta_all_requested().connect(
      [this]() {
          if (!requireLightsOnForHomeAction())
            return;
          showDeltaAllPage();
        });

    m_homePage->signal_delta_group_requested().connect(
      [this]() {
          if (!requireLightsOnForHomeAction())
            return;
          showDeltaGroupPage();
        });

    m_homePage->signal_themes_requested().connect(
        [this]() {
          if (!requireLightsOnForHomeAction())
            return;
          showThemesPage();
        });

    m_homePage->signal_patterns_requested().connect(
        [this]() {
          if (!requireLightsOnForHomeAction())
            return;
          showPatternPage();
        });

    m_homePage->signal_settings_requested().connect(
        [this]() { showSettingsPage(); });
  } else {
    LOG_ERROR() << "m_homePage is null in connectPageSignals()";
  }

  if (m_clockPage) {
    m_clockPage->signal_dismiss_requested().connect(
        [this]() { dismissClockPage(); });

    m_clockPage->setEnvProvider([this](float &tempF, float &humidity) -> bool {
#ifdef MOCK_HARDWARE
      tempF = 71.8f;
      humidity = 43.0f;
      return true;
#else
      tempF = m_lastEnvironmentReading.temperatureF;
      humidity = m_lastEnvironmentReading.humidity;
      return true;
#endif
    });
  } else {
    LOG_ERROR() << "m_clockPage is null in connectPageSignals()";
  }
}

void MainWindow::onLightSensorChanged(bool sensorWantsLightsOn) {
  LOG_INFO() << "Light sensor changed -> "
             << (sensorWantsLightsOn ? "ON" : "OFF");

  if (!m_liveSportsTeams.empty() || anyNormalScheduleActive()) {
    applyCurrentScheduleState();
    updateLightsDependentUi();
    return;
  }

  m_options.on = sensorWantsLightsOn ? 1 : 0;
  persistOptions(m_options);

  if (setLightsPowerEnabled(sensorWantsLightsOn)) {
    if (sensorWantsLightsOn)
      applyCurrentScheduleState();
    updateLightShowState();
  }
  updateLightsDependentUi();
}

void MainWindow::onEnvironmentChanged(EnvironmentThread::Reading reading) {
  m_lastEnvironmentReading = reading;

  LOG_INFO() << "Environment: " << reading.temperatureF << "F, "
             << reading.humidity << "%, " << reading.pressureHPa << " hPa";

  if (m_clockPage) {
    m_clockPage->setTempHumidity(reading.temperatureF, reading.humidity);
  }
}

bool MainWindow::persistOptions(const Options &options, bool pushToServer) {
  if (!writeOptions(runtimeSettingsPath(), options)) {
    LOG_ERROR() << "Failed to persist options locally";
    return false;
  }

  if (pushToServer && m_runtimeServerSyncEnabled && m_mobileLightsPoller) {
    m_mobileLightsPoller->pushOptionsUpdate(options);
  }

  return true;
}

bool MainWindow::persistLEDInfo(const std::vector<LEDData> &ledInfo,
                                bool pushToServer) {
  if (!writeLEDInfo(runtimeSettingsPath(), ledInfo)) {
    LOG_ERROR() << "Failed to persist LED info locally";
    return false;
  }

  if (pushToServer && m_runtimeServerSyncEnabled && m_mobileLightsPoller) {
    m_mobileLightsPoller->pushLedUpdate(ledInfo);
  }

  return true;
}

bool MainWindow::persistSchedule(const std::vector<Schedule> &schedule,
                                 bool pushToServer) {
  if (!writeSchedule(runtimeSettingsPath(), schedule)) {
    LOG_ERROR() << "Failed to persist schedules locally";
    return false;
  }

  if (pushToServer && m_runtimeServerSyncEnabled && m_mobileLightsPoller) {
    m_mobileLightsPoller->pushScheduleUpdate(schedule);
  }

  return true;
}

bool MainWindow::persistThemes(const std::vector<Theme> &themes,
                               bool pushToServer) {
  if (!writeThemeColors(runtimeSettingsPath(), themes)) {
    LOG_ERROR() << "Failed to persist themes locally";
    return false;
  }

  if (pushToServer && m_runtimeServerSyncEnabled && m_mobileLightsPoller) {
    m_mobileLightsPoller->pushThemesUpdate(themes);
  }

  return true;
}

bool MainWindow::persistPatterns(const std::vector<Pattern> &patterns,
                                 bool pushToServer) {
  if (!writePatternSpeeds(runtimeSettingsPath(), patterns)) {
    LOG_ERROR() << "Failed to persist patterns locally";
    return false;
  }

  if (pushToServer && m_runtimeServerSyncEnabled && m_mobileLightsPoller) {
    m_mobileLightsPoller->pushPatternsUpdate(patterns);
  }

  return true;
}

bool MainWindow::persistTeams(const std::vector<TeamRecord> &teams,
                              bool pushToServer) {
  std::vector<TeamRecord> oldTeams = readTeams(sportsDbPath());

  if (!writeTeams(sportsDbPath(), teams)) {
    LOG_ERROR() << "Failed to persist teams locally";
    return false;
  }

  for (const auto &oldTeam : oldTeams) {
    auto it = std::find_if(teams.begin(), teams.end(),
                           [&](const TeamRecord &team) {
                             return team.id == oldTeam.id;
                           });
    if (oldTeam.id > 0 && it == teams.end()) {
      deleteSportsTeamFromTeensy(oldTeam.id, oldTeam.name);
    }
  }

  if (pushToServer && m_runtimeServerSyncEnabled && m_mobileLightsPoller) {
    m_mobileLightsPoller->pushTeamsUpdate(teams);
  }

  return true;
}

void MainWindow::onMobileOptionsChanged(const Options &options) {
  if (m_options == options)
    return;
  const Options previous = m_options;
  m_options = options;
  LOG_INFO() << "Applying remote options changes locally";
  persistOptions(m_options, false);

  if (previous.sensor != m_options.sensor) {
    if (m_options.sensor) {
      const bool sensorWantsLightsOn = m_lightSensorThread.readOnce();
      m_options.on = sensorWantsLightsOn ? 1 : 0;
      persistOptions(m_options);
      if (setLightsPowerEnabled(sensorWantsLightsOn) && sensorWantsLightsOn)
        applyCurrentScheduleState();
      m_lightSensorThread.start();
    } else {
      m_lightSensorThread.stop();
    }
  }

  if (previous.on != m_options.on && !m_options.sensor) {
    const bool enabled = m_options.on != 0;
    if (setLightsPowerEnabled(enabled) && enabled)
      applyCurrentScheduleState();
  }

  if (previous.theme != m_options.theme || previous.ptrn != m_options.ptrn) {
    if (lightsAreOn())
      m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
    else
      LOG_INFO() << "Remote theme/pattern write skipped because lights are off";
  }

  if (previous.bluetooth != m_options.bluetooth) {
    startBluetoothTransition(m_options.bluetooth != 0);
  }

  updateLightShowState();
  updateLightsDependentUi();
}

void MainWindow::onMobileLEDsChanged(const std::vector<LEDData> &ledInfo) {
  if (m_ledInfo == ledInfo)
    return;
  LOG_INFO() << "Applying remote LED changes locally";
  m_ledInfo = ledInfo;
  persistLEDInfo(m_ledInfo, false);
  if (lightsAreOn())
    restoreManualLedsAsync();
  else
    LOG_INFO() << "Remote LED restore deferred because lights are off";
}

void MainWindow::onMobileSchedulesChanged(
    const std::vector<Schedule> &schedule) {
  if (m_schedule == schedule)
    return;
  LOG_INFO() << "Applying remote schedule changes locally";
  m_schedule = schedule;
  persistSchedule(m_schedule, false);
  ClockThread::instance().setSchedules(m_schedule);
  if (lightsAreOn())
    applyCurrentScheduleState();
}

void MainWindow::onMobileThemesChanged(const std::vector<Theme> &themes) {
  LOG_INFO() << "Applying remote themes changes locally";
  std::vector<Theme> changedThemes;
  for (const auto &theme : themes) {
    const auto old = std::find_if(m_themes.begin(), m_themes.end(),
                                  [&theme](const Theme &existing) {
                                    return existing.id == theme.id;
                                  });
    if (old == m_themes.end() || !themeRowsEqual(*old, theme))
      changedThemes.push_back(theme);
  }

  m_themes = themes;
  m_themesLoaded = true;
  persistThemes(m_themes, false);

  if (!changedThemes.empty())
    queueThemeUploadsToTeensy(changedThemes);
}

void MainWindow::onMobilePatternsChanged(
    const std::vector<Pattern> &patterns) {
  LOG_INFO() << "Applying remote pattern changes locally";
  m_pattern = patterns;
  m_patternsLoaded = true;
  persistPatterns(m_pattern, false);
  if (lightsAreOn())
    sendPatternSpeedsToTeensyAsync(m_pattern);
  else
    LOG_INFO() << "Remote pattern speed transfer deferred because lights are off";
}

void MainWindow::onMobileTeamsChanged(const std::vector<TeamRecord> &teams) {
  LOG_INFO() << "Applying remote team changes locally";
  m_teams = teams;
  m_teamsLoaded = true;
  persistTeams(m_teams, false);
  if (m_dailySportsPoller)
    m_dailySportsPoller->runOnceAsync(m_teams);
  refreshLiveSportsPoller();
}

void MainWindow::onStartupSyncComplete() {
  m_runtimeServerSyncEnabled = true;

  if (m_dailySportsPoller && m_teamsLoaded) {
    m_dailySportsPoller->start(m_teams);
    m_dailySportsPoller->runOnceAsync(m_teams);
  }
}

void MainWindow::showPage(const std::string &pageName) {
  LOG_INFO() << "Switching to page: " << pageName;

  if (pageName != "clock")
    resetIdleClockTimer();

  m_stack.set_visible_child(pageName);
  show_all_children();
}

void MainWindow::showHomePage() {
  LOG_INFO() << "Showing home page";

  m_clockVisible = false;

  resetIdleClockTimer();
  m_stack.set_visible_child(*m_homePage);
  updateLightsDependentUi();
  m_stack.show_all_children();
  show_all_children();
  m_stack.queue_draw();
}

void MainWindow::showSettingsPage() {
  LOG_INFO() << "showSettingsPage requested";

  destroyTemporaryPage("settings");

  m_settingsPage = Gtk::manage(
      new Settings(ICON_PATH, m_options, m_bluetoothState,
                   m_lightShowRunning.load()));
  m_settingsPage->set_bluetooth_enabled(m_btInitialized && !m_btBusy.load());

  m_settingsPage->signal_auto_sensor_toggled().connect([this](bool enabled) {
    m_options.sensor = enabled ? 1 : 0;
    persistOptions(m_options);

    if (enabled) {
      const bool sensorWantsLightsOn = m_lightSensorThread.readOnce();

      m_options.on = sensorWantsLightsOn ? 1 : 0;
      persistOptions(m_options);

      if (setLightsPowerEnabled(sensorWantsLightsOn) && sensorWantsLightsOn)
        applyCurrentScheduleState();

      m_lightSensorThread.start();
    } else {
      m_lightSensorThread.stop();

      // Leave lights in their current state.
      // User can use the normal Lights button after this.
    }

    updateLightShowState();
    updateLightsDependentUi();
  });

  m_settingsPage->signal_lights_toggled().connect([this](bool enabled) {
    if (setLightsPowerEnabled(enabled)) {
      m_options.on = enabled ? 1 : 0;
      persistOptions(m_options);
      if (enabled)
        applyCurrentScheduleState();
      updateLightShowState();
      updateLightsDependentUi();
    } else {
      LOG_WARN() << "Failed to toggle lights: " << m_powerSwitch.lastError();
    }
  });

  m_settingsPage->signal_bluetooth_toggled().connect(
      [this](bool enabled) {
        m_options.bluetooth = enabled ? 1 : 0;
        persistOptions(m_options);
        startBluetoothTransition(enabled);
      });

  m_settingsPage->signal_restart_requested().connect([this]() {
    if (m_settingsPage)
      m_settingsPage->set_restart_enabled(false);
    startRestartCountdown();
  });

  m_settingsPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_settingsPage, "settings");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_settingsPage);
  m_stack.queue_draw();

  m_settingsPage->signal_edit_theme_requested().connect(
      [this]() { showEditThemesPage(); });

  m_settingsPage->signal_edit_pattern_requested().connect(
      [this]() { showEditPatternPage(); });

  m_settingsPage->signal_edit_teams_requested().connect(
      [this]() { showTeamListPage(); });

  m_settingsPage->signal_bluetooth_controls_requested().connect(
      [this]() { showBluetoothControlsPage(); });

  m_settingsPage->signal_hardware_config_requested().connect(
      [this]() { showHardwareConfigPage(); });

  m_settingsPage->signal_debug_requested().connect(
      [this]() { showDebugPage(); });
}

void MainWindow::showHardwareConfigPage() {
  LOG_INFO() << "showHardwareConfigPage requested";

  removeHardwareConfigPage();

  const auto [numLeds, numShiftRegs] = readHardwareConfigDefaults();
  m_hardwareConfigPage =
      Gtk::manage(new HardwareConfigPage(numLeds, numShiftRegs));

  m_hardwareConfigPage->signal_cancel_requested().connect([this]() {
    removeHardwareConfigPage();
    showSettingsPage();
  });

  m_hardwareConfigPage->signal_apply_requested().connect(
      [this](int leds, int shiftRegs) {
        sendHardwareConfigToTeensyAsync(leds, shiftRegs);
      });

  m_stack.add(*m_hardwareConfigPage, "hardware_config");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_hardwareConfigPage);
  m_stack.queue_draw();
}

void MainWindow::showDebugPage() {
  destroyTemporaryPage("debug");

  m_debugPage = Gtk::manage(new DebugPage());
  m_debugPage->signal_lightshow_requested().connect(
      [this]() { showLightShowSettingsPage(); });
  m_debugPage->signal_sports_requested().connect(
      [this]() { showDebugSportsPage(); });
  m_debugPage->signal_back_requested().connect([this]() { showSettingsPage(); });

  m_stack.add(*m_debugPage, "debug");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_debugPage);
  m_stack.queue_draw();
}

void MainWindow::showDebugSportsPage() {
  destroyTemporaryPage("debug_sports");

  m_debugSportsPage = Gtk::manage(new DebugSportsPage());
  m_debugSportsPage->signal_update_all_requested().connect(
      [this]() { runDebugSportsScheduleUpdate(""); });
  m_debugSportsPage->signal_update_team_requested().connect(
      [this](const std::string &teamName) {
        runDebugSportsScheduleUpdate(teamName);
      });
  m_debugSportsPage->signal_game_day_requested().connect(
      [this](const std::string &teamName) { runDebugGameDayAnimation(teamName); });
  m_debugSportsPage->signal_score_placeholder_requested().connect(
      [this]() { runDebugScorePlaceholder(); });
  m_debugSportsPage->signal_back_requested().connect(
      [this]() { showDebugPage(); });

  m_stack.add(*m_debugSportsPage, "debug_sports");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_debugSportsPage);
  m_stack.queue_draw();
}

void MainWindow::runDebugSportsScheduleUpdate(const std::string &teamName) {
  ensureTeamsLoaded();
  if (!m_dailySportsPoller) {
    showShortToast("Sports poller unavailable");
    return;
  }

  std::vector<TeamRecord> teams;
  for (const auto &team : m_teams) {
    if (teamName.empty() || team.name.find(teamName) != std::string::npos)
      teams.push_back(team);
  }

  if (teams.empty()) {
    showShortToast("No matching team");
    return;
  }

  m_dailySportsPoller->runOnceAsync(teams);
  showShortToast(teamName.empty() ? "Updating all teams"
                                  : "Updating " + teamName);
}

void MainWindow::snapshotDebugLightState() {
  m_debugSavedOptions = m_options;
  m_debugSavedPowerEnabled = m_lightsPowerEnabled;
  m_debugLightSnapshotValid = true;
}

void MainWindow::restoreDebugLightState() {
  if (!m_debugLightSnapshotValid) {
    applyCurrentScheduleState();
    return;
  }

  m_debugLightSnapshotValid = false;

  if (!m_liveSportsTeams.empty() || anyNormalScheduleActive() ||
      m_debugSavedOptions.sensor) {
    m_options = m_debugSavedOptions;
    persistOptions(m_options, false);
    applyCurrentScheduleState();
    return;
  }

  m_options = m_debugSavedOptions;
  persistOptions(m_options, false);

  if (!m_debugSavedPowerEnabled) {
    setLightsPowerEnabled(false);
    updateLightShowState();
    updateLightsDependentUi();
    return;
  }

  if (!setLightsPowerEnabled(true))
    return;

  if (m_options.theme > 0)
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
  else
    restoreManualLedsAsync();

  updateLightShowState();
  updateLightsDependentUi();
}

void MainWindow::runDebugGameDayAnimation(const std::string &teamName) {
  if (m_sportsAnimationRunning) {
    showShortToast("Sports animation already running");
    return;
  }

  ensureTeamsLoaded();
  auto teamIt = std::find_if(m_teams.begin(), m_teams.end(),
                             [&teamName](const TeamRecord &team) {
                               return team.name.find(teamName) !=
                                      std::string::npos;
                             });
  if (teamIt == m_teams.end())
    teamIt = std::find_if(m_teams.begin(), m_teams.end(),
                          [](const TeamRecord &team) {
                            return team.name.find("Mariners") !=
                                   std::string::npos;
                          });
  if (teamIt == m_teams.end()) {
    showShortToast("No teams configured");
    return;
  }

  if (m_debugActionTimeoutConn.connected())
    m_debugActionTimeoutConn.disconnect();
  if (m_gameDayPage)
    m_gameDayPage->stop();
  destroyTemporaryPage("debug_action");

  snapshotDebugLightState();

  TeamRecord team = *teamIt;
  const bool seattleHome = team.homeAway != "away";
  const auto seattleName =
      splitCityTeam(team.name, team.teamCode.empty() ? "Seattle" : team.teamCode);

  GameDayTeamVisual seattleVisual;
  seattleVisual.city = seattleName.first;
  seattleVisual.name = seattleName.second;
  seattleVisual.logoPath = getTeamLogoPath(team);
  seattleVisual.flagPath = gameDayTeamAssetPath(team, "_f.png");
  seattleVisual.colors = colorsForSide(team, seattleHome);
  if (seattleVisual.colors.empty())
    seattleVisual.colors = neutralOpponentColors();

  GameDayTeamVisual opponentVisual;
  opponentVisual.city = "Debug";
  opponentVisual.name = "Opponent";
  const std::string debugOpponentId =
      uppercaseCopy(team.league) == "NFL"
          ? "SF"
          : (uppercaseCopy(team.league) == "NHL" ? "VAN" : "117");
  TeamRecord debugOpponentTeam;
  debugOpponentTeam.league = team.league;
  debugOpponentTeam.teamCode = debugOpponentId;
  debugOpponentTeam.apiTeamId = debugOpponentId;
  opponentVisual.logoPath = getTeamLogoPath(debugOpponentTeam);
  opponentVisual.flagPath =
      gameDayTeamAssetPath(team.league, debugOpponentId, "_f.png");
  opponentVisual.colors = neutralOpponentColors();

  GameDayAnimationData data;
  data.league = uppercaseCopy(team.league);
  data.gameTimeDisplay = "@ 7:00 PM";
  data.seattleTeamCode = team.teamCode;
  data.seattleTeamColors = allTeamColors(team);
  if (data.seattleTeamColors.empty())
    data.seattleTeamColors = seattleVisual.colors;

  if (seattleHome) {
    data.left = opponentVisual;
    data.right = seattleVisual;
  } else {
    data.left = seattleVisual;
    data.right = opponentVisual;
  }

  if (setLightsPowerEnabled(true) && syncSportsTeamToTeensy(team)) {
    m_teensyClient.activateSportsTeam(static_cast<uint16_t>(team.id),
                                      seattleHome, kAlternatePatternId);
  }

  if (data.league == "NHL")
    m_gameDayPage = Gtk::manage(new HockeyGameDayAnimation(data));
  else if (data.league == "NFL")
    m_gameDayPage = Gtk::manage(new FootballGameDayAnimation(data));
  else
    m_gameDayPage = Gtk::manage(new BaseballGameDayAnimation(data));

  m_gameDayPage->start();
  m_stack.add(*m_gameDayPage, "debug_action");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_gameDayPage);
  m_stack.queue_draw();

  m_debugActionTimeoutConn = Glib::signal_timeout().connect_seconds(
      [this]() -> bool {
        finishDebugTimedAction();
        return false;
      },
      30);
}

void MainWindow::runDebugScorePlaceholder() {
  if (m_debugActionTimeoutConn.connected())
    m_debugActionTimeoutConn.disconnect();
  destroyTemporaryPage("debug_action");

  snapshotDebugLightState();

  m_debugPlaceholderPage = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
  m_debugPlaceholderPage->set_halign(Gtk::ALIGN_CENTER);
  m_debugPlaceholderPage->set_valign(Gtk::ALIGN_CENTER);
  auto *label = Gtk::manage(new Gtk::Label("Score Animation (Not Implemented)"));
  label->set_margin_top(20);
  label->set_margin_bottom(20);
  m_debugPlaceholderPage->pack_start(*label, Gtk::PACK_SHRINK);

  m_stack.add(*m_debugPlaceholderPage, "debug_action");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_debugPlaceholderPage);
  m_stack.queue_draw();

  m_debugActionTimeoutConn = Glib::signal_timeout().connect_seconds(
      [this]() -> bool {
        finishDebugTimedAction();
        return false;
      },
      10);
}

void MainWindow::finishDebugTimedAction() {
  if (m_debugActionTimeoutConn.connected())
    m_debugActionTimeoutConn.disconnect();

  if (m_gameDayPage)
    m_gameDayPage->stop();

  destroyTemporaryPage("debug_action");
  restoreDebugLightState();
  showDebugSportsPage();
}

void MainWindow::setBluetoothButtonEnabled(bool enabled) {
  if (m_settingsPage)
    m_settingsPage->set_bluetooth_enabled(enabled);
}

bool MainWindow::setAmpEnabledForBluetooth(bool enabled,
                                           const std::string &reason) {
  if (m_ampEnabled.load() == enabled)
    return true;

  if (!m_ampSwitch.setEnabled(enabled)) {
    LOG_WARN() << "Failed to turn amp " << (enabled ? "on" : "off")
               << " (" << reason << "): " << m_ampSwitch.lastError();
    return false;
  }

  m_ampEnabled = enabled;
  LOG_INFO() << "AMP " << (enabled ? "ON" : "OFF") << " (" << reason << ")";
  updateLightShowState();
  return true;
}

void MainWindow::handleBluetoothConnected() {
  m_bluetoothConnected = true;
  cancelBluetoothIdleTimeout();
  setAmpEnabledForBluetooth(true, "Bluetooth connected");
}

void MainWindow::handleBluetoothDisconnected() {
  if (m_bluetoothConnected.exchange(false))
    LOG_INFO() << "Bluetooth disconnected; starting idle timeout";
  startBluetoothIdleTimeout();
  updateLightShowState();
}

void MainWindow::startBluetoothIdleTimeout() {
  if (!m_bluetoothState)
    return;

  if (!m_btIdleTimeoutActive) {
    m_btLastDisconnectTime = std::chrono::steady_clock::now();
    m_btIdleTimeoutActive = true;
  }

  if (!m_bluetoothIdleTimeoutConn.connected()) {
    m_bluetoothIdleTimeoutConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &MainWindow::onBluetoothIdleTimeoutTick), 5);
  }
}

void MainWindow::cancelBluetoothIdleTimeout() {
  m_btIdleTimeoutActive = false;
  if (m_bluetoothIdleTimeoutConn.connected())
    m_bluetoothIdleTimeoutConn.disconnect();
  if (m_bluetoothEnableTimeoutConn.connected())
    m_bluetoothEnableTimeoutConn.disconnect();
}

bool MainWindow::onBluetoothIdleTimeoutTick() {
  if (!m_bluetoothState || m_bluetoothConnected.load()) {
    m_btIdleTimeoutActive = false;
    return false;
  }

  const auto elapsed = std::chrono::steady_clock::now() - m_btLastDisconnectTime;
  if (elapsed < std::chrono::minutes(5))
    return true;

  if (m_btBusy.load())
    return true;

  LOG_WARN() << "Bluetooth idle timeout reached, shutting down";
  m_btIdleTimeoutActive = false;
  startBluetoothTransition(false);
  return false;
}

void MainWindow::startBluetoothTransition(bool enable) {
  if (!m_btInitialized) {
    LOG_WARN() << "Bluetooth transition ignored before BTControl init";
    return;
  }

  if (m_btBusy.exchange(true)) {
    LOG_WARN() << "Bluetooth transition ignored because one is already running";
    return;
  }

  setBluetoothButtonEnabled(false);

  if (m_btWorker.joinable())
    m_btWorker.join();

  m_btWorker = std::thread([this, enable]() {
    if (m_shuttingDown) {
      std::lock_guard<std::mutex> lock(m_btResultMutex);
      m_btResultEnabled = enable;
      m_btResultSuccess = false;
      m_btResultToast.clear();
      return;
    }

    bool success = false;
    std::string toast;

    LOG_INFO() << "Bluetooth worker started -> " << (enable ? "ON" : "OFF");

    if (enable) {
      if (!m_btControl.powerOn()) {
        LOG_ERROR() << (m_btControl.lastError().empty()
                            ? "Failed to power on bluetooth"
                            : m_btControl.lastError());
      } else {
        m_bluetoothState = 1;

        bool setupOk = true;
        if (!m_btControl.setPairable(true)) {
          LOG_WARN() << "Failed to set bluetooth pairable on";
          setupOk = false;
        }

        if (!m_btControl.setDiscoverable(true)) {
          LOG_WARN() << "Failed to set bluetooth discoverable on";
          setupOk = false;
        }

        if (!m_btControl.startScan()) {
          LOG_WARN() << "Failed to start bluetooth scan";
          setupOk = false;
        } else {
          LOG_INFO() << "Bluetooth pairing mode on";
        }

        auto connectedDevice = m_btControl.getConnectedDevice();
        if (setupOk && connectedDevice) {
          const std::string sink = chooseBluetoothSink();
          LOG_INFO() << "Bluetooth playback sink: "
                     << (sink.empty() ? "<not found>" : sink);
          if (!sink.empty() && !setBluetoothPlaybackSink(sink)) {
            LOG_WARN() << "Failed to set Bluetooth playback sink: " << sink;
          }
          toast = bluetoothToastMessage(m_btControl.lastStatus());
        } else if (setupOk) {
          toast = "No device connected";
        }

        if (!setupOk) {
          m_btControl.stopScan();
          m_btControl.setDiscoverable(false);
          m_btControl.setPairable(false);
          m_btControl.powerOff();
          m_bluetoothState = 0;
        }

        success = setupOk;
      }

    } else {
      if (!m_ampSwitch.setEnabled(false)) {
        LOG_WARN() << "Failed to turn amp off (Bluetooth disabled): "
                   << m_ampSwitch.lastError();
      } else {
        m_ampEnabled = false;
      }
      m_bluetoothConnected = false;
      m_btControl.disconnectAllDevices();
      m_btControl.stopScan();
      m_btControl.setDiscoverable(false);
      m_btControl.setPairable(false);
      m_bluezAgent.stop();

      if (!m_btControl.powerOff()) {
        LOG_ERROR() << (m_btControl.lastError().empty()
                            ? "Failed to power off bluetooth"
                            : m_btControl.lastError());
      } else {
        m_bluetoothState = 0;
        success = true;
      }
    }

    {
      std::lock_guard<std::mutex> lock(m_btResultMutex);
      m_btResultEnabled = enable;
      m_btResultSuccess = success;
      m_btResultToast = toast;
    }

    m_btUiDispatcher.emit();
  });
}

void MainWindow::onBluetoothWorkerFinished() {
  if (m_btWorker.joinable())
    m_btWorker.join();

  bool enable = false;
  bool success = false;
  std::string toast;

  {
    std::lock_guard<std::mutex> lock(m_btResultMutex);
    enable = m_btResultEnabled;
    success = m_btResultSuccess;
    toast = m_btResultToast;
  }

  if (success && enable) {
    if (m_bluetoothPollConn.connected())
      m_bluetoothPollConn.disconnect();

    m_bluetoothPollConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &MainWindow::onBluetoothPollTick), 2);

    auto connectedDevice = m_btControl.getConnectedDevice();
    if (connectedDevice)
      handleBluetoothConnected();
    else
      handleBluetoothDisconnected();
  } else {
    stopBluetoothPolling();
    cancelBluetoothIdleTimeout();
    m_bluetoothConnected = false;
    setAmpEnabledForBluetooth(false, "Bluetooth disabled");
  }

  if (!toast.empty())
    showShortToast(toast);

  setBluetoothButtonEnabled(true);
  m_btBusy = false;

  LOG_INFO() << "Bluetooth worker finished success=" << success
             << " enabled=" << enable;
  if (success && m_options.bluetooth != (enable ? 1 : 0)) {
    m_options.bluetooth = enable ? 1 : 0;
    persistOptions(m_options);
  }
  updateLightShowState();
  updateLightsDependentUi();
}

void MainWindow::startBluetoothManualConnect(const BTDevice &device) {
  if (m_btBusy.exchange(true)) {
    LOG_WARN() << "Bluetooth manual connect ignored because BT is busy";
    return;
  }

  if (m_bluetoothDeviceDetailPage)
    m_bluetoothDeviceDetailPage->set_busy(true);

  if (m_btWorker.joinable())
    m_btWorker.join();

  m_btWorker = std::thread([this, device]() {
    bool success = false;
    std::string toast;
    const bool wasPowered = m_btControl.isPoweredOn();
    bool powered = false;

    if (!m_shuttingDown) {
      success = m_btControl.connectSavedDevice(device.macAddress);
      if (!success && !wasPowered) {
        m_btControl.disconnectAllDevices();
        m_btControl.stopScan();
        m_btControl.setDiscoverable(false);
        m_btControl.setPairable(false);
        m_btControl.powerOff();
      }
      powered = m_btControl.isPoweredOn();
      m_bluetoothState = powered ? 1 : 0;

      auto connected = m_btControl.getConnectedDevice();
      if (success && connected) {
        const std::string sink = chooseBluetoothSink();
        LOG_INFO() << "Bluetooth playback sink: "
                   << (sink.empty() ? "<not found>" : sink);
        if (!sink.empty() && !setBluetoothPlaybackSink(sink)) {
          LOG_WARN() << "Failed to set Bluetooth playback sink: " << sink;
        }
        toast = bluetoothToastMessage(m_btControl.lastStatus());
      } else if (!success) {
        toast = "Device Unavailable";
      }
    }

    {
      std::lock_guard<std::mutex> lock(m_btResultMutex);
      m_btResultEnabled = powered;
      m_btResultSuccess = success;
      m_btResultToast = toast;
      m_btManualAction = BluetoothManualAction::Connect;
    }

    m_btManualUiDispatcher.emit();
  });
}

void MainWindow::startBluetoothManualDelete(const BTDevice &device) {
  if (m_btBusy.exchange(true)) {
    LOG_WARN() << "Bluetooth manual delete ignored because BT is busy";
    return;
  }

  if (m_bluetoothDeviceDetailPage)
    m_bluetoothDeviceDetailPage->set_busy(true);

  if (m_btWorker.joinable())
    m_btWorker.join();

  m_btWorker = std::thread([this, device]() {
    bool success = false;

    if (!m_shuttingDown) {
      success = m_btControl.deleteSavedDevice(device.macAddress);
      m_bluetoothState = m_btControl.isPoweredOn() ? 1 : 0;

      auto connected = m_btControl.getConnectedDevice();
      if (!connected)
        m_bluetoothConnected = false;
    }

    {
      std::lock_guard<std::mutex> lock(m_btResultMutex);
      m_btResultEnabled = true;
      m_btResultSuccess = success;
      m_btResultToast.clear();
      m_btManualAction = BluetoothManualAction::Delete;
    }

    m_btManualUiDispatcher.emit();
  });
}

void MainWindow::onBluetoothManualWorkerFinished() {
  if (m_btWorker.joinable())
    m_btWorker.join();

  bool success = false;
  std::string toast;
  bool powered = false;
  BluetoothManualAction action = BluetoothManualAction::None;

  {
    std::lock_guard<std::mutex> lock(m_btResultMutex);
    success = m_btResultSuccess;
    toast = m_btResultToast;
    powered = m_btResultEnabled;
    action = m_btManualAction;
    m_btManualAction = BluetoothManualAction::None;
  }

  if (success && action == BluetoothManualAction::Connect) {
    if (m_bluetoothPollConn.connected())
      m_bluetoothPollConn.disconnect();

    m_bluetoothPollConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &MainWindow::onBluetoothPollTick), 2);

    if (m_bluetoothEnableTimeoutConn.connected())
      m_bluetoothEnableTimeoutConn.disconnect();

    if (!toast.empty())
      showShortToast(toast);

    if (m_btControl.getConnectedDevice())
      handleBluetoothConnected();
    else
      handleBluetoothDisconnected();
  } else if (action == BluetoothManualAction::Connect) {
    m_bluetoothConnected = false;
    setAmpEnabledForBluetooth(false, "Bluetooth connection failed");
    if (!toast.empty())
      showShortToast(toast);
  }

  if (action == BluetoothManualAction::Delete) {
    if (!m_btControl.getConnectedDevice())
      handleBluetoothDisconnected();
    removeBluetoothDeviceDetailPage();
    showBluetoothControlsPage();
  } else if (m_bluetoothDeviceDetailPage) {
    m_bluetoothDeviceDetailPage->set_busy(false);
  }

  m_btBusy = false;
  if (action == BluetoothManualAction::Connect &&
      m_options.bluetooth != (powered ? 1 : 0)) {
    m_options.bluetooth = powered ? 1 : 0;
    persistOptions(m_options);
  }
  updateLightShowState();
}

void MainWindow::showThemesPage() {
  if (!requireLightsOnForHomeAction())
    return;

  destroyTemporaryPage("themes");
  ensureThemesLoaded();

  m_themesPage = Gtk::manage(
      new Themes(std::string(ICON_PATH), m_themes, m_options.theme));

  m_themesPage->signal_theme_selected().connect([this](int index) {
    if (!requireLightsOnForHomeAction())
      return;

    m_options.theme = index;
    persistOptions(m_options);
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
  });

  m_themesPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_themesPage, "themes");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_themesPage);
  m_stack.queue_draw();
}

void MainWindow::showPatternPage() {
  LOG_INFO() << "showPatternPage requested";

  if (!requireLightsOnForHomeAction())
    return;

  destroyTemporaryPage("pattern");

  m_patternPage =
      Gtk::manage(new Patterns(std::string(ICON_PATH), m_options.ptrn));

  m_patternPage->signal_pattern_selected().connect([this](int index) {
    if (!requireLightsOnForHomeAction())
      return;

    m_options.ptrn = index;
    persistOptions(m_options);
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
  });

  m_patternPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_patternPage, "pattern");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_patternPage);
  m_stack.queue_draw();
}

void MainWindow::showDeltaAllPage() {
  LOG_INFO() << "showDeltaAllPage requested";

  if (!requireLightsOnForHomeAction())
    return;

  destroyTemporaryPage("delta_all");

  int r = 0, g = 0, b = 0;
  getAvgColor(-1, r, g, b);

  m_deltaAllPage = Gtk::manage(new DeltaAll(
      std::string(ICON_PATH), r, g, b, UiMetrics::color_picker_size(),
      UiMetrics::color_bar_size(), KEY_PAD_PIXEL_SIZE));

  m_deltaAllPage->signal_color_changed().connect([this](int r, int g, int b) {
    if (!requireLightsOnForHomeAction())
      return;

    for (int i = 0; i < NUM_OF_LEDS; i++) {
      m_ledInfo[i].redVal = r;
      m_ledInfo[i].grnVal = g;
      m_ledInfo[i].bluVal = b;
    }
    persistLEDInfo(m_ledInfo);
    auto mask =
        TeensyClient::mask24FromBitString(std::string(NUM_OF_LEDS, '1'));
    m_teensyClient.applyMaskedRGB(mask, r, g, b);
  });

  m_deltaAllPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_deltaAllPage, "delta_all");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_deltaAllPage);
  m_stack.queue_draw();
}

void MainWindow::showDeltaGroupPage() {
  LOG_INFO() << "showDeltaGroupPage requested";

  if (!requireLightsOnForHomeAction())
    return;

  destroyTemporaryPage("delta_group");

  std::array<DeltaGroup::GroupColor, 3> colors{};
  for (int group = 0; group < 3; ++group) {
    int r = 0, g = 0, b = 0;
    getAvgColor(group, r, g, b);
    colors[group] = {r, g, b};
  }

  m_deltaGroupPage = Gtk::manage(
      new DeltaGroup(std::string(ICON_PATH), m_groupSelection, colors,
                     UiMetrics::color_picker_size(),
                     UiMetrics::color_bar_size(), KEY_PAD_PIXEL_SIZE));

  m_deltaGroupPage->signal_group_color_changed().connect(
      [this](int group, int r, int g, int b) {
        if (!requireLightsOnForHomeAction())
          return;

        m_groupSelection = group;
        std::string maskString = "";
        for (int i = 0; i < NUM_OF_LEDS; i++) {
          if (m_ledInfo[i].group == group) {
            maskString += '1';
            m_ledInfo[i].redVal = r;
            m_ledInfo[i].grnVal = g;
            m_ledInfo[i].bluVal = b;
          } else
            maskString += '0';
        }
        persistLEDInfo(m_ledInfo);
        auto mask = TeensyClient::mask24FromBitString(maskString);
        m_teensyClient.applyMaskedRGB(mask, static_cast<uint8_t>(r),
                                      static_cast<uint8_t>(g),
                                      static_cast<uint8_t>(b));
      });

  m_deltaGroupPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_deltaGroupPage, "delta_group");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_deltaGroupPage);
  m_stack.queue_draw();
}

void MainWindow::createTeamListPage() {
  if (m_teamList)
    return;
  ensureTeamsLoaded();

  m_teamList = Gtk::manage(
      new TeamList(ICON_PATH, runtimeSettingsPath() + "/lights.db"));

  m_teamList->signal_cancel().connect([this]() {
    removeTeamListPage();
    showSettingsPage();
  });

  m_teamList->signal_add_team_requested().connect([this]() {
    TeamRecord blank;
    showEditTeamPage(blank);
  });

  m_teamList->signal_edit_team_requested().connect(
      [this](TeamRecord team) { showEditTeamPage(team); });
}

void MainWindow::showTeamListPage() {
  createTeamListPage();

  if (m_teamList)
    m_teamList->reload();

  if (!m_stack.get_child_by_name("team_list"))
    m_stack.add(*m_teamList, "team_list");

  m_stack.show_all_children();
  m_stack.set_visible_child(*m_teamList);
  m_stack.queue_draw();
}

void MainWindow::showBluetoothControlsPage() {
  LOG_INFO() << "showBluetoothControlsPage requested";

  removeBluetoothControlsPage();
  removeBluetoothDeviceDetailPage();

  if (m_btControl.isPoweredOn())
    m_btControl.refreshAllKnownDevices();

  std::vector<BTDevice> devices;
  for (const auto &device : m_btControl.getSavedDevicesInDisplayOrder()) {
    if (!device.paired)
      continue;
    devices.push_back(device);
    if (devices.size() >= 12)
      break;
  }

  m_bluetoothControlsPage =
      Gtk::manage(new BluetoothControls(ICON_PATH, devices));

  m_bluetoothControlsPage->signal_cancel().connect([this]() {
    removeBluetoothControlsPage();
    showSettingsPage();
  });

  m_bluetoothControlsPage->signal_device_selected().connect(
      [this](BTDevice device) { showBluetoothDeviceDetailPage(device); });

  m_stack.add(*m_bluetoothControlsPage, "bluetooth_controls");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_bluetoothControlsPage);
  m_stack.queue_draw();
}

void MainWindow::showBluetoothDeviceDetailPage(const BTDevice &device) {
  LOG_INFO() << "showBluetoothDeviceDetailPage requested";

  removeBluetoothDeviceDetailPage();

  m_bluetoothDeviceDetailPage =
      Gtk::manage(new BluetoothDeviceDetail(ICON_PATH, device));

  m_bluetoothDeviceDetailPage->signal_cancel().connect([this]() {
    removeBluetoothDeviceDetailPage();
    showBluetoothControlsPage();
  });

  m_bluetoothDeviceDetailPage->signal_connect_requested().connect(
      [this](BTDevice selected) { startBluetoothManualConnect(selected); });

  m_bluetoothDeviceDetailPage->signal_delete_requested().connect(
      [this](BTDevice selected) { startBluetoothManualDelete(selected); });

  m_stack.add(*m_bluetoothDeviceDetailPage, "bluetooth_device_detail");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_bluetoothDeviceDetailPage);
  m_stack.queue_draw();
}

void MainWindow::removeTeamListPage() {
  if (!m_teamList)
    return;

  m_stack.remove(*m_teamList);
  m_teamList = nullptr;
}

void MainWindow::removeBluetoothControlsPage() {
  if (!m_bluetoothControlsPage)
    return;

  m_stack.remove(*m_bluetoothControlsPage);
  m_bluetoothControlsPage = nullptr;
}

void MainWindow::removeBluetoothDeviceDetailPage() {
  if (!m_bluetoothDeviceDetailPage)
    return;

  m_stack.remove(*m_bluetoothDeviceDetailPage);
  m_bluetoothDeviceDetailPage = nullptr;
}

void MainWindow::removeHardwareConfigPage() {
  if (!m_hardwareConfigPage)
    return;

  m_stack.remove(*m_hardwareConfigPage);
  m_hardwareConfigPage = nullptr;
}

void MainWindow::createEditTeamPage(const TeamRecord &team) {
  removeEditTeamPage();

  m_editTeam = Gtk::manage(new EditTeam(
      ICON_PATH, (runtimeSettingsPath() + "/lights.db"), team));

  m_editTeam->signal_cancel().connect([this]() {
    removeEditTeamPage();
    showTeamListPage();
  });

  m_editTeam->signal_saved().connect([this]() {
    m_teams = readTeams(sportsDbPath());
    persistTeams(m_teams);
    m_themes = readThemeColors(runtimeSettingsPath());
    m_themesLoaded = true;
    persistThemes(m_themes);
    if (m_dailySportsPoller)
      m_dailySportsPoller->runOnceAsync(m_teams);
    removeEditTeamPage();
    if (m_teamList)
      m_teamList->reload();
    showTeamListPage();
  });

  m_editTeam->signal_deleted().connect([this, team]() {
    deleteSportsTeamFromTeensy(team.id, team.name);
    m_teams = readTeams(sportsDbPath());
    persistTeams(m_teams);
    if (m_dailySportsPoller)
      m_dailySportsPoller->runOnceAsync(m_teams);
    removeEditTeamPage();
    if (m_teamList)
      m_teamList->reload();
    showTeamListPage();
  });

  m_stack.add(*m_editTeam, "edit_team");
  m_editTeam->signal_validation_failed().connect(
      [this](const std::string &msg) { showShortToast(msg); });
}

void MainWindow::showEditTeamPage(const TeamRecord &team) {
  createEditTeamPage(team);
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editTeam);
  m_stack.queue_draw();
}

void MainWindow::removeEditTeamPage() {
  if (!m_editTeam)
    return;

  m_stack.remove(*m_editTeam);
  m_editTeam = nullptr;
}

void MainWindow::showClockPage() {
  if (m_clockVisible) {
    LOG_WARN() << "showClockPage ignored because clock is already visible";
    return;
  }

  LOG_INFO() << "Showing clock page";

  m_clockVisible = true;

  if (m_clockPage)
    m_clockPage->start();
  else
    LOG_ERROR() << "m_clockPage is null in showClockPage()";

  m_stack.set_visible_child(*m_clockPage);
  m_stack.show_all_children();
  m_stack.queue_draw();
}

void MainWindow::dismissClockPage() {
  LOG_INFO() << "Clock page dismissed by user";

  m_clockVisible = false;

  if (m_clockPage) {
    m_clockPage->stop();
    m_clockPage->resetPosition();
  } else {
    LOG_ERROR() << "m_clockPage is null in dismissClockPage()";
  }

  showHomePage();
}

bool MainWindow::onBluetoothPollTick() {
  if (!m_bluetoothState)
    return false;

  const std::string oldStatus = m_btControl.lastStatus();

  m_btControl.scanAvailableDevices();
  m_btControl.scanPairedDevices();
  m_btControl.trustAllPairedDevices();

  const std::string newStatus = m_btControl.lastStatus();
  if (!newStatus.empty() && newStatus != oldStatus) {
    const std::string toast = bluetoothToastMessage(newStatus);
    if (!toast.empty())
      showShortToast(toast);
  }

  auto connectedDevice = m_btControl.getConnectedDevice();
  if (connectedDevice) {
    const std::string sink = chooseBluetoothSink();
    LOG_INFO() << "Bluetooth playback sink: "
               << (sink.empty() ? "<not found>" : sink);
    if (!sink.empty())
      setBluetoothPlaybackSink(sink);

    handleBluetoothConnected();
    return true;
  }

  if (m_btControl.autoReconnectBestDevice()) {
    const std::string reconnectStatus = m_btControl.lastStatus();
    if (!reconnectStatus.empty() && reconnectStatus != newStatus) {
      const std::string toast = bluetoothToastMessage(reconnectStatus);
      if (!toast.empty())
        showShortToast(toast);
    }

    auto reconnected = m_btControl.getConnectedDevice();
    if (reconnected) {
      const std::string sink = chooseBluetoothSink();
      LOG_INFO() << "Bluetooth playback sink: "
                 << (sink.empty() ? "<not found>" : sink);
      if (!sink.empty())
        setBluetoothPlaybackSink(sink);

      handleBluetoothConnected();
    }
  } else {
    handleBluetoothDisconnected();
  }

  return true;
}

void MainWindow::stopBluetoothPolling() {
  if (m_bluetoothPollConn.connected())
    m_bluetoothPollConn.disconnect();
}

void MainWindow::destroyTemporaryPage(const std::string &pageName) {
  auto child = m_stack.get_child_by_name(pageName);
  if (!child)
    return;

  LOG_INFO() << "Destroying temporary page: " << pageName;

  m_stack.remove(*child);

  if (pageName == "settings")
    m_settingsPage = nullptr;
  else if (pageName == "delta_all")
    m_deltaAllPage = nullptr;
  else if (pageName == "delta_group")
    m_deltaGroupPage = nullptr;
  else if (pageName == "themes")
    m_themesPage = nullptr;
  else if (pageName == "pattern")
    m_patternPage = nullptr;
  else if (pageName == "game_day")
    m_gameDayPage = nullptr;
  else if (pageName == "debug")
    m_debugPage = nullptr;
  else if (pageName == "debug_sports")
    m_debugSportsPage = nullptr;
  else if (pageName == "debug_action") {
    m_gameDayPage = nullptr;
    m_debugPlaceholderPage = nullptr;
  }
  else if (pageName == "edit_themes")
    m_editThemesPage = nullptr;
  else if (pageName == "edit_theme")
    m_editThemePage = nullptr;
  else if (pageName == "edit_pattern")
    m_editPatternPage = nullptr;
  else if (pageName == "lightshow_settings")
    m_lightShowSettingsPage = nullptr;
  else if (pageName == "team_list")
    m_teamList = nullptr;
  else if (pageName == "edit_team")
    m_editTeam = nullptr;
  else if (pageName == "bluetooth_controls")
    m_bluetoothControlsPage = nullptr;
  else if (pageName == "bluetooth_device_detail")
    m_bluetoothDeviceDetailPage = nullptr;
  else if (pageName == "hardware_config")
    m_hardwareConfigPage = nullptr;
}

void MainWindow::destroyAllTemporaryPages() {
  destroyTemporaryPage("settings");
  destroyTemporaryPage("delta_all");
  destroyTemporaryPage("delta_group");
  destroyTemporaryPage("themes");
  destroyTemporaryPage("pattern");
  destroyTemporaryPage("game_day");
  destroyTemporaryPage("debug");
  destroyTemporaryPage("debug_sports");
  destroyTemporaryPage("debug_action");
  destroyTemporaryPage("edit_themes");
  destroyTemporaryPage("edit_theme");
  destroyTemporaryPage("edit_pattern");
  destroyTemporaryPage("lightshow_settings");
  destroyTemporaryPage("bluetooth_controls");
  destroyTemporaryPage("bluetooth_device_detail");
  destroyTemporaryPage("hardware_config");
}

void MainWindow::resetIdleClockTimer() {
  if (m_idleClockConn.connected())
    m_idleClockConn.disconnect();

  if (m_clockVisible)
    return;

  m_idleClockConn = Glib::signal_timeout().connect_seconds(
      sigc::mem_fun(*this, &MainWindow::onIdleClockTimeout), 60);
}

bool MainWindow::onIdleClockTimeout() {
  LOG_INFO() << "Idle clock timeout fired";
  showClockPage();
  return false;
}

void MainWindow::onAnyEventAfter(GdkEvent *event) {
  if (!event)
    return;

  switch (event->type) {
  case GDK_BUTTON_PRESS:
  case GDK_BUTTON_RELEASE:
  case GDK_TOUCH_BEGIN:
  case GDK_TOUCH_UPDATE:
  case GDK_TOUCH_END:
  case GDK_KEY_PRESS:
  case GDK_KEY_RELEASE:
  case GDK_SCROLL:
    resetIdleClockTimer();
    break;

  default:
    break;
  }
}

void MainWindow::startRestartCountdown() {
  LOG_WARN() << "Restart countdown started";

  if (m_restartCountdownConn.connected())
    m_restartCountdownConn.disconnect();

  m_restartSecondsLeft = 5;
  updateRestartToastText();

  m_toast.showMessage(
      m_toast.text(),
      {{
          "",
          std::string(ICON_PATH) + "/cancel.png",
          72,
          0,
          0,
          "destructive-action",
          sigc::mem_fun(*this, &MainWindow::cancelRestartCountdown),
      }});

  m_overlay.queue_draw();

  m_restartCountdownConn = Glib::signal_timeout().connect_seconds(
      sigc::mem_fun(*this, &MainWindow::onRestartCountdownTick), 1);
}

void MainWindow::updateRestartToastText() {
  m_toast.setText("Restarting in... " + std::to_string(m_restartSecondsLeft));
}

bool MainWindow::onRestartCountdownTick() {
  --m_restartSecondsLeft;

  LOG_INFO() << "Restart countdown tick: " << m_restartSecondsLeft;

  if (m_restartSecondsLeft <= 0) {
    m_toast.hideMessage();

    if (m_restartCountdownConn.connected())
      m_restartCountdownConn.disconnect();

    doRestart();
    return false;
  }

  updateRestartToastText();
  return true;
}

void MainWindow::cancelRestartCountdown() {
  LOG_WARN() << "Restart countdown canceled";

  if (m_restartCountdownConn.connected())
    m_restartCountdownConn.disconnect();

  m_toast.hideMessage();

  if (m_settingsPage)
    m_settingsPage->set_restart_enabled(true);
}

void MainWindow::doRestart() {
  LOG_ERROR() << "Restarting system now";
  system("reboot");
}

bool MainWindow::isSportsSchedule(const Schedule &s) {
  return s.name.rfind("TEAM_", 0) == 0;
}

void MainWindow::restoreManualLedsAsync() {
  if (m_shuttingDown)
    return;

  if (m_ledInfo.size() != NUM_OF_LEDS) {
    LOG_ERROR() << "Manual LED restore skipped: expected " << NUM_OF_LEDS
                << " LEDs, got " << m_ledInfo.size();
    return;
  }

  joinManualRestoreThread();

  const std::vector<LEDData> leds = m_ledInfo;
  m_manualRestoreThread = std::thread([this, leds]() {
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      if (m_shuttingDown)
        return;

      uint32_t mask = (1u << i);

      m_teensyClient.applyMaskedRGB(mask,
                                    static_cast<uint8_t>(leds[i].redVal),
                                    static_cast<uint8_t>(leds[i].grnVal),
                                    static_cast<uint8_t>(leds[i].bluVal));

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
}

void MainWindow::joinManualRestoreThread() {
  if (m_manualRestoreThread.joinable())
    m_manualRestoreThread.join();
}

void MainWindow::applyCurrentScheduleState(bool) {
  if (!m_liveSportsTeams.empty()) {
    const auto &team = m_liveSportsTeams.front();
    applySportsTeamColors(team, team.homeAway != "away");
    updateLightsDependentUi();
    return;
  }

  auto active = ClockThread::instance().activeSchedulesSnapshot();

  for (const auto &base : m_schedule) {
    for (const auto &a : active) {
      if (a.name != base.name)
        continue;

      if (!isSportsSchedule(a))
        continue;

      for (const auto &team : m_teams) {
        if (a.name == scheduleNameForTeam(team)) {
          const bool isHome = team.homeAway != "away";
          applySportsTeamColors(team, isHome);
          updateLightsDependentUi();
          return;
        }
      }
    }
  }

  if (applyNormalScheduleState()) {
    updateLightsDependentUi();
    return;
  }

  if (m_options.sensor) {
    const bool sensorWantsLightsOn = m_lightSensorThread.readOnce();
    m_options.on = sensorWantsLightsOn ? 1 : 0;
    persistOptions(m_options);
    if (setLightsPowerEnabled(sensorWantsLightsOn) && sensorWantsLightsOn)
      restoreManualLedsAsync();
    updateLightShowState();
    updateLightsDependentUi();
    return;
  }

  if (m_options.on) {
    if (setLightsPowerEnabled(true)) {
      if (m_options.theme > 0)
        m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
      else
        restoreManualLedsAsync();
    }
    updateLightShowState();
    updateLightsDependentUi();
    return;
  }

  setLightsPowerEnabled(false);
  updateLightShowState();
  updateLightsDependentUi();
}

bool MainWindow::applyNormalScheduleState() {
  auto active = ClockThread::instance().activeSchedulesSnapshot();
  const Schedule *winner = nullptr;

  for (const auto &base : m_schedule) {
    for (const auto &a : active) {
      if (a.name != base.name)
        continue;
      if (!isSportsSchedule(a) && a.themeID > 0)
        winner = &a;
    }
  }

  if (!winner)
    return false;

  const int patternId = (m_options.ptrn == 0) ? 1 : m_options.ptrn;
  if (setLightsPowerEnabled(true))
    m_teensyClient.applyThemePattern(winner->themeID, patternId);
  updateLightShowState();
  return true;
}

bool MainWindow::anyNormalScheduleActive() {
  auto active = ClockThread::instance().activeSchedulesSnapshot();
  for (const auto &a : active) {
    if (!isSportsSchedule(a) && a.themeID > 0)
      return true;
  }
  return false;
}

bool MainWindow::applySportsTeamColors(const TeamRecord &team, bool isHome,
                                       uint8_t patternId) {
  std::vector<TeamColor> selected;
  const std::string prefix = isHome ? "home_" : "away_";
  for (const auto &color : team.colors) {
    if (color.colorRole.rfind(prefix, 0) == 0)
      selected.push_back(color);
  }
  std::sort(selected.begin(), selected.end(),
            [](const TeamColor &a, const TeamColor &b) {
              return a.displayOrder < b.displayOrder;
            });
  if (selected.empty()) {
    LOG_ERROR() << "Sports color apply failed: no " << prefix
                << " colors for " << team.name;
    return false;
  }

  m_options.on = 1;
  m_options.theme = 0;
  m_options.ptrn = 0;
  persistOptions(m_options);
  if (!setLightsPowerEnabled(true))
    return false;

  if (!syncSportsTeamToTeensy(team))
    return false;

  if (!m_teensyClient.activateSportsTeam(static_cast<uint16_t>(team.id), isHome,
                                         patternId)) {
    LOG_ERROR() << "Sports activation failed teamId=" << team.id
                << " name=" << team.name;
    return false;
  }
  updateLightShowState();
  return true;
}

bool MainWindow::syncSportsTeamToTeensy(const TeamRecord &team) {
  if (team.id <= 0) {
    LOG_ERROR() << "Sports team sync failed: invalid team id for " << team.name;
    return false;
  }

  if (!m_teensyClient.syncTeamColors(static_cast<uint16_t>(team.id), team.name,
                                     team.colors)) {
    LOG_ERROR() << "Sports team sync write failed teamId=" << team.id
                << " name=" << team.name;
    return false;
  }

  uint8_t status = TeensyClient::FILE_ERROR;
  for (int i = 0; i < 20; ++i) {
    if (!m_teensyClient.readFileStatus(status)) {
      LOG_ERROR() << "Sports team sync status read failed teamId=" << team.id
                  << " name=" << team.name;
      return false;
    }

    if (status == TeensyClient::FILE_SUCCESS)
      return true;

    if (status == TeensyClient::FILE_ERROR) {
      LOG_ERROR() << "Sports team sync failed on Teensy teamId=" << team.id
                  << " name=" << team.name;
      return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  LOG_ERROR() << "Sports team sync timed out teamId=" << team.id
              << " name=" << team.name;
  return false;
}

bool MainWindow::deleteSportsTeamFromTeensy(int teamId,
                                            const std::string &teamName) {
  if (teamId <= 0)
    return false;

  const bool wasPowered = m_lightsPowerEnabled;
  if (!wasPowered && !setLightsPowerEnabled(true)) {
    LOG_ERROR() << "Failed to power Teensy for team delete teamId=" << teamId
                << " name=" << teamName;
    return false;
  }

  const bool sent =
      m_teensyClient.deleteTeamColors(static_cast<uint16_t>(teamId));

  uint8_t status = TeensyClient::FILE_ERROR;
  bool ok = false;
  if (sent) {
    for (int i = 0; i < 20; ++i) {
      if (!m_teensyClient.readFileStatus(status))
        break;
      if (status == TeensyClient::FILE_SUCCESS) {
        ok = true;
        break;
      }
      if (status == TeensyClient::FILE_ERROR)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
  }

  if (!wasPowered)
    setLightsPowerEnabled(false);

  if (!ok) {
    LOG_ERROR() << "Failed to delete team colors on Teensy teamId=" << teamId
                << " name=" << teamName;
  }
  return ok;
}

void MainWindow::applyPostSportsState() {
  if (m_options.sensor) {
    const bool sensorWantsLightsOn = m_lightSensorThread.readOnce();
    m_options.on = sensorWantsLightsOn ? 1 : 0;
    persistOptions(m_options);
    if (setLightsPowerEnabled(sensorWantsLightsOn) && sensorWantsLightsOn)
      restoreManualLedsAsync();
    updateLightShowState();
    updateLightsDependentUi();
    return;
  }

  m_options.on = 0;
  persistOptions(m_options);
  setLightsPowerEnabled(false);
  updateLightShowState();
  updateLightsDependentUi();
}

void MainWindow::onScheduleStarted(const Schedule &schedule) {
  std::cout << "StartTheme: " << schedule.name << std::endl;
  applyCurrentScheduleState();
}

void MainWindow::onScheduleEnded(const Schedule &schedule) {
  std::cout << "EndTheme: " << schedule.name << std::endl;
  applyCurrentScheduleState();
}

std::vector<Schedule> MainWindow::getActiveSportsSchedules() {
  std::vector<Schedule> result;

  auto active = ClockThread::instance().activeSchedulesSnapshot();

  for (const auto &s : active) {
    if (s.name.rfind("TEAM_", 0) == 0) {
      result.push_back(s);
    }
  }

  return result;
}

int findScheduleIndexByName(const std::vector<Schedule> &list,
                            const std::string &name) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void upsertSchedule(std::vector<Schedule> &list, const Schedule &newSchedule) {
  int idx = findScheduleIndexByName(list, newSchedule.name);

  if (idx >= 0) {
    list[idx] = newSchedule; // ✅ update
  } else {
    list.push_back(newSchedule); // ✅ insert
  }
}

bool MainWindow::isGameDay(const std::string &date) {
  std::time_t t = std::time(nullptr);
  std::tm *now = std::localtime(&t);

  int month = now->tm_mon + 1;
  int day = now->tm_mday;

  char today[16];
  std::snprintf(today, sizeof(today), "%02d/%02d", month, day);

  const bool match = (date == today);

  LOG_INFO() << "isGameDay(" << date << ") -> " << (match ? "true" : "false");

  return match;
}

std::string MainWindow::addHours(const std::string &time24, int hours) {
  std::tm tm{};
  std::istringstream ss(time24);
  ss >> std::get_time(&tm, "%H:%M");

  std::time_t t = std::mktime(&tm);
  t += hours * 3600;

  std::tm *result = std::localtime(&t);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%H:%M", result);

  return buffer;
}

int MainWindow::themeIdForTeam(const TeamRecord &team) const {
  if (team.themeID > 0)
    return team.themeID;

  if (!team.themeName.empty()) {
    for (const auto &theme : m_themes) {
      if (theme.name == team.themeName)
        return theme.id;
    }
  }

  return m_options.theme;
}

std::string
MainWindow::chooseTeamAnimationPath(int teamId,
                                    const std::string &animationType) const {
  auto animations =
      readTeamAnimationsByType(sportsDbPath(), teamId, animationType);

  if (animations.empty())
    return "";

  static std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<size_t> dist(0, animations.size() - 1);
  return animations[dist(rng)].filePath;
}

void MainWindow::onSportsGamesChecked(
    std::vector<SportsNextGameEvent> events) {
  LOG_INFO() << "Sports daily check returned " << events.size() << " games";

  m_teams = readTeams(sportsDbPath());
  std::set<std::string> touchedNames;
  std::vector<TeamRecord> liveTeams;

  for (const auto &event : events) {
    TeamRecord team = event.team;
    const ParsedNextGame &game = event.game;

    std::tm gameLocal{};
    if (!parseUtcToLocal(game.dateUtc, gameLocal))
      continue;

    updateTeamNextGame(sportsDbPath(), team.id, game.dateUtc, game.id,
                       nowUtcString());
    for (auto &knownTeam : m_teams) {
      if (knownTeam.id == team.id) {
        knownTeam.nextGameUtc = game.dateUtc;
        knownTeam.lastGameId = game.id;
        knownTeam.lastCheckedUtc = nowUtcString();
        knownTeam.homeAway = game.isHome ? "home" : "away";
        knownTeam.nextOpponentCode = game.opponent;
        knownTeam.nextOpponentName = game.opponent;
        team = knownTeam;
        break;
      }
    }

    const std::string scheduleName = scheduleNameForTeam(team);
    touchedNames.insert(scheduleName);

    if (!isTodayLocal(gameLocal)) {
      m_schedule.erase(std::remove_if(m_schedule.begin(), m_schedule.end(),
                                      [&](const Schedule &s) {
                                        return s.name == scheduleName;
                                      }),
                       m_schedule.end());
      continue;
    }

    team.homeAway = game.isHome ? "home" : "away";

    Schedule s;
    s.name = scheduleName;
    s.themeID = 0;
    s.enabled = team.enabled ? 1 : 0;
    s.sDate = mmddFromTm(gameLocal);
    s.sTime = "00:00";
    s.eDate = s.sDate;
    s.eTime = "23:59";

    upsertSchedule(m_schedule, s);
    team.nextGameUtc = game.dateUtc;
    team.lastGameId = game.id;
    liveTeams.push_back(team);
  }

  persistSchedule(m_schedule);
  persistTeams(m_teams, false);
  ClockThread::instance().setSchedules(m_schedule);

  m_liveSportsTeams = liveTeams;
  refreshLiveSportsPoller();
  applyCurrentScheduleState();
}

void MainWindow::refreshLiveSportsPoller() {
  if (!m_liveGamePoller)
    return;

  m_liveGamePoller->setTeams(m_liveSportsTeams);

  if (m_liveSportsTeams.empty()) {
    if (m_liveGamePoller->running())
      m_liveGamePoller->stop();
    return;
  }

  if (!m_liveGamePoller->running())
    m_liveGamePoller->start();
}

void MainWindow::onSportsLiveUpdate(SportsLiveEvent event) {
  const std::string state = event.game.normalizedStatus.empty()
                                ? event.game.state
                                : event.game.normalizedStatus;
  updateSportsLiveState(sportsDbPath(), event.team.id, event.game.id,
                        state, event.game.homeScore,
                        event.game.awayScore, nowUtcString(),
                        !isSportsTerminalState(state));

  if (isSportsLiveState(state)) {
    TeamRecord liveTeam = event.team;
    liveTeam.homeAway = event.game.isHome ? "home" : "away";
    auto it = std::find_if(m_liveSportsTeams.begin(), m_liveSportsTeams.end(),
                           [&](const TeamRecord &t) {
                             return t.id == liveTeam.id;
                           });
    if (it == m_liveSportsTeams.end())
      m_liveSportsTeams.push_back(liveTeam);
    else
      *it = liveTeam;
    applySportsTeamColors(liveTeam, event.game.isHome);
  }
}

void MainWindow::onSportsHomeScore(SportsLiveEvent event) {
  const int delay =
      std::clamp(event.team.scoreAnimationDelaySeconds, 0, 120);
  if (delay > 0) {
    LOG_INFO() << "Delaying sports score animation for " << event.team.name
               << " by " << delay << " seconds";
    Glib::signal_timeout().connect_seconds(
        [this, event]() -> bool {
          if (m_shuttingDown)
            return false;
          runSportsScoreAnimation(event);
          return false;
        },
        delay);
    return;
  }

  runSportsScoreAnimation(event);
}

void MainWindow::runSportsScoreAnimation(SportsLiveEvent event) {
  const int diff = event.game.homeScore - event.game.awayScore;
  std::string type = "home_score";

  if (diff >= 4 &&
      !readTeamAnimationsByType(sportsDbPath(), event.team.id, "blowout")
           .empty()) {
    type = "blowout";
  } else if (diff >= 3 &&
             !readTeamAnimationsByType(sportsDbPath(), event.team.id,
                                       "lopsided")
                  .empty()) {
    type = "lopsided";
  }

  GameInfo info;
  info.id = 0;
  info.gameState = event.game.state;
  info.home = event.game.homeTeam;
  info.away = event.game.awayTeam;
  info.dateTimeUTC = event.game.dateUtc;
  info.scoreHome = std::to_string(event.game.homeScore);
  info.scoreAway = std::to_string(event.game.awayScore);
  info.isHomeGame = event.game.isHome;
  info.opponentCode = event.game.opponent;
  info.opponentName = event.game.opponent;

  updateSportsAnimatedScore(sportsDbPath(), event.team.id,
                            event.game.homeScore, event.game.awayScore);
  triggerSportsAnimation(event.team, type, info);
}

void MainWindow::onSportsGameFinished(SportsLiveEvent event) {
  const std::string state = event.game.normalizedStatus.empty()
                                ? event.game.state
                                : event.game.normalizedStatus;
  m_liveSportsTeams.erase(
      std::remove_if(m_liveSportsTeams.begin(), m_liveSportsTeams.end(),
                     [&](const TeamRecord &t) { return t.id == event.team.id; }),
      m_liveSportsTeams.end());
  refreshLiveSportsPoller();

  updateSportsLiveState(sportsDbPath(), event.team.id, event.game.id,
                        state, event.game.homeScore,
                        event.game.awayScore, nowUtcString(), false);
  applyPostSportsState();
}

void MainWindow::triggerHourlyGameDayAnimation() {
  if (m_sportsAnimationRunning)
    return;

  const auto activeSports = getActiveSportsSchedules();
  if (activeSports.empty())
    return;

  m_teams = readTeams(sportsDbPath());

  for (const auto &schedule : activeSports) {
    for (const auto &team : m_teams) {
      if (schedule.name == scheduleNameForTeam(team)) {
        GameInfo info;
        const bool seattleHome = team.homeAway != "away";
        info.home = seattleHome ? team.name : team.nextOpponentName;
        info.away = seattleHome ? team.nextOpponentName : team.name;
        info.gameState = "game_day";
        info.dateTimeUTC = team.nextGameUtc;
        info.isHomeGame = seattleHome;
        info.opponentCode = team.nextOpponentCode;
        info.opponentName = team.nextOpponentName;
        triggerSportsAnimation(team, "game_day_hourly", info);
        return;
      }
    }
  }
}

void MainWindow::triggerSportsAnimation(const TeamRecord &team,
                                        const std::string &animationType,
                                        const GameInfo &gameInfo) {
  if (m_sportsAnimationRunning && animationType != "home_score" &&
      animationType != "blowout" && animationType != "lopsided") {
    return;
  }

  if (m_sportsAnimationRunning) {
    if (m_gameDayPage)
      m_gameDayPage->stop();
    destroyTemporaryPage("game_day");
  }

  m_sportsAnimationRunning = true;

  TeamRecord currentTeam = team;
  for (const auto &knownTeam : m_teams) {
    if (knownTeam.id == team.id) {
      currentTeam = knownTeam;
      break;
    }
  }

  const bool seattleHome =
      !gameInfo.home.empty() && !gameInfo.away.empty()
          ? gameInfo.isHomeGame
          : (currentTeam.homeAway != "away");
  const std::string opponentCode =
      !gameInfo.opponentCode.empty()
          ? gameInfo.opponentCode
          : (!currentTeam.nextOpponentCode.empty() ? currentTeam.nextOpponentCode
                                                   : gameInfo.opponentName);
  const std::string opponentName =
      !gameInfo.opponentName.empty()
          ? gameInfo.opponentName
          : (!currentTeam.nextOpponentName.empty() ? currentTeam.nextOpponentName
                                                   : opponentCode);

  GameDayAnimationData data;
  data.league = uppercaseCopy(currentTeam.league);
  data.gameTimeDisplay = formatGameTimeDisplay(currentTeam, gameInfo);
  data.seattleTeamCode = currentTeam.teamCode;
  data.seattleTeamColors = allTeamColors(currentTeam);
  if (data.seattleTeamColors.empty())
    data.seattleTeamColors = colorsForSide(currentTeam, seattleHome);

  const auto seattleName =
      splitCityTeam(currentTeam.name, currentTeam.teamCode.empty()
                                         ? "Seattle"
                                         : currentTeam.teamCode);
  GameDayTeamVisual seattleVisual;
  seattleVisual.city = seattleName.first;
  seattleVisual.name = seattleName.second;
  seattleVisual.logoPath = getTeamLogoPath(currentTeam);
  seattleVisual.flagPath = gameDayTeamAssetPath(currentTeam, "_f.png");
  seattleVisual.colors = colorsForSide(currentTeam, seattleHome);
  if (seattleVisual.colors.empty())
    seattleVisual.colors = neutralOpponentColors();

  TeamRecord opponentTeam;
  bool foundOpponent = false;
  const std::string normalizedOpponentCode = uppercaseCopy(opponentCode);
  for (const auto &knownTeam : m_teams) {
    if (uppercaseCopy(knownTeam.league) == uppercaseCopy(currentTeam.league) &&
        uppercaseCopy(knownTeam.teamCode) == normalizedOpponentCode) {
      opponentTeam = knownTeam;
      foundOpponent = true;
      break;
    }
  }

  const auto opponentSplit =
      splitCityTeam(foundOpponent ? opponentTeam.name : opponentName,
                    normalizedOpponentCode.empty() ? "OPP"
                                                   : normalizedOpponentCode);
  GameDayTeamVisual opponentVisual;
  opponentVisual.city = opponentSplit.first;
  opponentVisual.name = opponentSplit.second;
  if (foundOpponent) {
    opponentVisual.logoPath = getTeamLogoPath(opponentTeam);
  } else {
    TeamRecord opponentAssetTeam;
    opponentAssetTeam.league = currentTeam.league;
    opponentAssetTeam.teamCode = normalizedOpponentCode;
    opponentAssetTeam.apiTeamId = normalizedOpponentCode;
    opponentVisual.logoPath = getTeamLogoPath(opponentAssetTeam);
  }
  opponentVisual.flagPath =
      foundOpponent ? gameDayTeamAssetPath(opponentTeam, "_f.png")
                    : gameDayTeamAssetPath(currentTeam.league,
                                           normalizedOpponentCode, "_f.png");
  opponentVisual.colors =
      foundOpponent ? colorsForSide(opponentTeam, !seattleHome)
                    : fallbackOpponentColors(currentTeam.league,
                                             normalizedOpponentCode);
  if (opponentVisual.colors.empty())
    opponentVisual.colors = neutralOpponentColors();

  if (seattleHome) {
    data.left = opponentVisual;
    data.right = seattleVisual;
  } else {
    data.left = seattleVisual;
    data.right = opponentVisual;
  }

  LOG_INFO() << "Game-day animation league=" << data.league
             << " seattle=" << currentTeam.teamCode
             << " opponent=" << normalizedOpponentCode
             << " seattleHome=" << (seattleHome ? "true" : "false")
             << " background=" << gameDayAnimationRoot() << "/gd_"
             << lowercaseCopy(data.league) << ".png"
             << " leftLogo=" << data.left.logoPath
             << " leftFlag=" << data.left.flagPath
             << " rightLogo=" << data.right.logoPath
             << " rightFlag=" << data.right.flagPath;

  animationBasePath();
  applySportsTeamColors(currentTeam, seattleHome, kAlternatePatternId);

  if (data.league == "NHL") {
    m_gameDayPage = Gtk::manage(new HockeyGameDayAnimation(data));
  } else if (data.league == "NFL") {
    m_gameDayPage = Gtk::manage(new FootballGameDayAnimation(data));
  } else if (data.league == "MLB") {
    m_gameDayPage = Gtk::manage(new BaseballGameDayAnimation(data));
  } else {
    LOG_WARN() << "Unknown sports animation league " << data.league
               << "; using baseball-style field";
    m_gameDayPage = Gtk::manage(new BaseballGameDayAnimation(data));
  }
  m_gameDayPage->start();

  m_returnToClockAfterGameDay = m_clockVisible;
  m_stack.add(*m_gameDayPage, "game_day");
  showPage("game_day");
  m_stack.set_visible_child(*m_gameDayPage);
  m_stack.show_all_children();
  show_all_children();

  if (m_sportsAnimationTimeoutConn.connected())
    m_sportsAnimationTimeoutConn.disconnect();
  if (m_debugActionTimeoutConn.connected())
    m_debugActionTimeoutConn.disconnect();

  m_sportsAnimationTimeoutConn = Glib::signal_timeout().connect_seconds(
      [this]() -> bool {
        if (m_gameDayPage)
          m_gameDayPage->stop();

        destroyTemporaryPage("game_day");
        m_sportsAnimationRunning = false;
        applyCurrentScheduleState();
        if (m_returnToClockAfterGameDay) {
          m_returnToClockAfterGameDay = false;
          m_clockVisible = false;
          showClockPage();
        } else {
          showHomePage();
        }
        return false;
      },
      30);
}

void MainWindow::onDoorbellChanged(bool pressed) {
  LOG_INFO() << "Doorbell changed: " << (pressed ? "PRESSED" : "RELEASED");

  if (pressed) {
    const std::string sink = chooseDoorbellSink();
    LOG_INFO() << "Doorbell playback sink: "
               << (sink.empty() ? "<not found>" : sink);
    if (sink.empty() || !playDoorbellSoundOnSink(sink)) {
      LOG_WARN() << "Failed to play doorbell sound on HDMI sink";
    }
  }
}

void MainWindow::updateOptions(const Options &options) {
  m_options = options;
  persistOptions(m_options);
  LOG_INFO() << "Options updated";
  if (lightsAreOn())
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
}

void MainWindow::updateScheduleEntry(int index, const Schedule &entry) {
  if (index < 0 || static_cast<size_t>(index) >= m_schedule.size()) {
    LOG_ERROR() << "updateScheduleEntry index out of range: " << index;
    return;
  }

  m_schedule[index] = entry;
  persistSchedule(m_schedule);
  ClockThread::instance().setSchedules(m_schedule);
  LOG_INFO() << "Schedule entry updated at index " << index;
}

void MainWindow::getAvgColor(int allOrGroup, int &r, int &g, int &b) {
  int x = 0;
  int y = 0;
  int z = 0;

  if (allOrGroup < 0) {
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      x += m_ledInfo[i].redVal;
      y += m_ledInfo[i].grnVal;
      z += m_ledInfo[i].bluVal;
    }
    r = x / NUM_OF_LEDS;
    g = y / NUM_OF_LEDS;
    b = z / NUM_OF_LEDS;
  } else {
    int count = 0;
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      if (m_ledInfo[i].group == allOrGroup) {
        x += m_ledInfo[i].redVal;
        y += m_ledInfo[i].grnVal;
        z += m_ledInfo[i].bluVal;
        ++count;
      }
    }

    if (count <= 0) {
      r = 0;
      g = 0;
      b = 0;
      return;
    }

    r = x / count;
    g = y / count;
    b = z / count;
  }
}
void MainWindow::showEditThemesPage() {
  LOG_INFO() << "showEditThemesPage requested";

  destroyTemporaryPage("edit_themes");
  ensureThemesLoaded();

  m_editThemesPage =
      Gtk::manage(new EditThemes(runtimeSettingsPath(), m_themes));

  m_editThemesPage->signal_theme_edit_requested().connect(
      [this](int themeId) { showEditThemePage(themeId); });

  m_editThemesPage->signal_done().connect([this]() { showSettingsPage(); });

  m_stack.add(*m_editThemesPage, "edit_themes");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editThemesPage);
  m_stack.queue_draw();
}

void MainWindow::showEditThemePage(int themeId) {
  LOG_INFO() << "showEditThemePage requested themeId=" << themeId;

  destroyTemporaryPage("edit_theme");
  ensureThemesLoaded();

  auto it = std::find_if(m_themes.begin(), m_themes.end(),
                         [themeId](const Theme &t) { return t.id == themeId; });

  if (it == m_themes.end()) {
    LOG_ERROR() << "Theme id not found: " << themeId;
    return;
  }

  m_editThemePage = Gtk::manage(new EditThemePage(
      std::string(ICON_PATH), *it, UiMetrics::color_picker_size(),
      UiMetrics::color_bar_size(), 96));

  m_editThemePage->signal_save_requested().connect([this, themeId](
                                                       Theme updatedTheme) {
    auto it2 = std::find_if(
        m_themes.begin(), m_themes.end(),
        [&updatedTheme](const Theme &t) { return t.id == updatedTheme.id; });

    if (it2 != m_themes.end()) {
      *it2 = updatedTheme;
      persistThemes(m_themes);
      queueThemeUploadToTeensy(updatedTheme);
      LOG_INFO() << "Theme saved: " << updatedTheme.name;
    }

    showEditThemesPage();
  });

  m_editThemePage->signal_cancel_requested().connect(
      [this]() { showEditThemesPage(); });

  m_stack.add(*m_editThemePage, "edit_theme");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editThemePage);
  m_stack.queue_draw();
}

void MainWindow::showEditPatternPage() {
  LOG_INFO() << "showEditPatternPage requested";

  destroyTemporaryPage("edit_pattern");
  ensurePatternsLoaded();

  m_editPatternPage =
      Gtk::manage(new EditPattern(std::string(ICON_PATH), m_pattern));

  m_editPatternPage->signal_pattern_speed_preview().connect(
      [this](int patternId, int speed) {
        LOG_INFO() << "Pattern speed preview id=" << patternId
                   << " speed=" << speed;

        // Live preview only. No DB save here.
        if (lightsAreOn())
          m_teensyClient.applyPatternSpeed(static_cast<uint8_t>(patternId),
                                           static_cast<uint8_t>(speed));
      });

  m_editPatternPage->signal_save().connect(
      [this](const std::vector<Pattern> &patterns) {
        LOG_INFO() << "Saving pattern speeds";

        m_pattern = patterns;
        persistPatterns(m_pattern);

        // After save, turn active pattern off.
        m_options.ptrn = 0;
        persistOptions(m_options);
        if (lightsAreOn())
          m_teensyClient.applyThemePattern(m_options.theme, 0);

        // Send saved speeds to Teensy.
        if (lightsAreOn())
          sendPatternSpeedsToTeensyAsync(m_pattern);

        showSettingsPage();
      });

  m_editPatternPage->signal_cancel().connect([this]() {
    LOG_INFO() << "EditPattern canceled";

    // Discard unsaved speed edits and restore current saved pattern/theme.
    if (lightsAreOn())
      m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);

    showSettingsPage();
  });

  m_stack.add(*m_editPatternPage, "edit_pattern");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editPatternPage);
  m_stack.queue_draw();
}

void MainWindow::showLightShowSettingsPage() {
  LOG_INFO() << "showLightShowSettingsPage requested";

  if (!requireLightsOnForHomeAction())
    return;

  if (!m_lightShowRunning.load() || !m_lightShow) {
    showShortToast("LightShow is off");
    return;
  }

  destroyTemporaryPage("lightshow_settings");

  m_lightShowSettingsPage =
      Gtk::manage(new LightShowSettingsPage(ICON_PATH, m_lightShow->cfg_));

  m_lightShowSettingsPage->signal_value_changed().connect(
      [this](LightShowControl control, float value) {
        applyLightShowControl(control, value);
      });

  m_lightShowSettingsPage->signal_done().connect(
      [this]() { showSettingsPage(); });

  m_stack.add(*m_lightShowSettingsPage, "lightshow_settings");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_lightShowSettingsPage);
  m_stack.queue_draw();
}

void MainWindow::applyLightShowControl(LightShowControl control, float value) {
  std::lock_guard<std::mutex> lock(m_lightShowMutex);

  if (!m_lightShowRunning.load() || !m_lightShow)
    return;

  switch (control) {
  case LightShowControl::AgcTarget:
    m_lightShow->setAGCTarget(value);
    break;
  case LightShowControl::MasterGain:
    m_lightShow->setMasterGain(value);
    break;
  case LightShowControl::Gamma:
    m_lightShow->setGamma(value);
    break;
  case LightShowControl::Saturation:
    m_lightShow->setSaturation(value);
    break;
  case LightShowControl::Contrast:
    m_lightShow->setContrast(value);
    break;
  case LightShowControl::LogGain:
    m_lightShow->setLogGain(value);
    break;
  case LightShowControl::BassGain:
    m_lightShow->setBandGains(value, m_lightShow->cfg_.band_gain_mid.load(),
                              m_lightShow->cfg_.band_gain_high.load());
    break;
  case LightShowControl::MidGain:
    m_lightShow->setBandGains(m_lightShow->cfg_.band_gain_bass.load(), value,
                              m_lightShow->cfg_.band_gain_high.load());
    break;
  case LightShowControl::HighGain:
    m_lightShow->setBandGains(m_lightShow->cfg_.band_gain_bass.load(),
                              m_lightShow->cfg_.band_gain_mid.load(), value);
    break;
  case LightShowControl::DriftAmount:
    m_lightShow->setDriftAmount(value);
    break;
  case LightShowControl::DriftSpeed:
    m_lightShow->setDriftSpeedScale(value);
    break;
  }
}

void MainWindow::showShortToast(const std::string &message) {
  if (message.empty())
    return;

  if (m_lastShortToastMessage == message)
    return;

  m_lastShortToastMessage = message;

  if (m_toastHideConn.connected())
    m_toastHideConn.disconnect();

  m_toast.showMessage(message);

  m_toastHideConn = Glib::signal_timeout().connect_seconds(
      [this]() -> bool {
        m_toast.hideMessage();
        m_lastShortToastMessage.clear();
        return false;
      },
      5);
}

void MainWindow::onPowerSwitchChanged(bool enabled) {
  LOG_INFO() << "MainWindow: power switch changed -> "
             << (enabled ? "ON" : "OFF");

  m_options.on = enabled ? 1 : 0;
  if (!m_shuttingDown)
    persistOptions(m_options);
  updateLightShowState();
  updateLightsDependentUi();
}

void MainWindow::onBluetoothPowerChanged(bool enabled) {
  LOG_INFO() << "MainWindow: bluetooth power changed -> "
             << (enabled ? "ON" : "OFF");

  m_bluetoothState = enabled ? 1 : 0;

  if (!enabled) {
    stopBluetoothPolling();
    cancelBluetoothIdleTimeout();
    m_bluetoothConnected = false;
    setAmpEnabledForBluetooth(false, "Bluetooth disabled");
  }

  updateLightShowState();
}

void MainWindow::updateLightShowState() {
  if (m_shuttingDown)
    return;

  const auto connectedDevice = m_btControl.getConnectedDevice();
  const std::string monitor = chooseLightShowMonitor();
  const bool audioReady = m_bluetoothState && connectedDevice &&
                          m_ampEnabled.load() && !monitor.empty();
  const bool shouldRun = (lightsAreOn() && audioReady);

  const bool running = m_lightShowRunning.load();
  LOG_INFO() << "LightShow shouldRun=" << shouldRun
             << " running=" << running
             << " audioReady=" << audioReady
             << " monitor=" << (monitor.empty() ? "<not found>" : monitor);

  if (shouldRun) {
    if (!running)
      startLightShow();
  } else {
    if (running)
      stopLightShow();
  }

}

void MainWindow::startLightShow() {
  std::lock_guard<std::mutex> lock(m_lightShowMutex);

  if (m_lightShowRunning.load())
    return;

  LOG_INFO() << "Starting LightShow";

  const std::string monitor = chooseLightShowMonitor();
  LOG_INFO() << "LightShow selected monitor source: "
             << (monitor.empty() ? "<not found>" : monitor);

  if (monitor.empty()) {
    LOG_WARN() << "LightShow start skipped: HAT monitor source not found";
    return;
  }

  m_lightShow = std::make_unique<LightShow>(NUM_OF_LEDS, monitor);

  m_lightShow->setFrameSender([this](const LightShow::LedFrame &frame) {
    if (m_shuttingDown || !m_lightShowRunning.load())
      return false;

    return m_teensyClient.sendLedFrame(frame);
  });

  m_lightShowRunning = true;
  if (!m_lightShow->start()) {
    LOG_ERROR() << "LightShow failed to start";
    m_lightShowRunning = false;
    m_lightShow->setFrameSender(nullptr);
    m_lightShow.reset();
    return;
  }

}

void MainWindow::stopLightShow() {
  {
    std::lock_guard<std::mutex> lock(m_lightShowMutex);

    if (!m_lightShowRunning.load())
      return;

    LOG_INFO() << "Stopping LightShow";

    m_lightShowRunning = false;
    if (m_lightShow) {
      m_lightShow->setFrameSender(nullptr);
      m_lightShow->stop();
      m_lightShow.reset();
    }
  }

  destroyTemporaryPage("lightshow_settings");

  if (!m_shuttingDown && m_options.on && m_lightsPowerEnabled)
    applyCurrentScheduleState();
}

void MainWindow::queueThemeUploadToTeensy(const Theme &theme) {
  {
    std::lock_guard<std::mutex> lock(m_themeUploadMutex);
    if (themeUploadQueued(m_pendingThemeUploads, theme.id)) {
      LOG_INFO() << "Theme upload already queued themeId=" << theme.id;
      return;
    }
    m_pendingThemeUploads.push_back(theme);
  }

  startNextThemeUpload();
}

void MainWindow::queueThemeUploadsToTeensy(const std::vector<Theme> &themes) {
  {
    std::lock_guard<std::mutex> lock(m_themeUploadMutex);
    for (const auto &theme : themes) {
      if (!themeUploadQueued(m_pendingThemeUploads, theme.id))
        m_pendingThemeUploads.push_back(theme);
    }
  }

  startNextThemeUpload();
}

void MainWindow::startNextThemeUpload() {
  if (m_shuttingDown || !lightsAreOn())
    return;

  bool expected = false;
  if (!m_themeSendBusy.compare_exchange_strong(expected, true))
    return;

  Theme next;
  {
    std::lock_guard<std::mutex> lock(m_themeUploadMutex);
    if (m_pendingThemeUploads.empty()) {
      m_themeSendBusy = false;
      return;
    }
    next = m_pendingThemeUploads.front();
    m_pendingThemeUploads.pop_front();
  }

  sendThemeToTeensyAsync(next.id, next.name, next.colors);
}

void MainWindow::sendThemeToTeensyAsync(int themeId,
                                        const std::string &themeName,
                                        const std::vector<RGB_Color> &colors) {

  if (!lightsAreOn()) {
    LOG_INFO() << "Theme transfer skipped because lights are off themeId="
               << themeId;
    m_themeSendBusy = false;
    return;
  }

  if (m_themeSendThread.joinable())
    m_themeSendThread.join();

  m_themeSendThread = std::thread([this, themeId, themeName, colors]() {
    bool ok = false;
    std::string msg;

    do {
      if (!m_teensyClient.sendThemeColors(static_cast<uint8_t>(themeId),
                                          colors)) {
        msg = "Failed to send '" + themeName + "'";
        LOG_ERROR() << "Theme transfer write failed themeId=" << themeId
                    << " name=" << themeName
                    << " colors=" << colors.size();
        break;
      }

      uint8_t status = TeensyClient::FILE_ERROR;

      for (int i = 0; i < 20; ++i) {
        if (!m_teensyClient.readFileStatus(status)) {
          msg = "Failed to read Teensy status";
          LOG_ERROR() << "Theme transfer status read failed themeId="
                      << themeId << " name=" << themeName;
          break;
        }

        if (status == TeensyClient::FILE_SUCCESS) {
          ok = true;
          msg = "Theme '" + themeName + "' updated";
          LOG_INFO() << "Theme transfer succeeded themeId=" << themeId
                     << " name=" << themeName;
          break;
        }

        if (status == TeensyClient::FILE_ERROR) {
          msg = "Theme '" + themeName + "' failed";
          LOG_ERROR() << "Theme transfer failed on Teensy themeId="
                      << themeId << " name=" << themeName;
          break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }

      if (!ok && msg.empty()) {
        msg = "Theme '" + themeName + "' timed out";
        LOG_ERROR() << "Theme transfer timed out themeId=" << themeId
                    << " name=" << themeName;
      }

    } while (false);

    if (m_shuttingDown) {
      m_themeSendBusy = false;
      return;
    }

    Glib::signal_idle().connect_once([this, msg]() {
      if (m_shuttingDown)
        return;
      showShortToast(msg);
      m_themeSendBusy = false;
      startNextThemeUpload();
    });
  });
}

void MainWindow::sendPatternSpeedsToTeensyAsync(
    const std::vector<Pattern> &patterns) {
  if (!lightsAreOn()) {
    LOG_INFO() << "Pattern speed transfer skipped because lights are off";
    return;
  }

  LOG_INFO() << "Sending pattern speeds to Teensy";

  if (m_patternSendThread.joinable())
    m_patternSendThread.join();

  m_patternSendThread = std::thread([this, patterns]() {
    bool ok = false;
    std::string msg;

    do {
      if (!m_teensyClient.sendPatternSpeeds(patterns)) {
        msg = "Failed to send pattern speeds";
        LOG_ERROR() << "Pattern speed transfer write failed count="
                    << patterns.size();
        break;
      }

      uint8_t status = TeensyClient::FILE_ERROR;

      for (int i = 0; i < 20; ++i) {
        if (!m_teensyClient.readFileStatus(status)) {
          msg = "Failed to read Teensy pattern status";
          LOG_ERROR() << "Pattern speed status read failed";
          break;
        }

        if (status == TeensyClient::FILE_SUCCESS) {
          ok = true;
          msg = "Pattern speeds updated";
          LOG_INFO() << "Pattern speed transfer succeeded";
          break;
        }

        if (status == TeensyClient::FILE_ERROR) {
          msg = "Pattern speed update failed";
          LOG_ERROR() << "Pattern speed transfer failed on Teensy";
          break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }

      if (!ok && msg.empty()) {
        msg = "Pattern speed update timed out";
        LOG_ERROR() << "Pattern speed transfer timed out";
      }

    } while (false);

    if (m_shuttingDown)
      return;

    Glib::signal_idle().connect_once([this, msg]() {
      if (!m_shuttingDown)
        showShortToast(msg);
    });
  });
}

void MainWindow::sendHardwareConfigToTeensyAsync(int numLeds,
                                                 int numShiftRegs) {
  if (m_hardwareConfigBusy.exchange(true)) {
    showShortToast("Hardware config already sending");
    return;
  }

  if (numLeds < 1 || numLeds > kMaxHardwareLeds || numShiftRegs < 1 ||
      numShiftRegs > kMaxHardwareShiftRegs) {
    m_hardwareConfigBusy = false;
    showShortToast("Invalid hardware config");
    return;
  }

  const int requiredShiftRegs =
      static_cast<int>(std::ceil((numLeds * 3) / 8.0));
  if (numShiftRegs < requiredShiftRegs) {
    m_hardwareConfigBusy = false;
    showShortToast("More shift registers required");
    return;
  }

  if (m_hardwareConfigPage)
    m_hardwareConfigPage->set_busy(true);

  if (m_hardwareConfigThread.joinable())
    m_hardwareConfigThread.join();

  const bool wasPowered = lightsAreOn();

  m_hardwareConfigThread =
      std::thread([this, numLeds, numShiftRegs, wasPowered]() {
        bool ok = false;
        bool pageShouldClose = false;
        std::string msg;

        do {
          LOG_INFO() << "Sending hardware config LEDs=" << numLeds
                     << " shift_regs=" << numShiftRegs;

          if (!wasPowered && !setLightsPowerEnabled(true)) {
            msg = "Failed to power Teensy";
            LOG_ERROR() << "Hardware config failed: power on failed";
            break;
          }

          if (!m_teensyClient.sendHardwareConfig(
                  static_cast<uint8_t>(numLeds),
                  static_cast<uint8_t>(numShiftRegs))) {
            msg = "Failed to send hardware config";
            LOG_ERROR() << "Hardware config I2C send failed";
            break;
          }

          uint8_t status = TeensyClient::FILE_ERROR;
          const auto deadline =
              std::chrono::steady_clock::now() + std::chrono::seconds(5);

          while (std::chrono::steady_clock::now() < deadline) {
            if (!m_teensyClient.readFileStatus(status)) {
              msg = "Failed to read Teensy status";
              LOG_ERROR() << "Hardware config status read failed";
              break;
            }

            if (status == TeensyClient::FILE_SUCCESS) {
              ok = true;
              break;
            }

            if (status == TeensyClient::FILE_ERROR) {
              msg = "Teensy rejected hardware config";
              LOG_ERROR() << "Hardware config rejected by Teensy";
              break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }

          if (!ok) {
            if (msg.empty())
              msg = "Hardware config timed out";
            break;
          }

          LOG_INFO() << "Hardware config confirmed; power-cycling Teensy";
          if (!setLightsPowerEnabled(false)) {
            msg = "Saved, but power cycle off failed";
            LOG_ERROR() << "Hardware config power-cycle off failed";
            break;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(600));

          if (!setLightsPowerEnabled(true)) {
            msg = "Saved, but Teensy reboot failed";
            LOG_ERROR() << "Hardware config power-cycle on failed";
            break;
          }

          if (!wasPowered)
            setLightsPowerEnabled(false);

          writeHardwareConfigDefaults(numLeds, numShiftRegs);
          msg = "Hardware config saved";
          pageShouldClose = true;
        } while (false);

        if (!ok && !wasPowered)
          setLightsPowerEnabled(false);

        if (m_shuttingDown) {
          m_hardwareConfigBusy = false;
          return;
        }

        Glib::signal_idle().connect_once([this, msg, pageShouldClose]() {
          if (m_shuttingDown)
            return;

          m_hardwareConfigBusy = false;
          showShortToast(msg);

          if (m_hardwareConfigPage)
            m_hardwareConfigPage->set_busy(false);

          if (pageShouldClose) {
            removeHardwareConfigPage();
            showSettingsPage();
          }
        });
      });
}

void MainWindow::onNewYear(int year) {
  const bool changed = updateMoveableHolidayDates(year);
  LOG_INFO() << "Handling yearly holiday update for " << year;
  if (changed) {
    showShortToast("Holiday dates updated for " + std::to_string(year));
    applyCurrentScheduleState();
  }
}

bool MainWindow::isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int MainWindow::daysInMonth(int year, int month) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2)
    return isLeapYear(year) ? 29 : 28;
  return kDays[month - 1];
}

// returns 0=Sunday, 1=Monday, ... 6=Saturday
int MainWindow::weekdaySunday0(int year, int month, int day) {
  std::tm tmv{};
  tmv.tm_year = year - 1900;
  tmv.tm_mon = month - 1;
  tmv.tm_mday = day;
  tmv.tm_hour = 12; // avoid weird DST edge nonsense
  tmv.tm_isdst = -1;
  std::mktime(&tmv);
  return tmv.tm_wday;
}

std::string MainWindow::mmdd(int month, int day) {
  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(2) << month << "/" << std::setfill('0')
     << std::setw(2) << day;
  return ss.str();
}

std::pair<int, int> MainWindow::easterSunday(int year) {
  const int a = year % 19;
  const int b = year / 100;
  const int c = year % 100;
  const int d = b / 4;
  const int e = b % 4;
  const int f = (b + 8) / 25;
  const int g = (b - f + 1) / 3;
  const int h = (19 * a + b - d - g + 15) % 30;
  const int i = c / 4;
  const int k = c % 4;
  const int l = (32 + 2 * e + 2 * i - h - k) % 7;
  const int m = (a + 11 * h + 22 * l) / 451;
  const int month = (h + l - 7 * m + 114) / 31;
  const int day = ((h + l - 7 * m + 114) % 31) + 1;
  return {month, day};
}

std::pair<int, int> MainWindow::lastMondayOfMay(int year) {
  int day = 31;
  while (weekdaySunday0(year, 5, day) != 1)
    --day;
  return {5, day};
}

std::pair<int, int> MainWindow::firstMondayOfSeptember(int year) {
  int day = 1;
  while (weekdaySunday0(year, 9, day) != 1)
    ++day;
  return {9, day};
}

std::pair<int, int> MainWindow::fourthThursdayOfNovember(int year) {
  int count = 0;
  for (int day = 1; day <= 30; ++day) {
    if (weekdaySunday0(year, 11, day) == 4) {
      ++count;
      if (count == 4)
        return {11, day};
    }
  }
  return {11, 22}; // fallback, should never happen
}

bool MainWindow::updateMoveableHolidayDates(int year) {
  bool changed = false;

  const auto [eMonth, eDay] = easterSunday(year);
  const auto [memMonth, memDay] = lastMondayOfMay(year);
  const auto [labMonth, labDay] = firstMondayOfSeptember(year);
  const auto [thanksMonth, thanksDay] = fourthThursdayOfNovember(year);

  // Easter weekend = Good Friday through Easter Sunday
  int goodFridayMonth = eMonth;
  int goodFridayDay = eDay - 2;

  if (goodFridayDay <= 0) {
    goodFridayMonth -= 1;
    if (goodFridayMonth <= 0)
      goodFridayMonth = 12; // should never happen for Easter, but safe
    goodFridayDay += daysInMonth(year, goodFridayMonth);
  }

  const std::string easterStartDate = mmdd(goodFridayMonth, goodFridayDay);
  const std::string easterEndDate = mmdd(eMonth, eDay);

  const std::string memorialDate = mmdd(memMonth, memDay);
  const std::string laborDate = mmdd(labMonth, labDay);
  const std::string thanksgivingEndDate = mmdd(thanksMonth, thanksDay);

  // Christmas starts the day after Thanksgiving
  int christmasStartMonth = thanksMonth;
  int christmasStartDay = thanksDay + 1;

  if (christmasStartDay > daysInMonth(year, christmasStartMonth)) {
    christmasStartDay = 1;
    christmasStartMonth += 1;
    if (christmasStartMonth > 12)
      christmasStartMonth = 1;
  }

  const std::string christmasStartDate =
      mmdd(christmasStartMonth, christmasStartDay);

  for (auto &s : m_schedule) {
    if (s.name == "Easter") {
      if (s.sDate != easterStartDate || s.sTime != "00:00" ||
          s.eDate != easterEndDate || s.eTime != "23:59") {
        s.sDate = easterStartDate;
        s.sTime = "00:00";
        s.eDate = easterEndDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Memorial Day") {
      if (s.sDate != memorialDate || s.sTime != "00:00" ||
          s.eDate != memorialDate || s.eTime != "23:59") {
        s.sDate = memorialDate;
        s.sTime = "00:00";
        s.eDate = memorialDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Labor Day") {
      if (s.sDate != laborDate || s.sTime != "00:00" || s.eDate != laborDate ||
          s.eTime != "23:59") {
        s.sDate = laborDate;
        s.sTime = "00:00";
        s.eDate = laborDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Thanksgiving") {
      if (s.sDate != "11/01" || s.sTime != "00:00" ||
          s.eDate != thanksgivingEndDate || s.eTime != "23:59") {
        s.sDate = "11/01";
        s.sTime = "00:00";
        s.eDate = thanksgivingEndDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Christmas") {
      if (s.sDate != christmasStartDate || s.sTime != "00:00" ||
          s.eDate != "12/31" || s.eTime != "23:59") {
        s.sDate = christmasStartDate;
        s.sTime = "00:00";
        s.eDate = "12/31";
        s.eTime = "23:59";
        changed = true;
      }
    }
  }

  if (changed) {
    persistSchedule(m_schedule);
    ClockThread::instance().setSchedules(m_schedule);
    LOG_INFO() << "Updated moveable holiday dates for year " << year;
  } else {
    LOG_INFO() << "No moveable holiday date changes needed for year " << year;
  }

  return changed;
}

MainWindow::~MainWindow() {
  LOG_INFO() << "MainWindow dtor begin";

  m_shuttingDown = true;
  m_bluetoothState = 0;

  if (m_startupCacheThread.joinable()) {
    LOG_WARN() << "Startup cache worker still running during shutdown; joining";
    m_startupCacheThread.join();
  }

  if (m_startupHardwareThread.joinable()) {
    LOG_WARN()
        << "Startup hardware worker still running during shutdown; joining";
    m_startupHardwareThread.join();
  }

  if (m_sportsAnimationTimeoutConn.connected())
    m_sportsAnimationTimeoutConn.disconnect();

  if (m_sportsGamesConn.connected())
    m_sportsGamesConn.disconnect();
  if (m_sportsLiveUpdateConn.connected())
    m_sportsLiveUpdateConn.disconnect();
  if (m_sportsHomeScoreConn.connected())
    m_sportsHomeScoreConn.disconnect();
  if (m_sportsGameFinishedConn.connected())
    m_sportsGameFinishedConn.disconnect();

  if (m_liveGamePoller)
    m_liveGamePoller->stop();

  if (m_dailySportsPoller)
    m_dailySportsPoller->stop();

  stopLightShow();

  if (!m_ampSwitch.setEnabled(false)) {
    LOG_WARN() << "Failed to turn amp off in dtor: " << m_ampSwitch.lastError();
  }

  stopBluetoothPolling();

  if (m_btWorker.joinable()) {
    LOG_WARN() << "Bluetooth worker still running during shutdown; Joining";
    m_btWorker.join();
  }

  if (!setBluetoothRfkillBlocked(true)) {
    LOG_WARN() << "Failed to rfkill-block bluetooth in dtor";
  } else {
    LOG_INFO() << "Bluetooth rfkill-blocked in dtor";
  }

  if (m_toastHideConn.connected())
    m_toastHideConn.disconnect();

  if (m_bluetoothEnableTimeoutConn.connected())
    m_bluetoothEnableTimeoutConn.disconnect();

  if (m_idleClockConn.connected())
    m_idleClockConn.disconnect();

  if (m_restartCountdownConn.connected())
    m_restartCountdownConn.disconnect();

  if (m_themeConn.connected())
    m_themeConn.disconnect();

  if (m_powerChangedConn.connected())
    m_powerChangedConn.disconnect();

  if (m_btPowerChangedConn.connected())
    m_btPowerChangedConn.disconnect();

  if (m_newHourConn.connected())
    m_newHourConn.disconnect();

  if (m_newYearConn.connected())
    m_newYearConn.disconnect();

  if (m_newMinuteConn.connected())
    m_newMinuteConn.disconnect();

  if (m_scheduleStartedConn.connected())
    m_scheduleStartedConn.disconnect();

  if (m_scheduleEndedConn.connected())
    m_scheduleEndedConn.disconnect();

  ClockThread::instance().stop();

  if (m_themeSendThread.joinable()) {
    LOG_WARN() << "Theme send worker still running during shutdown; joining";
    m_themeSendThread.join();
  }

  if (m_patternSendThread.joinable()) {
    LOG_WARN() << "Pattern send worker still running during shutdown; joining";
    m_patternSendThread.join();
  }

  if (m_hardwareConfigThread.joinable()) {
    LOG_WARN()
        << "Hardware config worker still running during shutdown; joining";
    m_hardwareConfigThread.join();
  }

  joinManualRestoreThread();

  if (m_lightSensorConn.connected())
    m_lightSensorConn.disconnect();
  m_lightSensorThread.stop();

  if (!setLightsPowerEnabled(false)) {
    LOG_WARN() << "Failed to turn power off in dtor: "
               << m_powerSwitch.lastError();
  }
  m_doorbellThread.stop();

  if (m_doorbellConn.connected())
    m_doorbellConn.disconnect();

  if (m_mobileOptionsConn.connected())
    m_mobileOptionsConn.disconnect();
  if (m_mobileLedsConn.connected())
    m_mobileLedsConn.disconnect();
  if (m_mobileSchedulesConn.connected())
    m_mobileSchedulesConn.disconnect();
  if (m_mobileThemesConn.connected())
    m_mobileThemesConn.disconnect();
  if (m_mobilePatternsConn.connected())
    m_mobilePatternsConn.disconnect();
  if (m_mobileTeamsConn.connected())
    m_mobileTeamsConn.disconnect();
  if (m_mobileStartupSyncConn.connected())
    m_mobileStartupSyncConn.disconnect();

  if (m_mobileLightsPoller)
    m_mobileLightsPoller->stop();

  if (m_environmentConn.connected())
    m_environmentConn.disconnect();
  m_environmentThread.stop();

  destroyAllTemporaryPages();

  LOG_INFO() << "MainWindow dtor complete";
}
