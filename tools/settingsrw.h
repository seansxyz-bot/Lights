#pragma once

#include "httphelper.h"

#if (UBUNTU == 1)
#define HOME_DIR "/home/dev"
#define SETTINGS_PATH "/home/dev/.lightcontroller"
#else
#define HOME_DIR "/home/lights"
#define SETTINGS_PATH "/home/lights/.lightcontroller"
#endif

#define NUM_OF_LEDS 19

#include "fstream"
#include "iostream"
#include <atomic>
#include <gdk/gdk.h>
#include <sstream>
#include <vector>

extern HttpHelper http;
extern std::atomic<bool> writeToServer;

struct RGB_Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct Theme {
  std::string name;
  int id;
  std::vector<RGB_Color> colors;
};

struct TeamRecord {
  int id = 0;

  std::string name;
  std::string league;
  std::string teamCode;

  std::string nextGameUrlTemplate;
  std::string nextGameParser;

  std::string liveGameUrlTemplate;
  std::string liveGameParser;

  std::string apiTeamId;

  int enabled = 1;
  int displayOrder = 0;

  std::string themeName;
  std::string nextGameUtc;
  // hydrated/runtime-only fields
  Glib::RefPtr<Gdk::Pixbuf> logo;
  Gdk::RGBA c1;
  Gdk::RGBA c2;
};

struct TeamStats {
  int ranking = 0;

  int divisionRank = 0;
  int conferenceRank = 0;

  int wins = 0;
  int losses = 0;
  int draws = 0;
  int overtimeLosses = 0;

  int points = 0;
  double winPct = 0.0;

  std::string conference;
  std::string division;
  std::string streak;
  std::string seasonLabel;
  std::string recordText;
};

struct GameInfo {
  int id;
  std::string gameState;

  std::string home;
  std::string away;
  std::string militaryTime;      // 24 Hour
  std::string standardTime;      // 12 Hour
  std::string displayedDateTime; // MM-DD @ HH:MM am/pm
  std::string dateTimeUTC;       // orignal time string
  std::string scheduledDate;     // MM/DD
  std::string venue;
  std::string scoreHome;
  std::string scoreAway;

  bool isHomeGame = false;
  bool isPlayoffGame = false;

  std::string opponentCode;
  std::string opponentName;
};

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

std::vector<LEDData> readLEDInfo(std::string path);
int writeLEDInfo(std::string path, std::vector<LEDData> data);

Options readOptions(std::string path);
int writeOptions(std::string path, Options data);

std::vector<Schedule> readSchedule(std::string path);
int writeSchedule(std::string path, std::vector<Schedule> data);

GameInfo readNextGame(std::string path, std::string fileName);
int writeNextGame(std::string path, std::string fileName, GameInfo data);

std::vector<Theme> readThemeColors(const std::string &dbPath);
int writeThemeColors(const std::string &dbPath,
                     const std::vector<Theme> &themes);

std::vector<TeamRecord> readTeams(const std::string &dbPath);
bool writeTeam(const std::string &dbPath, const TeamRecord &team);
bool deleteTeam(const std::string &dbPath, int id);

bool loadParserConfig(const std::string &settingsPath,
                      const std::string &parserName, ParserConfig &outConfig);

bool validateParserConfig(const ParserConfig &cfg, std::string &error);

bool parseNextGameJson(const std::string &payload, const ParserConfig &cfg,
                       ParsedNextGame &outGame, std::string &error);

bool parseLiveGameJson(const std::string &payload, const ParserConfig &cfg,
                       ParsedLiveGame &outGame, std::string &error);
