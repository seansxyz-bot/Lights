#pragma once

#define SR_SWITCH 10
#define TEENSY_SWITCH 9

#include "readerwriter.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <sigc++/sigc++.h>
#include <string>
#include <thread>

class PowerSwitch {
public:
  explicit PowerSwitch(const std::string &chipName = "gpiochip0");
  ~PowerSwitch();

  void start();
  void stop();

  void setEnabled(Options &options, bool enabled);

  sigc::signal<void, bool> &signal_power_changed() {
    return m_signalPowerChanged;
  }

private:
  void threadMain();

private:
  std::string m_chipName;
  std::thread m_thread;
  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::optional<bool> m_pendingEnabled;
  bool m_running = false;
  bool m_stopRequested = false;

  sigc::signal<void, bool> m_signalPowerChanged;
};
