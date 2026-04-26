#include "sportspoller.h"

#include "../drivers/network/httphelper.h"
#include "../utils/logger.h"

#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace {
std::string replaceAll(std::string s, const std::string &from,
                       const std::string &to) {
  if (from.empty())
    return s;

  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
}

std::string expandTemplate(std::string url, const TeamRecord &team,
                           const std::string &gameId = "") {
  url = replaceAll(url, "{team_code}", team.teamCode);
  url = replaceAll(url, "{api_team_id}", team.apiTeamId);
  url = replaceAll(url, "{team_id}", team.apiTeamId);
  url = replaceAll(url, "{game_id}", gameId.empty() ? team.lastGameId : gameId);
  return url;
}

bool isTerminalState(const std::string &state) {
  std::string s = state;
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  return s == "final" || s == "off" || s == "complete" ||
         s == "completed" || s == "post" || s == "closed";
}

bool isLiveState(const std::string &state) {
  std::string s = state;
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  return s == "live" || s == "inprogress" || s == "in_progress" ||
         s == "active" || s == "critical" || s == "gameon";
}
} // namespace

DailySportsPoller::DailySportsPoller(const std::string &settingsPath)
    : m_settingsPath(settingsPath) {
  m_dispatcher.connect(
      sigc::mem_fun(*this, &DailySportsPoller::processDispatch));
}

DailySportsPoller::~DailySportsPoller() { stop(); }

void DailySportsPoller::start(const std::vector<TeamRecord> &teams) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_teams = teams;
  }

  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true))
    return;

  m_thread = std::thread(&DailySportsPoller::threadLoop, this);
}

void DailySportsPoller::stop() {
  bool expected = true;
  if (!m_running.compare_exchange_strong(expected, false))
    return;

  if (m_thread.joinable())
    m_thread.join();
}

void DailySportsPoller::runOnceAsync(const std::vector<TeamRecord> &teams) {
  std::thread([this, teams]() {
    auto events = pollTeams(teams);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_pendingEvents = std::move(events);
    }
    m_dispatcher.emit();
  }).detach();
}

void DailySportsPoller::threadLoop() {
  while (m_running) {
    for (int i = 0; i < 24 * 60 * 60 && m_running; ++i)
      std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!m_running)
      break;

    std::vector<TeamRecord> teams;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      teams = m_teams;
    }

    auto events = pollTeams(teams);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_pendingEvents = std::move(events);
    }
    m_dispatcher.emit();
  }
}

std::vector<SportsNextGameEvent>
DailySportsPoller::pollTeams(const std::vector<TeamRecord> &teams) {
  std::vector<SportsNextGameEvent> events;
  ParserHelper parser;
  HttpHelper http;

  for (const auto &team : teams) {
    if (!team.enabled || team.nextGameUrlTemplate.empty() ||
        team.nextGameParser.empty())
      continue;

    ParserConfig cfg;
    if (!parser.loadParserConfig(m_settingsPath, team.nextGameParser, cfg))
      continue;

    std::string error;
    if (cfg.mode != "next_game" ||
        !parser.validateParserConfig(cfg, error)) {
      LOG_WARN() << "DailySportsPoller parser invalid for " << team.name
                 << ": " << error;
      continue;
    }

    const std::string url = expandTemplate(team.nextGameUrlTemplate, team);
    const std::string payload = http.get(url);
    if (payload.empty()) {
      LOG_WARN() << "DailySportsPoller empty response for " << team.name;
      continue;
    }

    ParsedNextGame game;
    if (!parser.parseNextGameJson(payload, cfg, game, error)) {
      LOG_WARN() << "DailySportsPoller parse failed for " << team.name << ": "
                 << error;
      continue;
    }

    events.push_back({team, game});
  }

  return events;
}

void DailySportsPoller::processDispatch() {
  std::vector<SportsNextGameEvent> events;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    events.swap(m_pendingEvents);
  }

  m_signalGamesChecked.emit(events);
}

sigc::signal<void, std::vector<SportsNextGameEvent>> &
DailySportsPoller::signal_games_checked() {
  return m_signalGamesChecked;
}

LiveGamePoller::LiveGamePoller(const std::string &settingsPath)
    : m_settingsPath(settingsPath) {
  m_dispatcher.connect(sigc::mem_fun(*this, &LiveGamePoller::processDispatch));
}

LiveGamePoller::~LiveGamePoller() { stop(); }

void LiveGamePoller::setTeams(const std::vector<TeamRecord> &teams) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_teams = teams;
}

void LiveGamePoller::start() {
  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true))
    return;

  m_thread = std::thread(&LiveGamePoller::threadLoop, this);
}

void LiveGamePoller::stop() {
  bool expected = true;
  if (!m_running.compare_exchange_strong(expected, false))
    return;

  if (m_thread.joinable())
    m_thread.join();
}

void LiveGamePoller::threadLoop() {
  while (m_running) {
    std::vector<TeamRecord> teams;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      teams = m_teams;
    }

    for (const auto &team : teams)
      pollTeam(team);

    for (int i = 0; i < 5 && m_running; ++i)
      std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void LiveGamePoller::pollTeam(const TeamRecord &team) {
  if (!team.enabled || team.liveGameUrlTemplate.empty() ||
      team.liveGameParser.empty())
    return;

  ParserHelper parser;
  ParserConfig cfg;
  if (!parser.loadParserConfig(m_settingsPath, team.liveGameParser, cfg))
    return;

  HttpHelper http;
  const std::string url = expandTemplate(team.liveGameUrlTemplate, team);
  const std::string payload = http.get(url);
  if (payload.empty()) {
    LOG_WARN() << "LiveGamePoller empty response for " << team.name;
    return;
  }

  std::string error;
  ParsedLiveGame game;
  if (!parser.parseLiveGameJson(payload, cfg, game, error)) {
    LOG_WARN() << "LiveGamePoller parse failed for " << team.name << ": "
               << error;
    return;
  }

  SportsLiveEvent event;
  event.team = team;
  event.game = game;

  bool homeScore = false;
  bool finished = false;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto &state = m_scores[team.id];
    if (!state.valid && team.lastGameId == game.id && team.lastHomeScore >= 0) {
      state.home = team.lastHomeScore;
      state.away = team.lastAwayScore;
      state.gameId = team.lastGameId;
      state.valid = true;
    }

    event.oldHomeScore = state.home;
    event.oldAwayScore = state.away;

    if (state.valid && game.homeScore > state.home)
      homeScore = true;

    state.valid = true;
    state.home = game.homeScore;
    state.away = game.awayScore;
    state.gameId = game.id;

    m_pendingUpdates.push_back(event);

    if (homeScore)
      m_pendingScores.push_back(event);

    if (isTerminalState(game.state)) {
      finished = true;
      m_pendingFinished.push_back(event);
    }
  }

  if (isLiveState(game.state) || homeScore || finished)
    m_dispatcher.emit();
}

void LiveGamePoller::processDispatch() {
  std::vector<SportsLiveEvent> updates;
  std::vector<SportsLiveEvent> scores;
  std::vector<SportsLiveEvent> finished;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    updates.swap(m_pendingUpdates);
    scores.swap(m_pendingScores);
    finished.swap(m_pendingFinished);
  }

  for (const auto &e : updates)
    m_signalLiveUpdate.emit(e);

  for (const auto &e : scores)
    m_signalHomeScore.emit(e);

  for (const auto &e : finished)
    m_signalGameFinished.emit(e);
}

sigc::signal<void, SportsLiveEvent> &LiveGamePoller::signal_home_score() {
  return m_signalHomeScore;
}

sigc::signal<void, SportsLiveEvent> &LiveGamePoller::signal_game_finished() {
  return m_signalGameFinished;
}

sigc::signal<void, SportsLiveEvent> &LiveGamePoller::signal_live_update() {
  return m_signalLiveUpdate;
}
