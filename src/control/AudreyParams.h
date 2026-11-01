// Unofficial Audrey - parameter table (JUCE-free, shared by the plugin and the headless tests).
//
// Every parameter has a "real" value (what the engine / the host displays) and a normalised
// 0..1 position (what a knob, a MIDI CC or host automation moves). The curve says how the two
// relate. Ranges come from Phase 0 (AudreyTouch registerParams + Audrey-II echo controls).
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <string>

namespace audrey {

enum ParamId : int {
    kPitchOffset = 0,   // semitones added to the held MIDI note
    kFeedbackGain,      // dBFS
    kBody,              // seconds (feedback delay)
    kLpfCutoff,         // Hz
    kHpfCutoff,         // Hz
    kReverbMix,         // 0..1
    kReverbDecay,       // 0.2..1 (engine reverb feedback)
    kOutputVolume,      // 0..1
    kInputVolume,       // 0..5
    kEnvelopeShape,     // 0..1
    kLfoFrequency,      // Hz
    kLfoDistribution,   // 0..1
    kLfoOn,             // toggle
    kDroneOn,           // toggle
    kEchoSend,          // 0..1
    kEchoTime,          // seconds
    kEchoFeedback,      // 0..1.5
    kBodyTune,          // toggle: snap Body to a whole number of periods of the played note (plugin addition)
    kNumParams
};

enum class Curve { Linear, Log, Square, AntiExp3 };

struct ParamInfo {
    const char *id;       // stable host identifier: never rename once released
    const char *name;     // name shown by the host
    const char *unit;     // "" if none
    float min, max, def;  // real units
    Curve curve;
    bool toggle;
    float smoothMs;       // 0 = no smoothing (applied immediately)
};

// Reference note range from Phase 0 ("Frequency note 16-88"); the held note + offset is clamped to it.
constexpr float kMinNote = 16.0f;
constexpr float kMaxNote = 88.0f;
constexpr float kDefaultNote = 40.0f;   // pitch used when no MIDI note is held (Phase 0 default)

// Curves: Square is DaisySP's Mapping::EXP (min + n*n*(max-min)); Log is Mapping::LOG; AntiExp3 is the
// firmware's ftension(n, -3) for Reverb Decay. Body, Output, Input, Echo Send and Echo Time are EXP in the
// AudreyTouch / Audrey-II firmware (verified in their registerParams). LFO Frequency is linear in the
// firmware; here it is Log because 0.01-7 Hz is too cramped at the slow end on a linear control.
//
// Defaults: Frequency (note 40), Echo Time (0.5 s), Echo Feedback (0), LFO (1 Hz, 0.5), Envelope (0),
// Output (0.5), HPF (250 Hz) are the firmware's registered defaults. The rest are playable plugin
// defaults (the firmware overwrites them from the knobs at boot): Feedback Gain -6 dB, LPF 8 kHz,
// Reverb 0.3 / decay 0.85, Input 0, as in Phase 1. Body is 1 ms (Phase 2 pitch check: at 10 ms the loop
// locks onto the body comb and the note is not heard at its own pitch).
inline const ParamInfo *paramTable()
{
    static const ParamInfo t[kNumParams] = {
        { "pitch_offset",   "Frequency Offset", "st",   -36.0f,  36.0f,   0.0f,   Curve::Linear,   false, 20.0f },
        { "fb_gain",        "Feedback Gain",    "dB",   -60.0f,  12.0f,  -6.0f,   Curve::Linear,   false, 30.0f },
        { "body",           "Body",             "s",      0.001f, 0.1f,   0.001f, Curve::Square,   false, 30.0f },
        { "fb_lpf",         "Feedback LPF",     "Hz",   100.0f, 18000.0f, 8000.0f, Curve::Log,     false, 30.0f },
        { "fb_hpf",         "Feedback HPF",     "Hz",    10.0f,  4000.0f,  250.0f, Curve::Log,     false, 30.0f },
        { "reverb_mix",     "Reverb Mix",       "",       0.0f,   1.0f,   0.3f,   Curve::Linear,   false, 30.0f },
        { "reverb_decay",   "Reverb Decay",     "",       0.2f,   1.0f,   0.85f,  Curve::AntiExp3, false, 50.0f },
        { "output_volume",  "Output Volume",    "",       0.0f,   1.0f,   0.5f,   Curve::Square,   false, 30.0f },
        { "input_volume",   "Input Volume",     "",       0.0f,   5.0f,   0.0f,   Curve::Square,   false, 30.0f },
        { "env_shape",      "Envelope Shape",   "",       0.0f,   1.0f,   0.0f,   Curve::Linear,   false, 30.0f },
        { "lfo_freq",       "LFO Frequency",    "Hz",     0.01f,  7.0f,   1.0f,   Curve::Log,      false, 30.0f },
        { "lfo_dist",       "LFO Distribution", "",       0.0f,   1.0f,   0.5f,   Curve::Linear,   false, 30.0f },
        { "lfo_on",         "LFO On",           "",       0.0f,   1.0f,   0.0f,   Curve::Linear,   true,   0.0f },
        { "drone_on",       "Drone On",         "",       0.0f,   1.0f,   0.0f,   Curve::Linear,   true,   0.0f },
        { "echo_send",      "Echo Send",        "",       0.0f,   1.0f,   0.0f,   Curve::Square,   false, 30.0f },
        { "echo_time",      "Echo Time",        "s",      0.05f,  5.0f,   0.5f,   Curve::Square,   false, 30.0f },
        { "echo_feedback",  "Echo Feedback",    "",       0.0f,   1.5f,   0.0f,   Curve::Linear,   false, 30.0f },
        { "body_tune",      "Body Tune",        "",       0.0f,   1.0f,   1.0f,   Curve::Linear,   true,   0.0f },
    };
    return t;
}

inline const ParamInfo &paramInfo(int id) { return paramTable()[id]; }

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// "Anti-exponential" shape used for Reverb Decay on the hardware (ftension with factor -3):
// rises quickly at the start of the knob travel.
inline float antiExp3(float n)  { return std::expm1(-3.0f * n) / std::expm1(-3.0f); }
inline float antiExp3Inv(float y)
{
    const float v = 1.0f - clamp01(y) * (1.0f - std::exp(-3.0f));
    return -std::log(v) / 3.0f;
}

// normalised 0..1 -> real units
inline float toReal(int id, float n)
{
    const ParamInfo &p = paramInfo(id);
    n = clamp01(n);
    if (p.toggle) return n >= 0.5f ? 1.0f : 0.0f;
    switch (p.curve) {
        case Curve::Linear:   return p.min + (p.max - p.min) * n;
        case Curve::Log:      return p.min * std::pow(p.max / p.min, n);
        case Curve::Square:   return p.min + (p.max - p.min) * n * n;
        case Curve::AntiExp3: return p.min + (p.max - p.min) * antiExp3(n);
    }
    return p.def;
}

// real units -> normalised 0..1
inline float toNormalised(int id, float v)
{
    const ParamInfo &p = paramInfo(id);
    if (p.toggle) return v >= 0.5f ? 1.0f : 0.0f;
    if (v < p.min) v = p.min;
    if (v > p.max) v = p.max;
    const float span = p.max - p.min;
    switch (p.curve) {
        case Curve::Linear:   return (v - p.min) / span;
        case Curve::Log:      return std::log(v / p.min) / std::log(p.max / p.min);
        case Curve::Square:   return std::sqrt((v - p.min) / span);
        case Curve::AntiExp3: return antiExp3Inv((v - p.min) / span);
    }
    return 0.0f;
}

// ---- text shown by the host and the editor, and parsing of typed values -----------------------------
// Examples: Body "1.0 ms", LPF "8.00 kHz" / "250 Hz", Echo Time "0.50 s", Feedback Gain "-6.0 dB".
inline std::string formatValue(int id, float v)
{
    char b[48];
    switch (id) {
        case kPitchOffset:   std::snprintf(b, sizeof b, "%+.1f st", v); break;
        case kFeedbackGain:  std::snprintf(b, sizeof b, "%.1f dB", v); break;
        case kBody: {
            const float ms = v * 1000.0f;
            std::snprintf(b, sizeof b, ms < 10.0f ? "%.2f ms" : "%.1f ms", ms);
            break;
        }
        case kLpfCutoff:
        case kHpfCutoff:
            if (v >= 1000.0f)     std::snprintf(b, sizeof b, "%.2f kHz", v / 1000.0f);
            else if (v >= 100.0f) std::snprintf(b, sizeof b, "%.0f Hz", v);
            else                  std::snprintf(b, sizeof b, "%.1f Hz", v);
            break;
        case kLfoFrequency:  std::snprintf(b, sizeof b, v < 1.0f ? "%.3f Hz" : "%.2f Hz", v); break;
        case kEchoTime:      std::snprintf(b, sizeof b, "%.2f s", v); break;
        case kLfoOn:
        case kDroneOn:
        case kBodyTune:      std::snprintf(b, sizeof b, "%s", v >= 0.5f ? "On" : "Off"); break;
        default:             std::snprintf(b, sizeof b, std::fabs(v) < 1.0f ? "%.3f" : "%.2f", v); break;
    }
    return b;
}

// Reads what a person types into a host field. Returns the real value (not clamped; the host clamps).
inline float parseValue(int id, const std::string &text)
{
    const ParamInfo &p = paramInfo(id);
    if (p.toggle) {
        std::string t;
        for (char ch : text) t += (char) std::tolower((unsigned char) ch);
        return (t.find("on") != std::string::npos || t.find('1') != std::string::npos || t.find("true") != std::string::npos) ? 1.0f : 0.0f;
    }
    const float n = (float) std::atof(text.c_str());
    const bool hasMs = text.find("ms") != std::string::npos || text.find("MS") != std::string::npos;
    const bool hasK  = text.find('k') != std::string::npos || text.find('K') != std::string::npos;
    switch (id) {
        case kBody:      return hasMs ? n / 1000.0f : (text.find('s') != std::string::npos ? n : n / 1000.0f);   // bare number = ms
        case kEchoTime:  return hasMs ? n / 1000.0f : n;                                                          // bare number = s
        case kLpfCutoff:
        case kHpfCutoff: return hasK ? n * 1000.0f : n;
        default:         return n;
    }
}

} // namespace audrey
