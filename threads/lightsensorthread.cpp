#include "lightsensorthread.h"

LightSensorThread::LightSensorThread() {
  m_dispatcher.connect(
      sigc::mem_fun(*this, &LightSensorThread::processMainThreadDispatch));
}

LightSensorThread::~LightSensorThread() { stop(); }

void LightSensorThread::start() {
  if (m_running)
    return;

  m_running = true;
  m_thread = std::thread(&LightSensorThread::threadLoop, this);
}

void LightSensorThread::stop() {
  if (!m_running)
    return;

  m_running = false;

  if (m_thread.joinable())
    m_thread.join();
}

bool LightSensorThread::readOnce() { return m_gpio.read(PIN_SENSOR, true); }

sigc::signal<void(bool)> &LightSensorThread::signal_sensor_changed() {
  return m_signalSensorChanged;
}

void LightSensorThread::threadLoop() {
  using namespace std::chrono;

  while (m_running) {
    const bool sensorWantsLightsOn = m_gpio.read(PIN_SENSOR, true);

    bool emitNow = false;

    {
      std::lock_guard<std::mutex> lock(m_mutex);

      if (!m_initialized) {
        m_lastState = sensorWantsLightsOn;
        m_initialized = true;
      } else if (sensorWantsLightsOn != m_lastState) {
        m_lastState = sensorWantsLightsOn;
        m_pendingState = sensorWantsLightsOn;
        m_hasPending = true;
        emitNow = true;
      }
    }

    if (emitNow)
      m_dispatcher.emit();

    for (int i = 0; i < 300 && m_running; ++i) {
      std::this_thread::sleep_for(seconds(1));
    }
  }
}

void LightSensorThread::processMainThreadDispatch() {
  bool state = false;
  bool hasPending = false;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    hasPending = m_hasPending;
    state = m_pendingState;
    m_hasPending = false;
  }

  if (hasPending)
    m_signalSensorChanged.emit(state);
}
