// Headless engine test: no JUCE. Exit code 0 = pass.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "FeedbackSynthEngine.h"
#include "AudreyController.h"

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


// ---------------------------------------------------------------------------------------------
// Control layer (parameter table, smoothing, pitch) - what the plugin actually runs
// ---------------------------------------------------------------------------------------------
using namespace audrey;

static void test_param_table() {
    for (int i = 0; i < kNumParams; ++i) {
        const ParamInfo &p = paramInfo(i);
        CHECK(p.id && p.name && p.min < p.max, "param %d table entry invalid", i);
        CHECK(p.def >= p.min && p.def <= p.max, "param %s default outside range", p.id);
        for (int j = i + 1; j < kNumParams; ++j)
            CHECK(std::string(paramInfo(j).id) != p.id, "duplicate param id %s", p.id);
        if (p.toggle) continue;
        CHECK(std::fabs(toReal(i, 0.f) - p.min) <= 1e-5f * (1 + std::fabs(p.min)), "%s: n=0 is not min", p.id);
        CHECK(std::fabs(toReal(i, 1.f) - p.max) <= 1e-4f * (1 + std::fabs(p.max)), "%s: n=1 is not max", p.id);
        float prev = -1e30f;
        for (int k = 0; k <= 100; ++k) {
            const float n = k / 100.f, v = toReal(i, n);
            CHECK(v >= prev, "%s: not monotonic at n=%.2f", p.id, n);
            prev = v;
            CHECK(std::fabs(toNormalised(i, v) - n) < 2e-3f, "%s: round trip n=%.2f -> %g -> %g", p.id, n, v, toNormalised(i, v));
        }
        CHECK(std::fabs(toReal(i, toNormalised(i, p.def)) - p.def) <= 1e-3f * (1 + std::fabs(p.def)), "%s: default round trip", p.id);
    }
    // spot checks against Phase 0 ranges
    CHECK(paramInfo(kFeedbackGain).min == -60.f && paramInfo(kFeedbackGain).max == 12.f, "feedback gain range");
    CHECK(paramInfo(kBody).min == 0.001f && paramInfo(kBody).max == 0.1f, "body range");
    CHECK(paramInfo(kLpfCutoff).min == 100.f && paramInfo(kLpfCutoff).max == 18000.f, "LPF range");
    CHECK(paramInfo(kHpfCutoff).min == 10.f && paramInfo(kHpfCutoff).max == 4000.f, "HPF range");
    CHECK(paramInfo(kReverbDecay).min == 0.2f && paramInfo(kReverbDecay).max == 1.f, "reverb decay range");
    CHECK(paramInfo(kInputVolume).max == 5.f, "input volume range");
    CHECK(paramInfo(kLfoFrequency).min == 0.01f && paramInfo(kLfoFrequency).max == 7.f, "LFO range");
    CHECK(paramInfo(kEchoTime).min == 0.05f && paramInfo(kEchoTime).max == 5.f, "echo time range");
    CHECK(paramInfo(kEchoFeedback).max == 1.5f, "echo feedback range");
    CHECK(kDefaultNote == 40.f && kMinNote == 16.f && kMaxNote == 88.f, "note range");
    CHECK(std::fabs(toReal(kLfoFrequency, 0.5f) - std::sqrt(0.01f * 7.f)) < 1e-4f, "LFO frequency is not logarithmic");
    // firmware EXP mapping = min + n^2 * (max - min): body, output, input, echo send, echo time
    CHECK(std::fabs(toReal(kBody, 0.5f) - (0.001f + 0.25f * 0.099f)) < 1e-6f, "body is not a square curve");
    CHECK(std::fabs(toReal(kEchoTime, 0.5f) - (0.05f + 0.25f * 4.95f)) < 1e-5f, "echo time is not a square curve");
    CHECK(std::fabs(toReal(kOutputVolume, 0.5f) - 0.25f) < 1e-6f && std::fabs(toReal(kInputVolume, 0.5f) - 1.25f) < 1e-5f, "output/input curves");
    CHECK(std::fabs(toReal(kEchoSend, 0.5f) - 0.25f) < 1e-6f, "echo send curve");
    // firmware LOG mapping (cutoffs): min * (max/min)^n
    CHECK(std::fabs(toReal(kLpfCutoff, 0.5f) - std::sqrt(100.f * 18000.f)) < 0.5f, "LPF is not logarithmic");
}

static void test_smoothing_and_pitch() {
    const float sr = 48000.f;
    Controller c;
    CHECK(c.prepare(sr), "controller prepare");
    float l, r;

    // a jump in Body glides instead of stepping, and arrives
    c.setParam(kBody, 0.1f);
    for (int i = 0; i < 16 * 4; ++i) c.process(0.f, l, r);
    const float early = c.getSmoothed(kBody);
    CHECK(early > 0.001f && early < 0.05f, "body should be gliding after ~1.3 ms (got %g)", early);
    for (int i = 0; i < (int) sr; ++i) c.process(0.f, l, r);
    CHECK(std::fabs(c.getSmoothed(kBody) - 0.1f) < 1e-4f, "body did not arrive (%g)", c.getSmoothed(kBody));

    // toggles are immediate (within one control block)
    c.setParam(kDroneOn, 1.f);
    for (int i = 0; i < Controller::kControlBlock + 1; ++i) c.process(0.f, l, r);
    CHECK(c.getSmoothed(kDroneOn) == 1.f, "drone toggle not applied");

    // out-of-range, NaN and inf are ignored/clamped
    c.setParam(kFeedbackGain, 1000.f);  CHECK(c.getParam(kFeedbackGain) == 12.f, "gain not clamped");
    c.setParam(kFeedbackGain, std::nanf("")); CHECK(c.getParam(kFeedbackGain) == 12.f, "NaN accepted");
    c.setParamNormalised(kOutputVolume, 5.f); CHECK(c.getParam(kOutputVolume) == 1.f, "normalised not clamped");

    // pitch: held note + offset, clamped to 16..88; default note when nothing is held
    Controller p; p.prepare(sr);
    CHECK(p.getPitchNote() == kDefaultNote, "default pitch %g", p.getPitchNote());
    p.noteOn(60); CHECK(p.getPitchNote() == 60.f, "note 60");
    p.setParam(kPitchOffset, 7.f); for (int i = 0; i < 48000; ++i) p.process(0.f, l, r);
    CHECK(std::fabs(p.getPitchNote() - 67.f) < 0.01f, "offset +7 -> %g", p.getPitchNote());
    p.noteOn(100); CHECK(p.getPitchNote() == kMaxNote, "clamp high");
    p.setParam(kPitchOffset, -36.f); p.noteOn(10);
    for (int i = 0; i < 48000; ++i) p.process(0.f, l, r);
    CHECK(p.getPitchNote() == kMinNote, "clamp low (%g)", p.getPitchNote());
    p.noteOn(64); p.noteOn(65);
    const float held = p.getPitchNote();
    CHECK(std::fabs(held - 29.f) < 0.01f, "note 65 with offset -36 -> %g", held);
    p.noteOff(64);   // an older note released: pitch must not change
    CHECK(p.getPitchNote() == held, "older note-off changed pitch (%g -> %g)", held, p.getPitchNote());

    // pitch stays on the last note after release; an offset change then applies to that note
    Controller h; h.prepare(sr); h.noteOn(60); h.noteOff(60);
    CHECK(h.getPitchNote() == 60.f, "pitch moved on note-off (%g)", h.getPitchNote());
    h.setParam(kPitchOffset, 5.f); for (int i = 0; i < 48000; ++i) h.process(0.f, l, r);
    CHECK(std::fabs(h.getPitchNote() - 65.f) < 0.01f, "offset after release -> %g", h.getPitchNote());

    // sample-rate change keeps the parameter targets
    Controller q; q.prepare(48000.f); q.setParam(kFeedbackGain, -20.f); q.prepare(96000.f);
    CHECK(q.getParam(kFeedbackGain) == -20.f && q.getSmoothed(kFeedbackGain) == -20.f, "targets lost on re-prepare");
    CHECK(!q.prepare(1000.f) && !q.ready(), "bad rate should leave controller not ready");
    q.process(0.f, l, r); CHECK(l == 0.f && r == 0.f, "not-ready controller must be silent");
}

static void test_midi_cc() {
    const float sr = 48000.f;
    struct Row { int cc; int param; float at0, at127; };
    const Row rows[] = {
        {  7, kOutputVolume,  0.f,    1.f },
        { 12, kReverbDecay,   0.2f,   1.f },
        { 72, kEnvelopeShape, 0.f,    1.f },
        { 74, kLpfCutoff,     100.f,  18000.f },
        { 75, kFeedbackGain, -60.f,   12.f },
        { 76, kBody,          0.001f, 0.1f },
        { 77, kInputVolume,   0.f,    5.f },
        { 81, kHpfCutoff,     10.f,   4000.f },
        { 91, kReverbMix,     0.f,    1.f },
    };
    Controller c; c.prepare(sr);
    for (const Row &row : rows) {
        auto a = c.handleCC(row.cc, 0), b = c.handleCC(row.cc, 127);
        CHECK(a.handled && a.paramId == row.param && b.paramId == row.param, "CC %d maps to wrong parameter (%d)", row.cc, a.paramId);
        CHECK(std::fabs(a.real - row.at0) <= 1e-4f * (1 + std::fabs(row.at0)), "CC %d at 0 -> %g (want %g)", row.cc, a.real, row.at0);
        CHECK(std::fabs(b.real - row.at127) <= 1e-4f * (1 + std::fabs(row.at127)), "CC %d at 127 -> %g (want %g)", row.cc, b.real, row.at127);
        CHECK(c.getParam(row.param) == b.real, "CC %d did not move the parameter target", row.cc);
    }
    // curves through the CC: mid values follow each control's own curve (firmware: UpdateNormalized + fmap)
    const float n = 64.f / 127.f;
    CHECK(std::fabs(c.handleCC(7, 64).real - n * n) < 1e-5f, "CC7 curve");
    CHECK(std::fabs(c.handleCC(77, 64).real - 5.f * n * n) < 1e-4f, "CC77 curve");
    CHECK(std::fabs(c.handleCC(76, 64).real - (0.001f + n * n * 0.099f)) < 1e-6f, "CC76 curve");
    CHECK(std::fabs(c.handleCC(12, 64).real - (0.2f + 0.8f * std::expm1(-3.f * n) / std::expm1(-3.f))) < 1e-5f, "CC12 anti-exp curve");
    CHECK(std::fabs(c.handleCC(74, 64).real - 100.f * std::pow(180.f, n)) < 1.f, "CC74 log curve");

    // unmapped CCs and out-of-range values
    CHECK(!c.handleCC(1, 64).handled && !c.handleCC(120, 0).handled, "unmapped CC must not be handled");
    CHECK(c.handleCC(75, 500).real == 12.f && c.handleCC(75, -5).real == -60.f, "CC value not clamped");

    // CC 70: direct pitch, 16..88, no offset, until the next note-on or offset change
    Controller p; p.prepare(sr); float l, r;
    p.setParam(kPitchOffset, 7.f); for (int i = 0; i < 48000; ++i) p.process(0.f, l, r);
    auto r70 = p.handleCC(70, 127);
    CHECK(r70.handled && r70.paramId == -1, "CC70 result");
    CHECK(p.getPitchNote() == kMaxNote, "CC70=127 -> %g", p.getPitchNote());
    p.handleCC(70, 0); CHECK(p.getPitchNote() == kMinNote, "CC70=0 -> %g", p.getPitchNote());
    p.handleCC(70, 64); CHECK(std::fabs(p.getPitchNote() - (16.f + 72.f * n)) < 1e-4f, "CC70=64 -> %g", p.getPitchNote());
    for (int i = 0; i < 4800; ++i) p.process(0.f, l, r);
    CHECK(std::fabs(p.getPitchNote() - (16.f + 72.f * n)) < 1e-4f, "CC70 pitch must survive the control tick (%g)", p.getPitchNote());
    p.noteOn(50); CHECK(std::fabs(p.getPitchNote() - 57.f) < 0.01f, "note-on after CC70 -> %g", p.getPitchNote());
    p.handleCC(70, 127); p.setParam(kPitchOffset, 12.f); for (int i = 0; i < 48000; ++i) p.process(0.f, l, r);
    CHECK(std::fabs(p.getPitchNote() - 62.f) < 0.01f, "offset change after CC70 -> %g", p.getPitchNote());

    // CC 123: all notes off (a later note-off for the old note is then a no-op)
    p.noteOn(60); CHECK(p.getHeldNote() == 60, "held note");
    auto r123 = p.handleCC(123, 0);
    CHECK(r123.handled && r123.paramId == -1 && p.getHeldNote() == -1, "CC123 did not release the note");

    // CC 7 = 0 silences the output; the engine really follows the CC
    Controller q; q.prepare(sr); q.setParam(kFeedbackGain, 6.f); q.noteOn(48);
    double loud = 0, quiet = 0;
    for (int i = 0; i < 48000 * 3; ++i) { float a, b; q.process(0.f, a, b); if (i > 48000 * 2) loud = std::fmax(loud, std::fabs(a)); }
    q.handleCC(7, 0);
    for (int i = 0; i < 48000; ++i) { float a, b; q.process(0.f, a, b); if (i > 24000) quiet = std::fmax(quiet, std::fabs(a)); }
    std::printf("CC7 check: loud %.4f -> quiet %.6f\n", loud, quiet);
    CHECK(loud > 0.01 && quiet < 1e-4, "CC7=0 should mute (%.5f -> %.6f)", loud, quiet);
}

static unsigned rng_state = 12345u;
static float rnd() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return (rng_state >> 8) * (1.f / 16777216.f); }

static void test_automation_stress() {
    const float rates[] = {44100.f, 48000.f, 96000.f};
    for (float sr : rates) {
        Controller c; CHECK(c.prepare(sr), "prepare %.0f", sr);
        double peak = 0; long bad = 0;
        c.setParam(kInputVolume, 3.f);
        const long total = (long) (sr * 20.f);
        for (long i = 0; i < total; ++i) {
            if (i % (long) (sr * 0.007f) == 0) {                       // random automation of every parameter every ~7 ms
                for (int k = 0; k < kNumParams; ++k) {
                    const float n = (rnd() < 0.15f) ? (rnd() < 0.5f ? 0.f : 1.f) : rnd();   // include the extremes
                    c.setParamNormalised(k, n);
                }
            }
            if (i % (long) (sr * 0.25f) == 0) { if (rnd() < 0.7f) c.noteOn(16 + (int) (rnd() * 90)); else c.allNotesOff(); }
            float in = (i % 4800 < 5) ? 0.5f : 0.f, l, r;
            c.process(in, l, r);
            if (!std::isfinite(l) || !std::isfinite(r)) { ++bad; continue; }
            peak = std::fmax(peak, std::fmax(std::fabs(l), std::fabs(r)));
        }
        std::printf("automation stress %6.0f: peak %.4f nonfinite %ld\n", sr, peak, bad);
        CHECK(bad == 0, "non-finite output under automation at %.0f", sr);
        CHECK(peak <= 1.0, "peak > 1 under automation at %.0f (%.3f)", sr, peak);
    }
}

static void test_controller_determinism() {
    std::vector<float> a, b;
    for (int pass = 0; pass < 2; ++pass) {
        Controller c; c.prepare(48000.f);
        c.setParam(kEchoSend, 0.5f); c.setParam(kLfoOn, 1.f); c.setParam(kLfoFrequency, 3.f);
        c.noteOn(52);
        for (int i = 0; i < 48000; ++i) {
            if (i == 20000) c.setParam(kFeedbackGain, -3.f);
            float l, r; c.process(0.f, l, r); (pass ? b : a).push_back(l);
        }
    }
    CHECK(a == b, "controller output not deterministic");
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

    test_param_table();
    test_smoothing_and_pitch();
    test_midi_cc();
    test_automation_stress();
    test_controller_determinism();

    std::printf(failures ? "FAILED (%d)\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
