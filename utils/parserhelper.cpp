#include "parserhelper.h"

#include "logger.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

std::string ParserHelper::normalizeParserName(const std::string &name) {
  std::string s = name;

  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";

  const auto last = s.find_last_not_of(" \t\r\n");
  s = s.substr(first, last - first + 1);

  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  for (char &c : s) {
    if (c == ' ')
      c = '_';
  }

  return s;
}

std::string ParserHelper::parserFilePath(const std::string &settingsPath,
                                         const std::string &parserName) {
  (void)settingsPath;
  const std::string normalized = normalizeParserName(parserName);
  return "/home/lights/.local/share/lights/parser/" + normalized;
}

bool ParserHelper::loadParserConfig(const std::string &settingsPath,
                                    const std::string &parserName,
                                    ParserConfig &config) {
  const std::string filePath = parserFilePath(settingsPath, parserName);

  std::ifstream in(filePath);
  if (!in.is_open()) {
    LOG_ERROR() << "Failed to open parser file: " << filePath;
    return false;
  }

  std::stringstream buffer;
  buffer << in.rdbuf();

  try {
    const auto j = nlohmann::json::parse(buffer.str());

    config.type = j.value("type", "");
    config.mode = j.value("mode", "");
    config.root = j.value("root", "");
    config.select = j.value("select", "first");
    config.items = j.value("items", "");

    if (!j.contains("fields") || !j.at("fields").is_object()) {
      LOG_ERROR() << "Parser file missing 'fields' object: " << filePath;
      return false;
    }

    const auto &fields = j.at("fields");
    config.fields.clear();
    for (auto it = fields.begin(); it != fields.end(); ++it) {
      if (it.value().is_string())
        config.fields[it.key()] = it.value().get<std::string>();
    }

    if (config.type != "json") {
      LOG_ERROR() << "Unsupported parser type: " << config.type;
      return false;
    }

    if (config.mode != "next_game" && config.mode != "live_game") {
      LOG_ERROR() << "Unsupported parser mode: " << config.mode;
      return false;
    }

    if (config.select.empty()) {
      config.select = "first";
    }

    return true;
  } catch (const std::exception &e) {
    LOG_ERROR() << "Failed parsing parser file: " << filePath
                << " error: " << e.what();
    return false;
  }
}

// bool ParserHelper::parseNextGame(const std::string &payload,
//                                  const ParserConfig &cfg,
//                                  ParsedNextGame &out,
//                                  std::string &error) {
//     if (cfg.type == "json" && cfg.mode == "next_game") {
//         return parseNextGameJson(payload, cfg, out, error);
//     }

//     if (cfg.type == "xml" && cfg.mode == "next_game") {
//         return parseNextGameXml(payload, cfg, out, error);
//     }

//     error = "Unsupported parser type/mode: " + cfg.type + "/" + cfg.mode;
//     return false;
// }

std::string ParserHelper::getStringByPath(const nlohmann::json &j,
                                          const std::string &path) {
  if (path.empty())
    return "";

  const nlohmann::json *cur = &j;
  std::stringstream ss(path);
  std::string token;

  while (std::getline(ss, token, '.')) {
    if (cur->is_array()) {
      try {
        const size_t idx = static_cast<size_t>(std::stoul(token));
        if (idx >= cur->size())
          return "";
        cur = &((*cur)[idx]);
      } catch (...) {
        return "";
      }
    } else {
      if (!cur->is_object() || !cur->contains(token))
        return "";

      cur = &((*cur)[token]);
    }
  }

  if (cur->is_string())
    return cur->get<std::string>();

  if (cur->is_number_integer())
    return std::to_string(cur->get<int>());

  if (cur->is_number_unsigned())
    return std::to_string(cur->get<unsigned int>());

  if (cur->is_number_float())
    return std::to_string(cur->get<double>());

  if (cur->is_boolean())
    return cur->get<bool>() ? "true" : "false";

  return "";
}

namespace {
std::string lowerString(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string normalizedStatus(const std::string &raw) {
  const std::string s = lowerString(raw);
  if (s == "live" || s == "inprogress" || s == "in_progress" ||
      s == "in progress" || s == "active" || s == "critical" ||
      s == "gameon")
    return "live";
  if (s == "final" || s == "off" || s == "complete" || s == "completed" ||
      s == "post" || s == "closed" || s == "ended")
    return "final";
  if (s == "postponed" || s == "cancelled" || s == "canceled")
    return s;
  if (s == "pre" || s == "pregame")
    return "pregame";
  return s.empty() ? "scheduled" : s;
}
} // namespace

int ParserHelper::getIntByPath(const nlohmann::json &j, const std::string &path,
                               int fallback) {
  if (path.empty())
    return fallback;

  const nlohmann::json *cur = &j;
  std::stringstream ss(path);
  std::string token;

  while (std::getline(ss, token, '.')) {
    if (cur->is_array()) {
      try {
        const size_t idx = static_cast<size_t>(std::stoul(token));
        if (idx >= cur->size())
          return fallback;
        cur = &((*cur)[idx]);
      } catch (...) {
        return fallback;
      }
    } else {
      if (!cur->is_object() || !cur->contains(token))
        return fallback;

      cur = &((*cur)[token]);
    }
  }

  if (cur->is_number_integer())
    return cur->get<int>();

  if (cur->is_number_unsigned())
    return static_cast<int>(cur->get<unsigned int>());

  if (cur->is_string()) {
    try {
      return std::stoi(cur->get<std::string>());
    } catch (...) {
      return fallback;
    }
  }

  return fallback;
}

bool ParserHelper::validateParserConfig(const ParserConfig &cfg,
                                        std::string &error) {
  if (cfg.type != "json") {
    error = "Unsupported parser type: " + cfg.type;
    return false;
  }

  if (cfg.mode != "next_game" && cfg.mode != "live_game") {
    error = "Unsupported parser mode: " + cfg.mode;
    return false;
  }

  const auto requireField = [&](const std::string &field) {
    return cfg.fields.find(field) != cfg.fields.end() &&
           !cfg.fields.at(field).empty();
  };

  if (!requireField("id") || !requireField("dateTimeUTC") ||
      !requireField("home") || !requireField("away")) {
    error = "Parser needs id, dateTimeUTC, home, and away fields";
    return false;
  }

  if (cfg.mode == "live_game" &&
      (!requireField("scoreHome") || !requireField("scoreAway"))) {
    error = "Live parser needs scoreHome and scoreAway fields";
    return false;
  }

  return true;
}

const nlohmann::json *ParserHelper::getByPath(const nlohmann::json &j,
                                              const std::string &path) {
  const nlohmann::json *cur = &j;

  if (path.empty())
    return cur;

  std::stringstream ss(path);
  std::string token;
  while (std::getline(ss, token, '.')) {
    if (cur->is_array()) {
      try {
        const size_t idx = static_cast<size_t>(std::stoul(token));
        if (idx >= cur->size())
          return nullptr;
        cur = &((*cur)[idx]);
      } catch (...) {
        return nullptr;
      }
    } else {
      if (!cur->is_object() || !cur->contains(token))
        return nullptr;
      cur = &((*cur)[token]);
    }
  }

  return cur;
}

const nlohmann::json *ParserHelper::selectRootItem(const nlohmann::json &j,
                                                   const ParserConfig &cfg,
                                                   std::string &error) {
  const nlohmann::json *root = getByPath(j, cfg.root);
  if (!root) {
    error = "Parser root not found: " + cfg.root;
    return nullptr;
  }

  if (cfg.select == "next_upcoming") {
    const nlohmann::json *best = nullptr;
    std::string bestUtc;
    std::time_t now = std::time(nullptr);

    const auto consider = [&](const nlohmann::json &candidate) {
      auto it = cfg.fields.find("dateTimeUTC");
      if (it == cfg.fields.end())
        return;
      const std::string utc = getStringByPath(candidate, it->second);
      if (utc.empty())
        return;

      std::string s = utc;
      if (!s.empty() && s.back() == 'Z')
        s.pop_back();
      std::tm tm{};
      std::istringstream ss(s);
      ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
      if (ss.fail()) {
        ss.clear();
        ss.str(s);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail())
          return;
      }
      tm.tm_isdst = 0;
      const std::time_t t = timegm(&tm);
      if (t == -1 || t < now - 6 * 60 * 60)
        return;
      if (!best || utc < bestUtc) {
        best = &candidate;
        bestUtc = utc;
      }
    };

    if (root->is_array()) {
      for (const auto &entry : *root) {
        const nlohmann::json *items = cfg.items.empty() ? &entry
                                                        : getByPath(entry, cfg.items);
        if (!items)
          continue;
        if (items->is_array()) {
          for (const auto &item : *items)
            consider(item);
        } else if (items->is_object()) {
          consider(*items);
        }
      }
    } else if (root->is_object()) {
      const nlohmann::json *items = cfg.items.empty() ? root : getByPath(*root, cfg.items);
      if (items && items->is_array()) {
        for (const auto &item : *items)
          consider(item);
      } else if (items && items->is_object()) {
        consider(*items);
      }
    }

    if (!best)
      error = "No upcoming item found";
    return best;
  }

  if (root->is_array()) {
    if (root->empty()) {
      error = "Parser root array is empty";
      return nullptr;
    }

    if (cfg.select.empty() || cfg.select == "first")
      return &root->at(0);

    error = "Unsupported parser select: " + cfg.select;
    return nullptr;
  }

  if (!root->is_object()) {
    error = "Parser root did not resolve to an object";
    return nullptr;
  }

  return root;
}

bool ParserHelper::parseNextGameJson(const std::string &payload,
                                     const ParserConfig &cfg,
                                     ParsedNextGame &outGame,
                                     std::string &error) {
  outGame = ParsedNextGame{};
  if (!validateParserConfig(cfg, error))
    return false;

  try {
    const auto j = nlohmann::json::parse(payload);
    const nlohmann::json *item = selectRootItem(j, cfg, error);
    if (!item)
      return false;

    const auto field = [&](const std::string &name) -> std::string {
      auto it = cfg.fields.find(name);
      return it == cfg.fields.end() ? "" : it->second;
    };

    outGame.id = getStringByPath(*item, field("id"));
    outGame.state = getStringByPath(*item, field("gameState"));
    outGame.dateUtc = getStringByPath(*item, field("dateTimeUTC"));
    outGame.homeTeam = getStringByPath(*item, field("home"));
    outGame.awayTeam = getStringByPath(*item, field("away"));
    outGame.homeScore = getIntByPath(*item, field("scoreHome"), 0);
    outGame.awayScore = getIntByPath(*item, field("scoreAway"), 0);
    outGame.normalizedStatus = normalizedStatus(outGame.state);

    outGame.valid = !outGame.id.empty() && !outGame.dateUtc.empty();
    if (!outGame.valid)
      error = "Parsed next game missing id or dateTimeUTC";

    return outGame.valid;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}

bool ParserHelper::parseLiveGameJson(const std::string &payload,
                                     const ParserConfig &cfg,
                                     ParsedLiveGame &outGame,
                                     std::string &error) {
  outGame = ParsedLiveGame{};
  if (!validateParserConfig(cfg, error))
    return false;

  try {
    const auto j = nlohmann::json::parse(payload);
    const nlohmann::json *item = selectRootItem(j, cfg, error);
    if (!item)
      return false;

    const auto field = [&](const std::string &name) -> std::string {
      auto it = cfg.fields.find(name);
      return it == cfg.fields.end() ? "" : it->second;
    };

    outGame.id = getStringByPath(*item, field("id"));
    outGame.state = getStringByPath(*item, field("gameState"));
    outGame.dateUtc = getStringByPath(*item, field("dateTimeUTC"));
    outGame.homeTeam = getStringByPath(*item, field("home"));
    outGame.awayTeam = getStringByPath(*item, field("away"));
    outGame.homeScore = getIntByPath(*item, field("scoreHome"), 0);
    outGame.awayScore = getIntByPath(*item, field("scoreAway"), 0);
    outGame.period = getStringByPath(*item, field("period"));
    outGame.clock = getStringByPath(*item, field("clock"));
    outGame.normalizedStatus = normalizedStatus(outGame.state);

    outGame.valid = !outGame.id.empty();
    if (!outGame.valid)
      error = "Parsed live game missing id";

    return outGame.valid;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
