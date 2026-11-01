// Offline pitch check (no JUCE). Renders one note with the Phase-1 plugin settings and
// writes the left channel as raw float32 to a file.
//   pitch_check <note> <body_ms> <sample_rate> <seconds> <out.raw> [gain_db] [lpf] [hpf] [pluck] [reverb_mix]
//   pluck=1: input level 1 and a single-sample impulse at t=0 (string excited, no noise build-up).
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
    std::vector<float> out((size_t) n);
    for (long i = 0; i < n; ++i) { float l, r; e->Process((pluck && i == 0) ? 1.0f : 0.0f, l, r); out[(size_t) i] = l; }
    FILE *f = std::fopen(argv[5], "wb");
    if (!f) return 4;
    std::fwrite(out.data(), sizeof(float), out.size(), f);
    std::fclose(f);
    return 0;
}
