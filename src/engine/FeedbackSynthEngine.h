// Unofficial Audrey - feedback synth engine (plugin port)
//
// Derived from the Audrey / AudreyTouch / Audrey-II firmware engine:
//   (c) 2022 Infrasonic Audio, LLC (MIT)        - original engine by Nick Donaldson
//   (c) 2026 Synthux Academy (MIT)              - Audrey-II / Simple Touch changes
// Plugin-port changes (see PORTING-NOTES.md): no Daisy SDRAM allocation, LFO state
// per instance, own PRNG (no std::random_device), sample rates up to 192 kHz,
// output limiter from the hardware main loop added.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "daisysp.h"
#include "BiquadFilters.h"
#include "EchoDelay.h"
#include "KarplusString.h"
#include "env.h"

namespace infrasonic {
namespace FeedbackSynth {

class Engine {

    public:
        static constexpr float kMinSampleRate = 8000.0f;
        static constexpr float kMaxSampleRate = 192000.0f;

        Engine() = default;
        ~Engine() = default;

        // Returns false (and leaves the engine unusable) if the sample rate is
        // outside [kMinSampleRate, kMaxSampleRate] or the reverb cannot be set up.
        bool Init(const float sample_rate);

        void SetStringPitch(const float nn);

        void SetFeedbackGain(const float gain_dbfs);

        void SetFeedbackDelay(const float delay_s);
        void SetFeedbackLPFCutoff(const float cutoff_hz);
        void SetFeedbackHPFCutoff(const float cutoff_hz);

        void SetEchoDelayTime(const float echo_time);
        void SetEchoDelayFeedback(const float echo_fb);
        void SetEchoDelaySendAmount(const float echo_send);

        // Both range 0-1
        void SetReverbMix(const float mix);
        void SetReverbFeedback(const float time);

        void SetOutputLevel(const float level);
        void SetInputLevel(const float level);

        void NoteOn();
        void NoteOff();
        void SetShape(const float shape);
        void DroneMode(bool mode);

        void SetBodyLFOOn(const bool);
        void SetLFOFrequency(const float);
        void SetLFODistribution(const float);

        void Process(float in, float &outL, float &outR);

    private:

        // 250 ms at the maximum supported sample rate (192 kHz)
        static constexpr size_t kMaxFeedbackDelaySamp = 48000;
        // 5 s at the maximum supported sample rate (192 kHz)
        static constexpr size_t kMaxEchoDelaySamp = 192000 * 5;

        float NextRandom01();

        bool ready_ = false;
        float sample_rate_ = 48000.0f;
        float fb_gain_ = 0.0f;
        float echo_send_ = 0.0f;
        float verb_mix_ = 0.0f;
        float output_level_ = 0.5f;
        float input_level_ = 1.0f;
        bool drone = false;

        float fb_delay_smooth_coef_ = 1.0f;
        float fb_delay_samp_ = 1000.f;
        float fb_delay_samp_target_ = 64.f;

        infrasonic::KarplusString strings_[2];
        daisysp::DelayLine<float, kMaxFeedbackDelaySamp> fb_delayline_[2];
        daisysp::Overdrive overdrive_[2];
        daisysp::Limiter limiter_[2];
        synthux::Envelope _env;

        // Noise source: same generator as DaisySP WhiteNoise (x * 16807 mod 2^32),
        // written with unsigned arithmetic so it is well defined.
        uint32_t noise_seed_ = 1;
        float noise_amp_ = 0.0f;

        LPF12 fb_lpf_;
        HPF12 fb_hpf_;

        daisysp::Oscillator lfo_;
        uint32_t rng_state_ = 0x9E3779B9u;
        float dist_lo_ = 0.415f;
        float dist_hi_ = 0.585f;
        float lfo_freq_ = 1.0f;
        bool lfo_on_ = false;
        float lfo_prev_ = 0.0f;
        float lfo_held_ = 0.0f;
        float lfo_smoothed_ = 0.0f;

        using VerbPtr = std::unique_ptr<daisysp::ReverbSc>;
        VerbPtr verb_;

        using EchoDelayPtr = std::unique_ptr<EchoDelay<kMaxEchoDelaySamp>>;
        EchoDelayPtr echo_delay_[2];

        Engine(const Engine &other) = delete;
        Engine(Engine &&other) = delete;
        Engine& operator=(const Engine &other) = delete;
        Engine& operator=(Engine &&other) = delete;
};

}
}
