#pragma once

#include "../models/types.h"
#include "../utils/parserhelper.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtkmm.h>

struct SportsNextGameEvent {
  TeamRecord team;
  ParsedNextGame game;
};

struct SportsLiveEvent {
  TeamRecord team;
  ParsedLiveGame game;
  int oldHomeScore = -1;
  int oldAwayScore = -1;
};

class DailySportsPoller {
public:
  explicit DailySportsPoller(const std::string &settingsPath);
  ~DailySportsPoller();

  void start(const std::vector<TeamRecord> &teams);
  void stop();
  void runOnceAsync(const std::vector<TeamRecord> &teams);

  sigc::signal<void, std::vector<SportsNextGameEvent>> &
  signal_games_checked();

private:
  void threadLoop();
  std::vector<SportsNextGameEvent> pollTeams(const std::vector<TeamRecord> &teams);
  void processDispatch();

  std::string m_settingsPath;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_onceRunning{false};
  std::thread m_thread;
  std::thread m_onceThread;
  std::mutex m_mutex;
  std::vector<TeamRecord> m_teams;
  std::vector<SportsNextGameEvent> m_pendingEvents;
  Glib::Dispatcher m_dispatcher;
  sigc::signal<void, std::vector<SportsNextGameEvent>> m_signalGamesChecked;
};

class LiveGamePoller {
public:
  explicit LiveGamePoller(const std::string &settingsPath);
  ~LiveGamePoller();

  void setTeams(const std::vector<TeamRecord> &teams);
  void start();
  void stop();
  bool running() const { return m_running.load(); }

  sigc::signal<void, SportsLiveEvent> &signal_home_score();
  sigc::signal<void, SportsLiveEvent> &signal_game_finished();
  sigc::signal<void, SportsLiveEvent> &signal_live_update();

private:
  struct ScoreState {
    bool valid = false;
    int home = -1;
    int away = -1;
    std::string gameId;
  };

  void threadLoop();
  void pollTeam(const TeamRecord &team);
  void processDispatch();

  std::string m_settingsPath;
  std::atomic<bool> m_running{false};
  std::thread m_thread;
  std::mutex m_mutex;
  std::vector<TeamRecord> m_teams;
  std::map<int, ScoreState> m_scores;
  std::vector<SportsLiveEvent> m_pendingUpdates;
  std::vector<SportsLiveEvent> m_pendingScores;
  std::vector<SportsLiveEvent> m_pendingFinished;
  Glib::Dispatcher m_dispatcher;
  sigc::signal<void, SportsLiveEvent> m_signalHomeScore;
  sigc::signal<void, SportsLiveEvent> m_signalGameFinished;
  sigc::signal<void, SportsLiveEvent> m_signalLiveUpdate;
};
