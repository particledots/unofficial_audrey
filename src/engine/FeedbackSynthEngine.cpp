#include "FeedbackSynthEngine.h"
#include "DSPUtils.h"
#include "env.h"
#include <algorithm>
#include <cmath>

using namespace infrasonic;
using namespace infrasonic::FeedbackSynth;
using namespace daisysp;

bool Engine::Init(const float sample_rate) {
  using ED = EchoDelay<kMaxEchoDelaySamp>;

  ready_ = false;
  if (!(sample_rate >= kMinSampleRate && sample_rate <= kMaxSampleRate)) return false;

  _env.Init(sample_rate);
  _env.SetMode(synthux::Envelope::Mode::ASR);
  _env.SetShape(0.0f);

  lfo_.Init(sample_rate);
  lfo_.SetAmp(1.f);
  lfo_.SetWaveform(daisysp::Oscillator::WAVE_RAMP);
  lfo_.SetFreq(1.0f);

  echo_delay_[0] = std::make_unique<ED>();
  echo_delay_[1] = std::make_unique<ED>();
  verb_ = std::make_unique<ReverbSc>();

  sample_rate_ = sample_rate;
  fb_delay_samp_ = 1000.f;
  fb_delay_samp_target_ = 64.f;
  fb_delay_smooth_coef_ = onepole_coef(0.2f, sample_rate);
  fb_gain_ = 0.0f;
  echo_send_ = 0.0f;
  verb_mix_ = 0.0f;
  output_level_ = 0.5f;
  input_level_ = 1.0f;
  drone = false;

  noise_seed_ = 1;
  noise_amp_ = dbfs2lin(-90.0f);

  lfo_freq_ = 1.0f;
  lfo_on_ = false;
  lfo_prev_ = lfo_held_ = lfo_smoothed_ = 0.0f;
  rng_state_ = 0x9E3779B9u;
  SetLFODistribution(0.5f);

  for (unsigned int i = 0; i < 2; i++) {

    strings_[i].Init(sample_rate);
    strings_[i].SetBrightness(0.98f);
    strings_[i].SetFreq(mtof(40.0f));
    strings_[i].SetDamping(0.4f);

    fb_delayline_[i].Init();

    echo_delay_[i]->Init(sample_rate);
    echo_delay_[i]->SetDelayTime(0.5f, true);
    echo_delay_[i]->SetFeedback(0.0f);
    echo_delay_[i]->SetLagTime(0.5f);

    overdrive_[i].Init();
    overdrive_[i].SetDrive(0.4);

    limiter_[i].Init();
  }

  if (verb_->Init(sample_rate) != 0) return false;
  verb_->SetFeedback(0.85f);
  verb_->SetLpFreq(12000.0f);

  fb_lpf_.Init(sample_rate);
  fb_lpf_.SetQ(0.9f);
  fb_lpf_.SetCutoff(18000.0f);

  fb_hpf_.Init(sample_rate);
  fb_hpf_.SetQ(0.9f);
  fb_hpf_.SetCutoff(60.f);

  ready_ = true;
  return true;
}

float Engine::NextRandom01() {
  // xorshift32
  rng_state_ ^= rng_state_ << 13;
  rng_state_ ^= rng_state_ >> 17;
  rng_state_ ^= rng_state_ << 5;
  return static_cast<float>(rng_state_ >> 8) * (1.0f / 16777216.0f);
}

void Engine::SetStringPitch(const float nn) {
  const auto freq = mtof(nn);
  strings_[0].SetFreq(freq);
  strings_[1].SetFreq(freq);
}

void Engine::SetFeedbackGain(const float gain_db) {
  fb_gain_ = dbfs2lin(gain_db);
}

void Engine::SetFeedbackDelay(const float delay_s) {
  fb_delay_samp_target_ =
      DSY_CLAMP(delay_s * sample_rate_, 1.0f,
                static_cast<float>(kMaxFeedbackDelaySamp - 1));
}

void Engine::SetFeedbackLPFCutoff(const float cutoff_hz) {
  fb_lpf_.SetCutoff(cutoff_hz);
}

void Engine::SetFeedbackHPFCutoff(const float cutoff_hz) {
  fb_hpf_.SetCutoff(cutoff_hz);
}

void Engine::SetEchoDelayTime(const float echo_time) {
  echo_delay_[0]->SetDelayTime(echo_time);
  echo_delay_[1]->SetDelayTime(echo_time);
}

void Engine::SetEchoDelayFeedback(const float echo_fb) {
  echo_delay_[0]->SetFeedback(echo_fb);
  echo_delay_[1]->SetFeedback(echo_fb);
}

void Engine::SetEchoDelaySendAmount(const float echo_send) {
  echo_send_ = echo_send;
}

void Engine::SetReverbMix(const float mix)
{
  verb_mix_ = fclamp(mix, 0.0f, 1.0f);
}

void Engine::SetReverbFeedback(const float time)
{
  verb_->SetFeedback(time);
}

void Engine::SetOutputLevel(const float level)
{
  output_level_ = level;
}

void Engine::SetInputLevel(const float level)
{
  input_level_ = level;
}

void Engine::NoteOn()
{
  _env.Trigger();
}

void Engine::NoteOff()
{
  _env.Release();
}

void Engine::DroneMode(bool mode)
{
  drone = mode;
}

void Engine::SetShape(const float shape)
{
  _env.SetShape(shape);
}

void Engine::SetBodyLFOOn(const bool on)
{
  lfo_on_ = on;
}

void Engine::SetLFOFrequency(const float hz)
{
  lfo_freq_ = hz;
  lfo_.SetFreq(hz);
}

void Engine::SetLFODistribution(const float norm)
{
  const auto dev = .05f + (.07f * (1.f - norm));
  dist_lo_ = norm - dev;
  dist_hi_ = norm + dev;
}

void Engine::Process(float in, float &outL, float &outR) {
  if (!ready_) { outL = outR = 0.0f; return; }

  // --- Update audio-rate-smoothed control params ---

  fonepole(fb_delay_samp_, fb_delay_samp_target_, fb_delay_smooth_coef_);

  // --- Process LFO ---

  const auto curr_lfo = lfo_.Process();
  if ((lfo_prev_ < 0.f && curr_lfo >= 0.f) || (lfo_prev_ > 0.f && curr_lfo <= 0.f))
    lfo_held_ = dist_lo_ + (dist_hi_ - dist_lo_) * NextRandom01();
  const auto lfo_slew_rate = lfo_freq_ < 1.f ? .0001f : .08f;
  lfo_smoothed_ += lfo_slew_rate * (lfo_held_ - lfo_smoothed_);
  lfo_prev_ = curr_lfo;

  const auto mapped_val = std::clamp(lfo_smoothed_ * lfo_smoothed_ * (0.1f - 0.001f) + 0.001f, 0.f, 1.f);
  if (lfo_on_) SetFeedbackDelay(mapped_val);

  // --- Process Samples ---

  float inL, inR, sampL, sampR, echoL, echoR, verbL, verbR;
  noise_seed_ *= 16807u;
  const float noise_samp =
      static_cast<float>(static_cast<int32_t>(noise_seed_)) * 4.6566129e-010f * noise_amp_;
  auto env = _env.Process();

  // ---> Feedback Loop

  // Get noise + feedback output
  inL = fb_delayline_[0].Read(fb_delay_samp_) + noise_samp + (in * input_level_);
  inR = fb_delayline_[1].Read(daisysp::fmax(1.0f, fb_delay_samp_ - 4.f)) +
        noise_samp + (in * input_level_);

  // Process through KS resonator
  sampL = strings_[0].Process(inL);
  sampR = strings_[1].Process(inR);

  // Distort + Clip
  sampL = overdrive_[0].Process(sampL);
  sampR = overdrive_[1].Process(sampR);

  // Filter in feedback loop
  fb_lpf_.ProcessStereo(sampL, sampR);
  fb_hpf_.ProcessStereo(sampL, sampR);

  // ---> Reverb

  verb_->Process(sampL, sampR, &verbL, &verbR);

  sampL -= (sampL - verbL) * verb_mix_;
  sampR -= (sampR - verbR) * verb_mix_;

  // ---> Resonator feedback

  // Write back into delay with attenuation
  fb_delayline_[0].Write(sampL * (fb_gain_ * (drone ? 1.0f : env)));
  fb_delayline_[1].Write(sampR * (fb_gain_ * (drone ? 1.0f : env)));

  // ---> Echo Delay

  echoL = echo_delay_[0]->Process(sampL * echo_send_);
  echoR = echo_delay_[1]->Process(sampR * echo_send_);

  sampL = 0.5f * (sampL + echoL);
  sampR = 0.5f * (sampR + echoR);

  // ---> Output
  outL = sampL * output_level_;
  outR = sampR * output_level_;

  // Hardware main loop runs Limiter(pre_gain 0.7) on each output channel
  limiter_[0].ProcessBlock(&outL, 1, 0.7f);
  limiter_[1].ProcessBlock(&outR, 1, 0.7f);
}
