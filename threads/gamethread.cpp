#include "gamethread.h"

#include <chrono>
#include <iostream>
#include <thread>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
size_t curl_write_fn(void *contents, size_t size, size_t nmemb, void *userp) {
  const size_t total = size * nmemb;
  auto *out = static_cast<std::string *>(userp);
  out->append(static_cast<char *>(contents), total);
  return total;
}
} // namespace

GameThread::GameThread(uint8_t watchMask, const std::string &nhlGameId,
                       const std::string &mlbGameId,
                       const std::string &espnGameId)
    : m_watchMask(watchMask), m_nhlGameId(nhlGameId), m_mlbGameId(mlbGameId),
      m_espnGameId(espnGameId) {}

GameThread::~GameThread() { stop(); }

void GameThread::start() {
  if (m_running)
    return;

  m_running = true;
  m_thread = std::thread(&GameThread::thread_main, this);
}

void GameThread::stop() {
  if (!m_running)
    return;

  m_running = false;

  if (m_thread.joinable())
    m_thread.join();
}

bool GameThread::running() const { return m_running; }

void GameThread::set_poll_interval_ms(int ms) {
  if (ms < 250)
    ms = 250;
  m_pollIntervalMs = ms;
}

sigc::signal<void, int, int> &GameThread::signal_kraken_score() {
  return m_signalKrakenScore;
}

sigc::signal<void, int, int> &GameThread::signal_mariners_score() {
  return m_signalMarinersScore;
}

sigc::signal<void, int, int> &GameThread::signal_seahawks_score() {
  return m_signalSeahawksScore;
}

void GameThread::thread_main() {
  while (m_running) {
    if (has_watch_flag(m_watchMask, Kraken) && !m_nhlGameId.empty()) {
      poll_kraken();
    }

    if (has_watch_flag(m_watchMask, Mariners) && !m_mlbGameId.empty()) {
      poll_mariners();
    }

    if (has_watch_flag(m_watchMask, Seahawks) && !m_espnGameId.empty()) {
      poll_seahawks();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(m_pollIntervalMs));
  }
}

bool GameThread::poll_kraken() {
  try {
    // Example:
    // https://api-web.nhle.com/v1/gamecenter/2025021040/landing
    const std::string url =
        "https://api-web.nhle.com/v1/gamecenter/" + m_nhlGameId + "/landing";

    std::string response;
    if (!http_get(url, response)) {
      std::cerr << "GameThread NHL: HTTP GET failed\n";
      return false;
    }

    const auto game = json::parse(response);

    if (!game.contains("homeTeam") || !game.contains("awayTeam"))
      return false;

    const std::string homeAbbrev =
        game["homeTeam"].value("abbrev", std::string{});
    const std::string awayAbbrev =
        game["awayTeam"].value("abbrev", std::string{});

    const int homeScore = game["homeTeam"].value("score", 0);
    const int awayScore = game["awayTeam"].value("score", 0);

    // Optional sanity check: make sure Seattle is in the game
    if (homeAbbrev != "SEA" && awayAbbrev != "SEA") {
      std::cerr << "GameThread NHL: game does not appear to be Kraken\n";
      return false;
    }

    if (!m_krakenLastScore.valid || m_krakenLastScore.away != awayScore ||
        m_krakenLastScore.home != homeScore) {

      m_krakenLastScore.valid = true;
      m_krakenLastScore.away = awayScore;
      m_krakenLastScore.home = homeScore;

      m_signalKrakenScore.emit(awayScore, homeScore);
    }

    return true;
  } catch (const std::exception &e) {
    std::cerr << "GameThread NHL exception: " << e.what() << std::endl;
    return false;
  }
}

bool GameThread::poll_mariners() {
  // Placeholder for now.
  //
  // Likely target will be MLB StatsAPI live game feed using gamePk/gameId,
  // then extract away/home runs and compare to cached values.
  //
  // When you’re ready, we can wire this for real.
  return false;
}

bool GameThread::poll_seahawks() {
  // Placeholder for now.
  //
  // ESPN scoreboard/game endpoints expose competitors with:
  // - homeAway
  // - team.abbreviation
  // - score
  //
  // So later we can find SEA and emit(awayScore, homeScore).
  return false;
}

bool GameThread::http_get(const std::string &url, std::string &response) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;

  response.clear();

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_fn);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "lights-gamethread/1.0");

  const CURLcode res = curl_easy_perform(curl);

  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  curl_easy_cleanup(curl);

  return (res == CURLE_OK && httpCode >= 200 && httpCode < 300);
}
