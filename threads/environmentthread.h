#pragma once

#include <atomic>
#include <glibmm/dispatcher.h>
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
  void processMainThreadDispatch();

  std::atomic<bool> m_running{false};
  std::thread m_thread;

  mutable std::mutex m_mutex;
  std::string m_lastError;
  Reading m_pendingReading{};
  bool m_hasPending{false};

  Glib::Dispatcher m_dispatcher;
  sigc::signal<void, Reading> m_signalEnvironmentChanged;
};
