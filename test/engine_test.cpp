// Headless engine test: no JUCE. Exit code 0 = pass.
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
#include "FeedbackSynthEngine.h"

using infrasonic::FeedbackSynth::Engine;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); ++failures; } } while (0)

static void setup(Engine &e) {
    e.SetStringPitch(40.f);
    e.SetFeedbackGain(-6.f);
    e.SetFeedbackDelay(0.01f);
    e.SetFeedbackLPFCutoff(8000.f);
    e.SetFeedbackHPFCutoff(250.f);
    e.SetReverbMix(0.3f);
    e.SetReverbFeedback(0.6f);
    e.SetOutputLevel(0.5f);
    e.SetInputLevel(0.f);
    e.SetEchoDelayTime(0.4f);
    e.SetEchoDelayFeedback(0.5f);
    e.SetEchoDelaySendAmount(0.3f);
    e.NoteOn();
}

struct Stats { double peak = 0, rms = 0; long nan = 0; };

static Stats render(Engine &e, float sr, float seconds, std::vector<float> *outL = nullptr) {
    Stats s; double acc = 0; long n = (long)(sr * seconds);
    for (long i = 0; i < n; ++i) {
        float l, r; e.Process(0.f, l, r);
        if (!std::isfinite(l) || !std::isfinite(r)) { ++s.nan; continue; }
        s.peak = std::fmax(s.peak, std::fmax(std::fabs(l), std::fabs(r)));
        acc += (double)l * l + (double)r * r;
        if (outL) outL->push_back(l);
    }
    s.rms = std::sqrt(acc / (2.0 * n));
    return s;
}

int main() {
    const float rates[] = {44100.f, 48000.f, 88200.f, 96000.f, 192000.f};
    for (float sr : rates) {
        auto e = std::make_unique<Engine>();
        CHECK(e->Init(sr), "Init failed at %.0f", sr);
        setup(*e);
        Stats s = render(*e, sr, 10.f);
        std::printf("sr %6.0f: peak %.4f rms %.5f nan %ld\n", sr, s.peak, s.rms, s.nan);
        CHECK(s.nan == 0, "NaN/Inf at %.0f", sr);
        CHECK(s.peak <= 1.0, "peak > 1 at %.0f (%.3f)", sr, s.peak);
        CHECK(s.rms > 1e-4, "silent output at %.0f", sr);
    }

    // Extreme settings must stay finite and bounded (limiter at end of chain)
    {
        auto e = std::make_unique<Engine>();
        CHECK(e->Init(48000.f), "Init");
        setup(*e);
        e->SetFeedbackGain(12.f);
        e->SetEchoDelayFeedback(1.5f);
        e->SetEchoDelaySendAmount(1.f);
        e->SetInputLevel(5.f);
        e->SetOutputLevel(1.f);
        e->SetReverbFeedback(1.f);
        e->DroneMode(true);
        e->SetBodyLFOOn(true);
        e->SetLFOFrequency(7.f);
        Stats s = render(*e, 48000.f, 20.f);
        std::printf("extreme: peak %.4f rms %.5f nan %ld\n", s.peak, s.rms, s.nan);
        CHECK(s.nan == 0, "NaN under extreme settings");
        CHECK(s.peak <= 1.0, "peak > 1 under extreme settings");
    }

    // Two instances are independent and deterministic: instance A alone == A while B runs
    {
        std::vector<float> solo, shared;
        { auto a = std::make_unique<Engine>(); a->Init(48000.f); setup(*a); a->SetBodyLFOOn(true); a->SetLFOFrequency(3.f); render(*a, 48000.f, 2.f, &solo); }
        {
            auto a = std::make_unique<Engine>(); auto b = std::make_unique<Engine>();
            a->Init(48000.f); b->Init(48000.f); setup(*a); setup(*b);
            a->SetBodyLFOOn(true); a->SetLFOFrequency(3.f);
            b->SetBodyLFOOn(true); b->SetLFOFrequency(5.f); b->SetStringPitch(52.f);
            for (long i = 0; i < 96000; ++i) { float l, r, l2, r2; a->Process(0.f, l, r); b->Process(0.f, l2, r2); shared.push_back(l); }
        }
        CHECK(solo.size() == shared.size(), "size mismatch");
        double maxdiff = 0; for (size_t i = 0; i < solo.size() && i < shared.size(); ++i) maxdiff = std::fmax(maxdiff, std::fabs(solo[i] - shared[i]));
        std::printf("two-instance max diff: %g\n", maxdiff);
        CHECK(maxdiff == 0.0, "instances interfere (max diff %g)", maxdiff);
    }

    // Unsupported sample rates are refused, and a refused engine is silent
    {
        Engine e; float l = 1, r = 1;
        CHECK(!e.Init(384000.f), "384 kHz should be refused");
        e.Process(0.f, l, r);
        CHECK(l == 0.f && r == 0.f, "refused engine must be silent");
    }

    std::printf(failures ? "FAILED (%d)\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
