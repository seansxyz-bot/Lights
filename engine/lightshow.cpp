#include "lightshow.h"

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
}

LightShow::LightShow(size_t num_leds_total, const std::string &monitor_source)
    : num_leds_(num_leds_total), monitor_(monitor_source) {
  initPersonalities_();
}

LightShow::~LightShow() { stop(); }

bool LightShow::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true))
    return true;

  have_frame_.store(false, std::memory_order_release);
  ringL_.clear();
  ringR_.clear();
  ringL_.reserve(NFFT * 4);
  ringR_.reserve(NFFT * 4);

  initFFTW_();

  if (!initPulse_()) {
    destroyFFTW_();
    running_.store(false);
    return false;
  }

  render_ = std::thread(&LightShow::renderLoop_, this);
  return true;
}

void LightShow::stop() {
  if (!running_.exchange(false))
    return;

  destroyPulse_();

  if (render_.joinable())
    render_.join();

  destroyFFTW_();

  std::cerr << "[LightShow] stopped cleanly\n";
}

void LightShow::setSource(const std::string &src) {
  std::lock_guard<std::mutex> lk(cfg_mtx_);
  monitor_ = src;
}

std::string LightShow::getSource() const {
  std::lock_guard<std::mutex> lk(cfg_mtx_);
  return monitor_;
}

float LightShow::clamp01_(float x) {
  return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

uint8_t LightShow::to8_(float x) {
  return static_cast<uint8_t>(std::lround(clamp01_(x) * 255.0f));
}

size_t LightShow::hzToBin_(float hz) {
  float bin = hz * static_cast<float>(NFFT) / static_cast<float>(SR);
  if (bin < 0.0f)
    return 0;
  size_t b = static_cast<size_t>(std::lround(bin));
  return std::min(b, static_cast<size_t>(NFFT / 2));
}

float LightShow::bandEnergyHz_(const std::vector<float> &mag, Range r) {
  size_t b0 = hzToBin_(r.lo);
  size_t b1 = hzToBin_(r.hi);
  if (b1 <= b0)
    b1 = b0 + 1;
  float sum = 0.0f;
  for (size_t k = b0; k < b1; ++k)
    sum += mag[k];
  return sum / static_cast<float>(b1 - b0);
}

void LightShow::initFFTW_() {
  in_ = static_cast<float *>(fftwf_malloc(sizeof(float) * NFFT));
  out_ = static_cast<fftwf_complex *>(
      fftwf_malloc(sizeof(fftwf_complex) * (NFFT / 2 + 1)));
  plan_ = fftwf_plan_dft_r2c_1d(NFFT, in_, out_, FFTW_ESTIMATE);

  window_.resize(NFFT);
  for (size_t i = 0; i < NFFT; ++i) {
    window_[i] = 0.5f - 0.5f * std::cos((2.0f * static_cast<float>(M_PI) *
                                         static_cast<float>(i)) /
                                        static_cast<float>(NFFT - 1));
  }

  magL_.assign(NFFT / 2 + 1, 0.0f);
  magR_.assign(NFFT / 2 + 1, 0.0f);
}

void LightShow::destroyFFTW_() {
  if (plan_) {
    fftwf_destroy_plan(plan_);
    plan_ = nullptr;
  }
  if (out_) {
    fftwf_free(out_);
    out_ = nullptr;
  }
  if (in_) {
    fftwf_free(in_);
    in_ = nullptr;
  }
}

void LightShow::initPersonalities_() {
  std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> gain_dist(0.92f, 1.08f);
  std::uniform_real_distribution<float> bias_dist(-0.035f, 0.035f);
  std::uniform_real_distribution<float> phase_dist(0.0f, kTwoPi);
  std::uniform_real_distribution<float> speed_dist(0.55f, 1.45f);
  std::uniform_real_distribution<float> wobble_dist(-0.08f, 0.08f);

  chan_r_.resize(num_leds_);
  chan_g_.resize(num_leds_);
  chan_b_.resize(num_leds_);

  for (size_t i = 0; i < num_leds_; ++i) {
    chan_r_[i] = {1.25f + wobble_dist(rng), 0.80f + wobble_dist(rng),
                  0.55f + wobble_dist(rng), gain_dist(rng),
                  bias_dist(rng),           phase_dist(rng),
                  speed_dist(rng)};

    chan_g_[i] = {0.70f + wobble_dist(rng), 1.20f + wobble_dist(rng),
                  0.75f + wobble_dist(rng), gain_dist(rng),
                  bias_dist(rng),           phase_dist(rng),
                  speed_dist(rng)};

    chan_b_[i] = {0.45f + wobble_dist(rng), 0.80f + wobble_dist(rng),
                  1.28f + wobble_dist(rng), gain_dist(rng),
                  bias_dist(rng),           phase_dist(rng),
                  speed_dist(rng)};
  }
}

bool LightShow::initPulse_() {
  std::string device;
  {
    std::lock_guard<std::mutex> lk(cfg_mtx_);
    device = monitor_;
  }

  pa_ml_ = pa_threaded_mainloop_new();
  if (!pa_ml_) {
    std::cerr << "[LightShow] pa_threaded_mainloop_new failed'\n";
    return false;
  }

  pa_api_ = pa_threaded_mainloop_get_api(pa_ml_);
  if (!pa_api_) {
    std::cerr << "[LightShow] get_api failed\n";
    destroyPulse_();
    return false;
  }

  pa_ctx_ = pa_context_new(pa_api_, "Lights-LightShow");
  if (!pa_ctx_) {
    std::cerr << "[LightShow] pa_context_new failed\n";
    destroyPulse_();
    return false;
  }

  pa_context_set_state_callback(pa_ctx_, &LightShow::contextStateCb_, this);

  if (pa_threaded_mainloop_start(pa_ml_) < 0) {
    std::cerr << "[LightShow] pa_threaded_mainloop_start failed/n";
    destroyPulse_();
    return false;
  }

  pa_threaded_mainloop_lock(pa_ml_);

  if (pa_context_connect(pa_ctx_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
    std::cerr << "[LightShow] pa_context_connect failed: "
              << pa_strerror(pa_context_errno(pa_ctx_)) << '\n';
    pa_threaded_mainloop_unlock(pa_ml_);
    destroyPulse_();
    return false;
  }

  for (;;) {
    pa_context_state_t st = pa_context_get_state(pa_ctx_);
    if (st == PA_CONTEXT_READY)
      break;
    if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED) {
      std::cerr << "[LightShow] context failed during connect\n";
      pa_threaded_mainloop_unlock(pa_ml_);
      destroyPulse_();
      return false;
    }
    pa_threaded_mainloop_wait(pa_ml_);
  }

  pa_sample_spec ss{};
  ss.format = PA_SAMPLE_S16LE;
  ss.rate = SR;
  ss.channels = CHANS;

  pa_buffer_attr attr{};
  attr.maxlength = static_cast<uint32_t>(-1);
  attr.tlength = static_cast<uint32_t>(-1);
  attr.prebuf = static_cast<uint32_t>(-1);
  attr.minreq = static_cast<uint32_t>(-1);
  attr.fragsize = HOP * CHANS * sizeof(int16_t);

  pa_stream_ = pa_stream_new(pa_ctx_, "capture", &ss, nullptr);
  if (!pa_stream_) {
    std::cerr << "[LightShow] pa_stream_new failed: "
              << pa_strerror(pa_context_errno(pa_ctx_)) << '\n';
    pa_threaded_mainloop_unlock(pa_ml_);
    destroyPulse_();
    return false;
  }

  pa_stream_set_state_callback(pa_stream_, &LightShow::streamStateCb_, this);
  pa_stream_set_read_callback(pa_stream_, &LightShow::streamReadCb_, this);

  if (pa_stream_connect_record(
          pa_stream_, device.empty() ? nullptr : device.c_str(), &attr,
          static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY |
                                         PA_STREAM_AUTO_TIMING_UPDATE |
                                         PA_STREAM_INTERPOLATE_TIMING)) < 0) {
    std::cerr << "[LightShow] pa_stream_connect_record failed: "
              << pa_strerror(pa_context_errno(pa_ctx_)) << '\n';
    pa_threaded_mainloop_unlock(pa_ml_);
    destroyPulse_();
    return false;
  }

  for (;;) {
    pa_stream_state_t st = pa_stream_get_state(pa_stream_);
    if (st == PA_STREAM_READY)
      break;
    if (st == PA_STREAM_FAILED || st == PA_STREAM_TERMINATED) {
      std::cerr << "[LightShow] stream failed during connect\n";
      pa_threaded_mainloop_unlock(pa_ml_);
      destroyPulse_();
      return false;
    }
    pa_threaded_mainloop_wait(pa_ml_);
  }

  pa_threaded_mainloop_unlock(pa_ml_);

  std::cerr << "[LightShow] capture '" << device << "' @" << ss.rate << "Hz\n";
  return true;
}

void LightShow::destroyPulse_() {
  if (!pa_ml_)
    return;

  pa_threaded_mainloop_lock(pa_ml_);

  if (pa_stream_) {
    pa_operation *op = pa_stream_cork(pa_stream_, 1, nullptr, nullptr);
    if (op)
      pa_operation_unref(op);

    pa_stream_set_read_callback(pa_stream_, nullptr, nullptr);
    pa_stream_set_state_callback(pa_stream_, nullptr, nullptr);
    pa_stream_disconnect(pa_stream_);
    pa_stream_unref(pa_stream_);
    pa_stream_ = nullptr;
  }

  if (pa_ctx_) {
    pa_context_set_state_callback(pa_ctx_, nullptr, nullptr);
    pa_context_disconnect(pa_ctx_);
    pa_context_unref(pa_ctx_);
    pa_ctx_ = nullptr;
  }

  pa_threaded_mainloop_unlock(pa_ml_);

  pa_threaded_mainloop_stop(pa_ml_);
  pa_threaded_mainloop_free(pa_ml_);
  pa_ml_ = nullptr;
  pa_api_ = nullptr;
}

void LightShow::contextStateCb_(pa_context *, void *userdata) {
  auto *self = static_cast<LightShow *>(userdata);
  if (self && self->pa_ml_)
    pa_threaded_mainloop_signal(self->pa_ml_, 0);
}

void LightShow::streamStateCb_(pa_stream *, void *userdata) {
  auto *self = static_cast<LightShow *>(userdata);
  if (self && self->pa_ml_)
    pa_threaded_mainloop_signal(self->pa_ml_, 0);
}

void LightShow::streamReadCb_(pa_stream *s, size_t, void *userdata) {
  auto *self = static_cast<LightShow *>(userdata);
  if (!self || !self->running_.load())
    return;

  const void *data = nullptr;
  size_t nbytes = 0;

  while (pa_stream_readable_size(s) > 0) {
    if (pa_stream_peek(s, &data, &nbytes) < 0)
      return;

    if (!data || nbytes == 0) {
      pa_stream_drop(s);
      continue;
    }

    const auto *samples = static_cast<const int16_t *>(data);
    const size_t sample_count = nbytes / sizeof(int16_t);
    const size_t frame_count = sample_count / CHANS;

    self->processAudioBlock_(samples, frame_count);

    pa_stream_drop(s);
  }
}

void LightShow::processAudioBlock_(const int16_t *samples, size_t frame_count) {
  if (!samples || frame_count == 0)
    return;

  std::lock_guard<std::mutex> lk(audio_mtx_);

  for (size_t i = 0; i < frame_count; ++i) {
    ringL_.push_back(static_cast<float>(samples[2 * i + 0]));
    ringR_.push_back(static_cast<float>(samples[2 * i + 1]));
  }

  while (ringL_.size() >= NFFT && ringR_.size() >= NFFT) {
    for (size_t i = 0; i < NFFT; ++i)
      in_[i] = ringL_[i] * window_[i];
    fftwf_execute(plan_);
    for (size_t k = 0; k <= NFFT / 2; ++k) {
      const float re = out_[k][0];
      const float im = out_[k][1];
      magL_[k] = std::sqrt(re * re + im * im);
    }

    for (size_t i = 0; i < NFFT; ++i)
      in_[i] = ringR_[i] * window_[i];
    fftwf_execute(plan_);
    for (size_t k = 0; k <= NFFT / 2; ++k) {
      const float re = out_[k][0];
      const float im = out_[k][1];
      magR_[k] = std::sqrt(re * re + im * im);
    }

    ringL_.erase(ringL_.begin(), ringL_.begin() + HOP);
    ringR_.erase(ringR_.begin(), ringR_.begin() + HOP);

    SideBands left = analyzeMag_(magL_, true, 0);
    SideBands right = analyzeMag_(magR_, false, 1);

    L_bass_.store(left.bass, std::memory_order_relaxed);
    L_mid_.store(left.mid, std::memory_order_relaxed);
    L_high_.store(left.high, std::memory_order_relaxed);
    L_base_.store(left.base, std::memory_order_relaxed);

    R_bass_.store(right.bass, std::memory_order_relaxed);
    R_mid_.store(right.mid, std::memory_order_relaxed);
    R_high_.store(right.high, std::memory_order_relaxed);
    R_base_.store(right.base, std::memory_order_relaxed);

    have_frame_.store(true, std::memory_order_release);
  }
}

LightShow::SideBands LightShow::analyzeMag_(const std::vector<float> &mag,
                                            bool is_left, int agc_index) {
  SideBands s;

  s.bass =
      std::max(0.0f, bandEnergyHz_(mag, R_BASS) * cfg_.band_gain_bass.load());
  s.mid = std::max(0.0f, bandEnergyHz_(mag, R_MID) * cfg_.band_gain_mid.load());
  s.high =
      std::max(0.0f, bandEnergyHz_(mag, R_HIGH) * cfg_.band_gain_high.load());

  if (is_left) {
    s.bass *= cfg_.tilt_left_r.load();
    s.mid *= cfg_.tilt_left_g.load();
    s.high *= cfg_.tilt_left_b.load();
  } else {
    s.bass *= cfg_.tilt_right_r.load();
    s.mid *= cfg_.tilt_right_g.load();
    s.high *= cfg_.tilt_right_b.load();
  }

  double acc = 0.0;
  for (float v : mag)
    acc += static_cast<double>(v) * static_cast<double>(v);
  const float rms =
      static_cast<float>(std::sqrt(acc / static_cast<double>(NFFT)));

  const float keep = cfg_.agc_baseline_keep.load();
  agc_med_ch_[agc_index] = keep * agc_med_ch_[agc_index] + (1.0f - keep) * rms;

  const float agc_target = cfg_.agc_target.load();
  s.base = (agc_med_ch_[agc_index] > 1e-6f)
               ? (agc_target * (rms / agc_med_ch_[agc_index]))
               : agc_target;

  return s;
}

LightShow::RGB LightShow::computeLed_(size_t led_index, const SideBands &side,
                                      bool /*is_left*/, double time_sec) const {
  const float black_t = cfg_.black_agc.load();
  const float white_t = cfg_.white_agc.load();

  if (side.base <= black_t)
    return {0, 0, 0};
  if (side.base >= white_t)
    return {255, 255, 255};

  float zone_t = (side.base - black_t) / std::max(1e-6f, (white_t - black_t));
  zone_t = clamp01_(zone_t);

  const auto apply_log = [this](float x) {
    const float lg = cfg_.log_gain.load();
    float t = lg * std::max(0.0f, x);
    if (t < -0.999f)
      t = -0.999f;
    return std::log1p(t);
  };

  const float bass = apply_log(side.bass);
  const float mid = apply_log(side.mid);
  const float high = apply_log(side.high);

  const ChannelPersonality &pr = chan_r_[led_index];
  const ChannelPersonality &pg = chan_g_[led_index];
  const ChannelPersonality &pb = chan_b_[led_index];

  float r = bass * pr.bass_w + mid * pr.mid_w + high * pr.high_w;
  float g = bass * pg.bass_w + mid * pg.mid_w + high * pg.high_w;
  float b = bass * pb.bass_w + mid * pb.mid_w + high * pb.high_w;

  r = std::max(0.0f, r);
  g = std::max(0.0f, g);
  b = std::max(0.0f, b);

  const float sum = r + g + b + 1e-6f;
  float wr = r / sum;
  float wg = g / sum;
  float wb = b / sum;

  const float contrast = std::max(0.5f, cfg_.color_contrast.load());
  wr = std::pow(wr, contrast);
  wg = std::pow(wg, contrast);
  wb = std::pow(wb, contrast);
  const float wsum = wr + wg + wb + 1e-6f;
  wr /= wsum;
  wg /= wsum;
  wb /= wsum;

  const float k = cfg_.soft_k.load();
  const float base_soft = side.base / (1.0f + k * std::fabs(side.base));

  r = wr * base_soft * pr.gain + pr.bias;
  g = wg * base_soft * pg.gain + pg.bias;
  b = wb * base_soft * pb.gain + pb.bias;

  const float drift_edge_fade = std::sin(zone_t * static_cast<float>(M_PI));
  const float drift_amount = cfg_.drift_amount.load() * drift_edge_fade;
  const float drift_speed_scale = cfg_.drift_speed_scale.load();

  r += drift_amount * std::sin(static_cast<float>(time_sec) * pr.drift_speed *
                                   drift_speed_scale +
                               pr.phase);
  g += drift_amount * std::sin(static_cast<float>(time_sec) * pg.drift_speed *
                                   drift_speed_scale +
                               pg.phase);
  b += drift_amount * std::sin(static_cast<float>(time_sec) * pb.drift_speed *
                                   drift_speed_scale +
                               pb.phase);

  const float sat = std::clamp(cfg_.saturation.load(), 0.0f, 1.0f);
  const float gray = (r + g + b) / 3.0f;
  r = gray + (r - gray) * sat;
  g = gray + (g - gray) * sat;
  b = gray + (b - gray) * sat;

  const float master = std::max(0.0f, cfg_.master_gain.load());
  r *= master;
  g *= master;
  b *= master;

  const float gamma = std::max(0.01f, cfg_.gamma.load());
  r = std::pow(clamp01_(r), gamma);
  g = std::pow(clamp01_(g), gamma);
  b = std::pow(clamp01_(b), gamma);

  return {to8_(r), to8_(g), to8_(b)};
}

LightShow::LedFrame LightShow::buildFrame_(const SideBands &left,
                                           const SideBands &right,
                                           double time_sec) const {
  LedFrame frame(num_leds_);
  for (size_t i = 0; i < num_leds_; ++i) {
    const bool is_left = (i & 1u) == 1u;
    frame[i] = computeLed_(i, is_left ? left : right, is_left, time_sec);
  }
  return frame;
}

void LightShow::printFrame_(const LedFrame &frame) const {
  std::cout << '[';
  for (size_t i = 0; i < frame.size(); ++i) {
    const auto &rgb = frame[i];
    std::cout << '{' << static_cast<int>(rgb[0]) << '-'
              << static_cast<int>(rgb[1]) << '-' << static_cast<int>(rgb[2])
              << '}';
    if (i + 1 != frame.size())
      std::cout << ' ';
  }
  std::cout << "]\n";
}

void LightShow::setFrameSender(FrameSender sender) {
  std::lock_guard<std::mutex> lk(frame_sender_mtx_);
  frame_sender_ = std::move(sender);
}

void LightShow::renderLoop_() {
  const auto tick = std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(1.0 / FPS_LED));
  auto next = Clock::now();
  const auto start_time = Clock::now();

  while (running_.load()) {
    const auto now = Clock::now();
    if (now < next) {
      std::this_thread::sleep_for(next - now);
      continue;
    }
    next += tick;

    if (!have_frame_.load(std::memory_order_acquire))
      continue;

    SideBands left;
    SideBands right;
    left.bass = L_bass_.load(std::memory_order_relaxed);
    left.mid = L_mid_.load(std::memory_order_relaxed);
    left.high = L_high_.load(std::memory_order_relaxed);
    left.base = L_base_.load(std::memory_order_relaxed);

    right.bass = R_bass_.load(std::memory_order_relaxed);
    right.mid = R_mid_.load(std::memory_order_relaxed);
    right.high = R_high_.load(std::memory_order_relaxed);
    right.base = R_base_.load(std::memory_order_relaxed);

    const double t = std::chrono::duration<double>(now - start_time).count();
    LedFrame frame = buildFrame_(left, right, t);

    FrameSender sender;
    {
      std::lock_guard<std::mutex> lk(frame_sender_mtx_);
      sender = frame_sender_;
    }

    if (sender) {
      sender(frame);
    } else {
      printFrame_(frame);
    }
  }

  std::cerr << "[LightShow] render loop stopped\n";
}
