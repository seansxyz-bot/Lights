#pragma once

#include <string>
#include <vector>

#include "settingsrw.h"

struct ParserConfig {
  std::string type;   // "json"
  std::string mode;   // "next_game"
  std::string root;   // e.g. "games"
  std::string select; // e.g. "first"

  std::string fieldId;
  std::string fieldGameState;
  std::string fieldDateTimeUTC;
  std::string fieldHome;
  std::string fieldAway;
  std::string fieldScoreHome;
  std::string fieldScoreAway;
};

class ParserHelper {
public:
  static std::string normalizeParserName(const std::string &name);

  static std::string parserFilePath(const std::string &settingsPath,
                                    const std::string &parserName);

  static bool loadParserConfig(const std::string &settingsPath,
                               const std::string &parserName,
                               ParserConfig &config);

  static bool parseNextGameJson(const std::string &jsonText,
                                const ParserConfig &config, GameInfo &game);

private:
  static std::string getStringByPath(const nlohmann::json &j,
                                     const std::string &path);

  static int getIntByPath(const nlohmann::json &j, const std::string &path,
                          int fallback = 0);
};
