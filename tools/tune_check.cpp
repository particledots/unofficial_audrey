// Offline end-to-end check of Body Tune through the real control layer (no JUCE).
//   tune_check <note> <tune 0|1> <body_ms> <gain_db> <hpf_hz> <excitation 0|2> <out.raw>
// Plugin defaults except the arguments. excitation 2 = 50 ms white-noise burst (0.5) at the input.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "AudreyController.h"

int main(int argc, char **argv) {
    if (argc < 8) { std::fprintf(stderr, "usage: tune_check note tune body_ms gain_db hpf excitation out.raw\n"); return 2; }
    using namespace audrey;
    const float sr = 48000.0f;
    Controller c;
    c.setParam(kBody, (float) std::atof(argv[3]) * 0.001f);
    c.setParam(kBodyTune, (float) std::atof(argv[2]));
    c.setParam(kFeedbackGain, (float) std::atof(argv[4]));
    c.setParam(kHpfCutoff, (float) std::atof(argv[5]));
    const int excitation = std::atoi(argv[6]);
    if (excitation == 2) c.setParam(kInputVolume, 1.0f);
    if (!c.prepare(sr)) return 3;
    c.noteOn(std::atoi(argv[1]));
    const long n = (long) (sr * 4.0f);
    std::vector<float> out((size_t) n);
    unsigned rng = 2463534242u;
    for (long i = 0; i < n; ++i) {
        float in = 0.0f;
        if (excitation == 2 && i < (long) (0.05f * sr)) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; in = 0.5f * ((float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f); }
        float l, r; c.process(in, l, r); out[(size_t) i] = l;
    }
    FILE *f = std::fopen(argv[7], "wb");
    if (!f) return 4;
    std::fwrite(out.data(), sizeof(float), out.size(), f);
    std::fclose(f);
    return 0;
}
