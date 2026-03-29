#include "doorbellthread.h"

#include <chrono>

DoorbellThread::DoorbellThread() {
  m_dispatcher.connect(
      sigc::mem_fun(*this, &DoorbellThread::processMainThreadDispatch));
}

DoorbellThread::~DoorbellThread() { stop(); }

void DoorbellThread::start() {
  if (m_running)
    return;

  m_running = true;
  m_thread = std::thread(&DoorbellThread::threadLoop, this);
}

void DoorbellThread::stop() {
  if (!m_running)
    return;

  m_running = false;

  if (m_thread.joinable())
    m_thread.join();
}

sigc::signal<void, bool> &DoorbellThread::signal_doorbell_changed() {
  return m_signalDoorbellChanged;
}

void DoorbellThread::threadLoop() {
  using namespace std::chrono;

  while (m_running) {
    bool state = m_gpio.read(PIN_DOORBELL, true);

    bool emitNow = false;

    {
      std::lock_guard<std::mutex> lock(m_mutex);

      if (!m_initialized) {
        m_lastState = state;
        m_initialized = true;
      } else if (state != m_lastState) {
        m_lastState = state;
        m_pendingState = state;
        m_hasPending = true;
        emitNow = true;
      }
    }

    if (emitNow) {
      m_dispatcher.emit();
    }

    std::this_thread::sleep_for(milliseconds(150));
  }
}

void DoorbellThread::processMainThreadDispatch() {
  bool state = false;
  bool hasPending = false;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    hasPending = m_hasPending;
    state = m_pendingState;
    m_hasPending = false;
  }

  if (hasPending) {
    m_signalDoorbellChanged.emit(state);
  }
}
