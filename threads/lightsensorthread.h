#pragma once

#include "../drivers/gpio/gpiohelper.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <glibmm/dispatcher.h>
#include <mutex>
#include <sigc++/sigc++.h>
#include <thread>

class LightSensorThread {
public:
  LightSensorThread();
  ~LightSensorThread();

  void start();
  void stop();

  bool readOnce();

  sigc::signal<void(bool)> &signal_sensor_changed();

private:
  void threadLoop();
  void processMainThreadDispatch();
  void setBaselineLocked(bool state);

private:
  std::atomic<bool> m_running{false};
  std::thread m_thread;

  std::mutex m_mutex;
  std::condition_variable m_wake;
  GPIOHelper m_gpio;

  bool m_initialized{false};
  bool m_lastState{false};

  bool m_pendingState{false};
  bool m_hasPending{false};

  Glib::Dispatcher m_dispatcher;
  sigc::signal<void(bool)> m_signalSensorChanged;
};
