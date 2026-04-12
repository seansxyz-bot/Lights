#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <fftw3.h>
#include <pulse/pulseaudio.h>

struct LightShowTunables {
  std::atomic<float> agc_target{0.60f};
  std::atomic<float> white_agc{0.98f};
  std::atomic<float> black_agc{0.025f};
  std::atomic<float> agc_baseline_keep{0.985f};

  std::atomic<float> master_gain{0.65f};
  std::atomic<float> gamma{0.98f};
  std::atomic<float> saturation{1.00f};
  std::atomic<float> color_contrast{3.2f};
  std::atomic<float> log_gain{1.35f};
  std::atomic<float> soft_k{0.55f};

  std::atomic<float> band_gain_bass{1.05f};
  std::atomic<float> band_gain_mid{0.95f};
  std::atomic<float> band_gain_high{1.00f};

  std::atomic<float> drift_amount{0.12f};
  std::atomic<float> drift_speed_scale{1.00f};

  std::atomic<float> tilt_left_r{1.00f};
  std::atomic<float> tilt_left_g{0.97f};
  std::atomic<float> tilt_left_b{1.04f};
  std::atomic<float> tilt_right_r{0.97f};
  std::atomic<float> tilt_right_g{1.04f};
  std::atomic<float> tilt_right_b{1.00f};
};

class LightShow {
public:
  using Clock = std::chrono::steady_clock;

  explicit LightShow(size_t num_leds_total = 19,
                     const std::string &monitor_source =
                         "alsa_output.platform-soc_sound.pro-output-0.monitor");
  ~LightShow();

  bool start();
  void stop();
  bool isRunning() const { return running_.load(); }

  void setSource(const std::string &src);
  std::string getSource() const;

  void setMasterGain(float v) { cfg_.master_gain.store(v); }
  void setWhiteAGC(float v) { cfg_.white_agc.store(v); }
  void setBlackAGC(float v) { cfg_.black_agc.store(v); }
  void setAGCTarget(float v) { cfg_.agc_target.store(v); }
  void setAGCBaselineKeep(float v) { cfg_.agc_baseline_keep.store(v); }
  void setGamma(float v) { cfg_.gamma.store(v); }
  void setSaturation(float v) { cfg_.saturation.store(v); }
  void setContrast(float v) { cfg_.color_contrast.store(v); }
  void setLogGain(float v) { cfg_.log_gain.store(v); }
  void setSoftK(float v) { cfg_.soft_k.store(v); }
  void setDriftAmount(float v) { cfg_.drift_amount.store(v); }
  void setDriftSpeedScale(float v) { cfg_.drift_speed_scale.store(v); }
  void setBandGains(float bass, float mid, float high) {
    cfg_.band_gain_bass.store(bass);
    cfg_.band_gain_mid.store(mid);
    cfg_.band_gain_high.store(high);
  }

  LightShowTunables cfg_;

private:
  static constexpr uint32_t SR = 48000;
  static constexpr uint32_t NFFT = 1024;
  static constexpr uint32_t HOP = NFFT / 2;
  static constexpr uint32_t CHANS = 2;
  static constexpr float FPS_LED = 35.0f;

  struct Range {
    float lo;
    float hi;
  };

  static constexpr Range R_BASS{20.0f, 250.0f};
  static constexpr Range R_MID{250.0f, 2000.0f};
  static constexpr Range R_HIGH{2000.0f, 16000.0f};

  struct SideBands {
    float bass = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
    float base = 0.0f;
  };

  struct ChannelPersonality {
    float bass_w = 1.0f;
    float mid_w = 1.0f;
    float high_w = 1.0f;
    float gain = 1.0f;
    float bias = 0.0f;
    float phase = 0.0f;
    float drift_speed = 1.0f;
  };

  using RGB = std::array<uint8_t, 3>;
  using LedFrame = std::vector<RGB>;

  static float clamp01_(float x);
  static uint8_t to8_(float x);
  static size_t hzToBin_(float hz);
  static float bandEnergyHz_(const std::vector<float> &mag, Range r);

  void initFFTW_();
  void destroyFFTW_();
  void initPersonalities_();

  bool initPulse_();
  void destroyPulse_();

  static void contextStateCb_(pa_context *c, void *userdata);
  static void streamStateCb_(pa_stream *s, void *userdata);
  static void streamReadCb_(pa_stream *s, size_t nbytes, void *userdata);

  void processAudioBlock_(const int16_t *samples, size_t frame_count);
  void renderLoop_();

  SideBands analyzeMag_(const std::vector<float> &mag, bool is_left,
                        int agc_index);
  LedFrame buildFrame_(const SideBands &left, const SideBands &right,
                       double time_sec) const;
  RGB computeLed_(size_t led_index, const SideBands &side, bool is_left,
                  double time_sec) const;
  void printFrame_(const LedFrame &frame) const;

private:
  const size_t num_leds_;

  mutable std::mutex cfg_mtx_;
  std::string monitor_;

  std::mutex audio_mtx_;
  std::vector<float> ringL_;
  std::vector<float> ringR_;

  pa_threaded_mainloop *pa_ml_ = nullptr;
  pa_mainloop_api *pa_api_ = nullptr;
  pa_context *pa_ctx_ = nullptr;
  pa_stream *pa_stream_ = nullptr;

  std::thread render_;
  std::atomic<bool> running_{false};

  float *in_ = nullptr;
  fftwf_complex *out_ = nullptr;
  fftwf_plan plan_ = nullptr;
  std::vector<float> window_;

  std::vector<float> magL_, magR_;

  std::atomic<float> L_bass_{0.0f}, L_mid_{0.0f}, L_high_{0.0f}, L_base_{0.0f};
  std::atomic<float> R_bass_{0.0f}, R_mid_{0.0f}, R_high_{0.0f}, R_base_{0.0f};
  std::atomic<bool> have_frame_{false};

  float agc_med_ch_[2] = {1e-3f, 1e-3f};

  std::vector<ChannelPersonality> chan_r_;
  std::vector<ChannelPersonality> chan_g_;
  std::vector<ChannelPersonality> chan_b_;
};
