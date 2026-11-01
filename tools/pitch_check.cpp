// Offline pitch check (no JUCE). Renders one note with the Phase-1 plugin settings and
// writes the left channel as raw float32 to a file.
//   pitch_check <note> <body_ms> <sample_rate> <seconds> <out.raw> [gain_db] [lpf] [hpf] [pluck] [reverb_mix]
//   pluck=1: input level 1 and a single-sample impulse at t=0 (string excited, no noise build-up).
//   pluck=2: input level 1 and a 50 ms white-noise burst (amplitude 0.5) at t=0, like audio fed to the input.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include "FeedbackSynthEngine.h"

int main(int argc, char **argv) {
    if (argc < 6) { std::fprintf(stderr, "usage: pitch_check note body_ms sr seconds out.raw [gain_db lpf hpf]\n"); return 2; }
    const float note = (float) std::atof(argv[1]);
    const float body_s = (float) std::atof(argv[2]) * 0.001f;
    const float sr = (float) std::atof(argv[3]);
    const float seconds = (float) std::atof(argv[4]);
    const float gain_db = argc > 6 ? (float) std::atof(argv[6]) : -6.0f;
    const float lpf = argc > 7 ? (float) std::atof(argv[7]) : 8000.0f;
    const float hpf = argc > 8 ? (float) std::atof(argv[8]) : 250.0f;
    const int pluck = argc > 9 ? std::atoi(argv[9]) : 0;
    const float verb_mix = argc > 10 ? (float) std::atof(argv[10]) : 0.3f;

    auto e = std::make_unique<infrasonic::FeedbackSynth::Engine>();
    if (!e->Init(sr)) return 3;
    // Same fixed settings as PluginProcessor::prepareToPlay in Phase 1
    e->SetStringPitch(note);
    e->SetFeedbackGain(gain_db);
    e->SetFeedbackDelay(body_s);
    e->SetFeedbackLPFCutoff(lpf);
    e->SetFeedbackHPFCutoff(hpf);
    e->SetReverbMix(verb_mix);
    e->SetReverbFeedback(0.6f);
    e->SetOutputLevel(0.5f);
    e->SetInputLevel(pluck ? 1.0f : 0.0f);
    e->NoteOn();

    const long n = (long) (sr * seconds);
    unsigned rng = 2463534242u;
    std::vector<float> out((size_t) n);
    for (long i = 0; i < n; ++i) { float in = 0.0f;
        if (pluck == 1 && i == 0) in = 1.0f;
        if (pluck == 2 && i < (long) (0.05f * sr)) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; in = 0.5f * ((float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f); }
        float l, r; e->Process(in, l, r); out[(size_t) i] = l; }
    FILE *f = std::fopen(argv[5], "wb");
    if (!f) return 4;
    std::fwrite(out.data(), sizeof(float), out.size(), f);
    std::fclose(f);
    return 0;
}
