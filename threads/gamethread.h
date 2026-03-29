#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include <sigc++/sigc++.h>

class GameThread {
public:
  enum WatchType : uint8_t {
    None = 0,
    Kraken = 1 << 0,
    Mariners = 1 << 1,
    Seahawks = 1 << 2
  };

public:
  GameThread(uint8_t watchMask, const std::string &nhlGameId = "",
             const std::string &mlbGameId = "",
             const std::string &espnGameId = "");

  ~GameThread();

  GameThread(const GameThread &) = delete;
  GameThread &operator=(const GameThread &) = delete;

  void start();
  void stop();
  bool running() const;

  void set_poll_interval_ms(int ms);

  sigc::signal<void, int, int> &signal_kraken_score();
  sigc::signal<void, int, int> &signal_mariners_score();
  sigc::signal<void, int, int> &signal_seahawks_score();

private:
  struct ScoreState {
    bool valid = false;
    int away = -1;
    int home = -1;
  };

private:
  void thread_main();

  bool poll_kraken();
  bool poll_mariners();
  bool poll_seahawks();

  static bool http_get(const std::string &url, std::string &response);

private:
  uint8_t m_watchMask = None;

  std::string m_nhlGameId;
  std::string m_mlbGameId;
  std::string m_espnGameId;

  std::atomic<bool> m_running{false};
  std::thread m_thread;
  int m_pollIntervalMs = 500;

  ScoreState m_krakenLastScore;
  ScoreState m_marinersLastScore;
  ScoreState m_seahawksLastScore;

  sigc::signal<void, int, int> m_signalKrakenScore;
  sigc::signal<void, int, int> m_signalMarinersScore;
  sigc::signal<void, int, int> m_signalSeahawksScore;
};

// Bitwise helpers
inline GameThread::WatchType operator|(GameThread::WatchType a,
                                       GameThread::WatchType b) {
  return static_cast<GameThread::WatchType>(static_cast<uint8_t>(a) |
                                            static_cast<uint8_t>(b));
}

inline bool has_watch_flag(uint8_t mask, GameThread::WatchType flag) {
  return (mask & static_cast<uint8_t>(flag)) != 0;
}
