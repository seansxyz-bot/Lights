#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include <gtkmm.h>

#include "../drivers/gpio/gpiohelper.h"

class DoorbellThread {
public:
  DoorbellThread();
  ~DoorbellThread();

  void start();
  void stop();

  sigc::signal<void, bool> &signal_doorbell_changed();

private:
  void threadLoop();
  void processMainThreadDispatch();

private:
  std::atomic<bool> m_running{false};
  std::thread m_thread;
  std::mutex m_mutex;

  GPIOHelper m_gpio;
  bool m_initialized{false};
  bool m_lastState{false};
  bool m_pendingState{false};
  bool m_hasPending{false};

  Glib::Dispatcher m_dispatcher;
  sigc::signal<void, bool> m_signalDoorbellChanged;
};
