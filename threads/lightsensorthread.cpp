#include "lightsensorthread.h"

LightSensorThread::LightSensorThread() {
  m_dispatcher.connect(
      sigc::mem_fun(*this, &LightSensorThread::processMainThreadDispatch));
}

LightSensorThread::~LightSensorThread() { stop(); }

void LightSensorThread::start() {
  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true))
    return;

  m_thread = std::thread(&LightSensorThread::threadLoop, this);
}

void LightSensorThread::stop() {
  if (!m_running)
    return;

  m_running = false;
  m_wake.notify_all();

  if (m_thread.joinable())
    m_thread.join();
}

bool LightSensorThread::sensorReadingMeansLightsOn(bool rawValue) {
  return rawValue;
}

bool LightSensorThread::readOnce() {
  const bool sensorWantsLightsOn =
      sensorReadingMeansLightsOn(m_gpio.read(PIN_SENSOR, true));

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    setBaselineLocked(sensorWantsLightsOn);
  }

  return sensorWantsLightsOn;
}

sigc::signal<void(bool)> &LightSensorThread::signal_sensor_changed() {
  return m_signalSensorChanged;
}

void LightSensorThread::threadLoop() {
  using namespace std::chrono;

  while (m_running) {
    const bool sensorWantsLightsOn =
        sensorReadingMeansLightsOn(m_gpio.read(PIN_SENSOR, true));

    bool emitNow = false;

    {
      std::lock_guard<std::mutex> lock(m_mutex);

      if (!m_initialized) {
        setBaselineLocked(sensorWantsLightsOn);
      } else {
        m_lastState = sensorWantsLightsOn;
        m_pendingState = sensorWantsLightsOn;
        m_hasPending = true;
        emitNow = true;
      }
    }

    if (emitNow)
      m_dispatcher.emit();

    std::unique_lock<std::mutex> lock(m_mutex);
    m_wake.wait_for(lock, seconds(300), [this]() { return !m_running; });
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

void LightSensorThread::setBaselineLocked(bool state) {
  m_lastState = state;
  m_pendingState = state;
  m_hasPending = false;
  m_initialized = true;
}
