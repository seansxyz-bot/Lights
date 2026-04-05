#include "parserhelper.h"

#include "logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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
  const std::string normalized = normalizeParserName(parserName);
  return settingsPath + "/parsers/" + normalized;
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

    if (!j.contains("fields") || !j.at("fields").is_object()) {
      LOG_ERROR() << "Parser file missing 'fields' object: " << filePath;
      return false;
    }

    const auto &fields = j.at("fields");
    // config.fieldId = fields.value("id", "");
    // config.fieldGameState = fields.value("gameState", "");
    // config.fieldDateTimeUTC = fields.value("dateTimeUTC", "");
    // config.fieldHome = fields.value("home", "");
    // config.fieldAway = fields.value("away", "");
    // config.fieldScoreHome = fields.value("scoreHome", "");
    // config.fieldScoreAway = fields.value("scoreAway", "");

    if (config.type != "json") {
      LOG_ERROR() << "Unsupported parser type: " << config.type;
      return false;
    }

    if (config.mode != "next_game") {
      LOG_ERROR() << "Unsupported parser mode: " << config.mode;
      return false;
    }

    if (config.root.empty()) {
      LOG_ERROR() << "Parser root is empty";
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
    if (!cur->is_object() || !cur->contains(token))
      return "";

    cur = &((*cur)[token]);
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

int ParserHelper::getIntByPath(const nlohmann::json &j, const std::string &path,
                               int fallback) {
  if (path.empty())
    return fallback;

  const nlohmann::json *cur = &j;
  std::stringstream ss(path);
  std::string token;

  while (std::getline(ss, token, '.')) {
    if (!cur->is_object() || !cur->contains(token))
      return fallback;

    cur = &((*cur)[token]);
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

bool ParserHelper::parseNextGameJson(const std::string &jsonText,
                                     const ParserConfig &config,
                                     GameInfo &game) {
  try {
    const auto j = nlohmann::json::parse(jsonText);

    if (!j.contains(config.root) || !j.at(config.root).is_array()) {
      LOG_ERROR() << "Root array not found or not array: " << config.root;
      return false;
    }

    const auto &arr = j.at(config.root);
    if (arr.empty()) {
      LOG_WARN() << "Parser root array is empty: " << config.root;
      return false;
    }

    const nlohmann::json *item = nullptr;

    if (config.select == "first") {
      item = &arr.at(0);
    } else {
      LOG_ERROR() << "Unsupported parser select mode: " << config.select;
      return false;
    }

    if (!item || !item->is_object()) {
      LOG_ERROR() << "Selected parser item is invalid";
      return false;
    }

    game = GameInfo{};
    game.id = getIntByPath(*item, config.fieldId);
    game.gameState = getStringByPath(*item, config.fieldGameState);
    game.dateTimeUTC = getStringByPath(*item, config.fieldDateTimeUTC);
    game.home = getStringByPath(*item, config.fieldHome);
    game.away = getStringByPath(*item, config.fieldAway);
    game.scoreHome = getIntByPath(*item, config.fieldScoreHome, 0);
    game.scoreAway = getIntByPath(*item, config.fieldScoreAway, 0);

    if (game.dateTimeUTC.empty()) {
      LOG_ERROR() << "Parsed game missing dateTimeUTC";
      return false;
    }

    return true;
  } catch (const std::exception &e) {
    LOG_ERROR() << "parseNextGameJson failed: " << e.what();
    return false;
  }
}
