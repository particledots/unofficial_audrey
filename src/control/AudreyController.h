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

    void process(float in, float &outL, float &outR);

private:
    void tick();                       // smooth + apply, once per control block
    void apply(int id, bool force);    // push one parameter to the engine if it changed
    void applyPitch();

    std::unique_ptr<infrasonic::FeedbackSynth::Engine> engine_;
    bool ready_ = false;
    float sampleRate_ = 48000.0f;
    int counter_ = 0;
    int heldNote_ = -1;
    float pitchSent_ = -1000.0f;
    bool lfoWasOn_ = false;

    float target_[kNumParams] = {};
    float current_[kNumParams] = {};
    float sent_[kNumParams] = {};
    float coef_[kNumParams] = {};
};

} // namespace audrey
