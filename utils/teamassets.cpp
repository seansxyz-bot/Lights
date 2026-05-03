#include "teamassets.h"

#include "../storage/common.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>

namespace {
std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string upper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return value;
}

std::filesystem::path teamAssetsRoot() {
  return std::filesystem::path(runtimeSettingsPath()) / "animations" /
         "team_assets";
}

std::string mappedAssetId(const std::string &league, const std::string &key) {
  const std::string lg = upper(league);
  const std::string k = upper(key);
  if (k.empty())
    return "";

  static const std::map<std::string, std::string> nhl = {
      {"ANA", "24"}, {"BOS", "6"},  {"BUF", "7"},  {"CAR", "12"},
      {"CBJ", "29"}, {"CGY", "20"}, {"CHI", "16"}, {"COL", "21"},
      {"DAL", "25"}, {"DET", "17"}, {"EDM", "22"}, {"FLA", "13"},
      {"LAK", "26"}, {"MIN", "30"}, {"MTL", "8"},  {"NJD", "1"},
      {"NSH", "18"}, {"NYI", "2"},  {"NYR", "3"},  {"OTT", "9"},
      {"PHI", "4"},  {"PIT", "5"},  {"SEA", "55"}, {"SJS", "28"},
      {"STL", "19"}, {"TB", "14"},  {"TBL", "14"}, {"TOR", "10"},
      {"UTA", "59"}, {"VAN", "23"}, {"VGK", "54"}, {"WAS", "15"},
      {"WSH", "15"}, {"WPG", "52"}};

  if (lg == "NFL")
    return k;
  if (lg == "NHL") {
    auto it = nhl.find(k);
    if (it != nhl.end())
      return it->second;
  }
  return k;
}

std::string assetIdForTeam(const TeamRecord &team) {
  const std::string lg = upper(team.league);
  if (lg == "NFL")
    return mappedAssetId(team.league,
                         !team.teamCode.empty() ? team.teamCode : team.apiTeamId);

  const std::string primary = !team.apiTeamId.empty() ? team.apiTeamId : team.teamCode;
  return mappedAssetId(team.league, primary);
}

std::string fallbackLogoPath() {
  const std::filesystem::path blank =
      std::filesystem::path(runtimeSettingsPath()) / "icons" / "blank.svg";
  if (std::filesystem::exists(blank))
    return blank.string();

  const std::filesystem::path none =
      std::filesystem::path(runtimeSettingsPath()) / "icons" / "none.png";
  if (std::filesystem::exists(none))
    return none.string();

  return "";
}
} // namespace

std::string getTeamLogoStoragePath(const TeamRecord &team) {
  const std::string lg = lower(team.league);
  const std::string assetId = assetIdForTeam(team);
  if (lg.empty() || assetId.empty())
    return "";

  return (teamAssetsRoot() / lg / (assetId + "_l.png")).string();
}

std::string getTeamLogoPath(const TeamRecord &team) {
  const std::string path = getTeamLogoStoragePath(team);
  if (!path.empty() && std::filesystem::exists(path))
    return path;

  if (!path.empty())
    LOG_WARN() << "Missing team logo asset " << path;

  return fallbackLogoPath();
}
