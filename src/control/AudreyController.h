// Unofficial Audrey - control layer between the host and the Audrey engine (JUCE-free).
//
// Holds the parameter targets, smooths them, tracks the held MIDI note, and applies everything
// to the engine every kControlBlock samples. The plugin and the headless tests both drive this
// class, so what the tests exercise is what the plugin runs.
#pragma once
#include <memory>
#include "AudreyParams.h"
#include "FeedbackSynthEngine.h"

namespace audrey {

class Controller {
public:
    static constexpr int kControlBlock = 16;   // samples between smoothed-parameter updates

    Controller()
    {
        for (int i = 0; i < kNumParams; ++i) target_[i] = current_[i] = sent_[i] = paramInfo(i).def;
    }

    // (Re)creates the engine. Returns false if the sample rate is unsupported (then Process is silent).
    // All parameters jump to their current targets (no glide on start).
    bool prepare(float sampleRate);
    bool ready() const { return ready_; }

    // Parameters. Real units / normalised 0..1. Targets are smoothed towards (toggles are immediate).
    void setParam(int id, float realValue);
    void setParamNormalised(int id, float n);
    float getParam(int id) const { return target_[id]; }          // target, real units
    float getParamNormalised(int id) const { return toNormalised(id, target_[id]); }
    float getSmoothed(int id) const { return current_[id]; }      // value the engine currently has
    float getPitchNote() const { return pitchSent_; }              // note number last sent to the string (after offset and clamp)

    // Notes (monophonic, last note wins; releasing an older note does nothing).
    void noteOn(int note);
    void noteOff(int note);
    void allNotesOff();

    // MIDI control change (Touch map: 7 output, 12 reverb decay, 70 frequency, 72 envelope shape,
    // 74 LPF, 75 feedback gain, 76 body, 77 input, 81 HPF, 91 reverb mix, 123 all notes off).
    // A CC moves the same parameter as the host control. Values 0..127 cover the parameter's full range
    // through its own curve, as on the hardware. CC 70 sets the string pitch directly (16..88, no offset)
    // until the next note-on or Frequency Offset change; it has no host parameter.
    struct CCResult {
        bool handled = false;   // CC is part of the map
        int paramId = -1;       // parameter moved (-1 for CC 70 and CC 123)
        float real = 0.0f;      // that parameter's new target, real units
    };
    CCResult handleCC(int cc, int value);

    int getHeldNote() const { return heldNote_; }

    // Body delay (seconds) the engine is using now. With Body Tune on this is a whole number of periods of the
    // string pitch, nearest to the Body setting and kept inside the Body range (1-100 ms).
    float getEffectiveBody() const { return effectiveBody(); }

    void process(float in, float &outL, float &outR);

private:
    void tick();                       // smooth + apply, once per control block
    void apply(int id, bool force);    // push one parameter to the engine if it changed
    void applyPitch();
    void applyBody(bool force);
    float effectiveBody() const;

    std::unique_ptr<infrasonic::FeedbackSynth::Engine> engine_;
    bool ready_ = false;
    float sampleRate_ = 48000.0f;
    int counter_ = 0;
    int heldNote_ = -1;                // note currently held (for note-off matching)
    float baseNote_ = kDefaultNote;    // last note played; pitch stays on it after release, as on the hardware
    float pitchSent_ = -1000.0f;
    bool lfoWasOn_ = false;
    float pitchOverride_ = -1.0f;      // >= 0: CC 70 pitch in use
    float bodySent_ = -1.0f;

    float target_[kNumParams] = {};
    float current_[kNumParams] = {};
    float sent_[kNumParams] = {};
    float coef_[kNumParams] = {};
};

} // namespace audrey
