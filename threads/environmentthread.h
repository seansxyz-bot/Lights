#pragma once

#include <atomic>
#include <mutex>
#include <sigc++/sigc++.h>
#include <string>
#include <thread>

class EnvironmentThread {
public:
  struct Reading {
    float temperatureC{0.0f};
    float temperatureF{0.0f};
    float humidity{0.0f};
    float pressureHPa{0.0f};
    float altitudeM{0.0f};
  };

  EnvironmentThread();
  ~EnvironmentThread();

  void start();
  void stop();
  bool isRunning() const;

  std::string lastError() const;

  sigc::signal<void, Reading> &signal_environment_changed();

private:
  void run();
  bool readSensor(Reading &reading);

  std::atomic<bool> m_running{false};
  std::thread m_thread;

  mutable std::mutex m_mutex;
  std::string m_lastError;

  sigc::signal<void, Reading> m_signalEnvironmentChanged;
};
