#pragma once

#include <curl/curl.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct ParserConfig {
  std::string name;
  std::string type; // json, xml
  std::string mode; // next_game, live_game
  std::string root;
  std::string select;

  std::map<std::string, std::string> fields;
};

struct ParsedNextGame {
  bool valid = false;
  std::string id;
  std::string state;
  std::string dateUtc;
  std::string homeTeam;
  std::string awayTeam;
  int homeScore = 0;
  int awayScore = 0;
};

struct ParsedLiveGame {
  bool valid = false;
  std::string id;
  std::string state;
  std::string dateUtc;
  std::string homeTeam;
  std::string awayTeam;
  int homeScore = 0;
  int awayScore = 0;
  std::string period;
  std::string clock;
};

class ParserHelper {
public:
  static std::string normalizeParserName(const std::string &name);

  static std::string parserFilePath(const std::string &settingsPath,
                                    const std::string &parserName);

  bool loadParserConfig(const std::string &settingsPath,
                        const std::string &parserName, ParserConfig &outConfig);

  bool validateParserConfig(const ParserConfig &cfg, std::string &error);

  bool parseNextGameJson(const std::string &payload, const ParserConfig &cfg,
                         ParsedNextGame &outGame, std::string &error);

  bool parseLiveGameJson(const std::string &payload, const ParserConfig &cfg,
                         ParsedLiveGame &outGame, std::string &error);

  static std::string getStringByPath(const nlohmann::json &j,
                                     const std::string &path);

  static int getIntByPath(const nlohmann::json &j, const std::string &path,
                          int fallback = 0);

private:
  static const nlohmann::json *selectRootItem(const nlohmann::json &j,
                                              const ParserConfig &cfg,
                                              std::string &error);
};
