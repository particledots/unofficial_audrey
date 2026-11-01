#include "AudreyController.h"
#include <algorithm>
#include <cmath>

namespace audrey {

bool Controller::prepare(float sampleRate)
{
    ready_ = false;
    sampleRate_ = sampleRate;
    auto e = std::make_unique<infrasonic::FeedbackSynth::Engine>();
    if (!e->Init(sampleRate)) { engine_.reset(); return false; }
    engine_ = std::move(e);

    for (int i = 0; i < kNumParams; ++i) {
        const ParamInfo &p = paramInfo(i);
        // Targets survive a re-prepare (e.g. sample-rate change); the engine starts at the targets.
        current_[i] = target_[i];
        const float ticksPerSec = sampleRate / (float) kControlBlock;
        coef_[i] = p.smoothMs > 0.0f ? 1.0f - std::exp(-1.0f / (ticksPerSec * p.smoothMs * 0.001f)) : 1.0f;
    }
    heldNote_ = -1;
    baseNote_ = kDefaultNote;
    pitchOverride_ = -1.0f;
    pitchSent_ = -1000.0f;
    lfoWasOn_ = false;
    counter_ = 0;
    for (int i = 0; i < kNumParams; ++i) apply(i, true);
    applyPitch();
    ready_ = true;
    return true;
}

void Controller::setParam(int id, float v)
{
    if (id < 0 || id >= kNumParams || !std::isfinite(v)) return;
    const ParamInfo &p = paramInfo(id);
    float t = std::min(std::max(v, p.min), p.max);
    if (p.toggle) t = t >= 0.5f ? 1.0f : 0.0f;
    // Moving Frequency Offset hands pitch back to note + offset (the hardware recomputes pitch the same way).
    if (id == kPitchOffset && t != target_[id]) pitchOverride_ = -1.0f;
    target_[id] = t;
}

void Controller::setParamNormalised(int id, float n)
{
    if (id < 0 || id >= kNumParams || !std::isfinite(n)) return;
    setParam(id, toReal(id, n));
}

void Controller::noteOn(int note)
{
    if (!ready_) return;
    heldNote_ = note;
    baseNote_ = (float) note;
    pitchOverride_ = -1.0f;
    applyPitch();
    engine_->NoteOn();
}

void Controller::noteOff(int note)
{
    if (!ready_) return;
    if (note == heldNote_) {
        heldNote_ = -1;
        engine_->NoteOff();
    }
}

void Controller::allNotesOff()
{
    if (!ready_) return;
    heldNote_ = -1;
    engine_->NoteOff();
}

void Controller::applyPitch()
{
    if (!engine_) return;
    const float note = pitchOverride_ >= 0.0f
                           ? pitchOverride_
                           : std::min(std::max(baseNote_ + current_[kPitchOffset], kMinNote), kMaxNote);
    if (note != pitchSent_) {
        engine_->SetStringPitch(note);
        pitchSent_ = note;
    }
}

void Controller::apply(int id, bool force)
{
    const float v = current_[id];
    if (!force && v == sent_[id]) return;
    sent_[id] = v;
    auto &e = *engine_;
    switch (id) {
        case kPitchOffset:     applyPitch(); break;
        case kFeedbackGain:    e.SetFeedbackGain(v); break;
        case kBody:            if (current_[kLfoOn] < 0.5f) e.SetFeedbackDelay(v); break;   // the LFO drives the body while it is on
        case kLpfCutoff:       e.SetFeedbackLPFCutoff(v); break;
        case kHpfCutoff:       e.SetFeedbackHPFCutoff(v); break;
        case kReverbMix:       e.SetReverbMix(v); break;
        case kReverbDecay:     e.SetReverbFeedback(v); break;
        case kOutputVolume:    e.SetOutputLevel(v); break;
        case kInputVolume:     e.SetInputLevel(v); break;
        case kEnvelopeShape:   e.SetShape(v); break;
        case kLfoFrequency:    e.SetLFOFrequency(v); break;
        case kLfoDistribution: e.SetLFODistribution(v); break;
        case kLfoOn:           e.SetBodyLFOOn(v >= 0.5f); break;
        case kDroneOn:         e.DroneMode(v >= 0.5f); break;
        case kEchoSend:        e.SetEchoDelaySendAmount(v); break;
        case kEchoTime:        e.SetEchoDelayTime(v); break;
        case kEchoFeedback:    e.SetEchoDelayFeedback(v); break;
        default: break;
    }
}

Controller::CCResult Controller::handleCC(int cc, int value)
{
    CCResult r;
    const float n = clamp01((float) std::min(std::max(value, 0), 127) / 127.0f);
    int id = -1;
    switch (cc) {
        case 7:  id = kOutputVolume;  break;
        case 12: id = kReverbDecay;   break;
        case 72: id = kEnvelopeShape; break;
        case 74: id = kLpfCutoff;     break;
        case 75: id = kFeedbackGain;  break;
        case 76: id = kBody;          break;
        case 77: id = kInputVolume;   break;
        case 81: id = kHpfCutoff;     break;
        case 91: id = kReverbMix;     break;
        case 70:
            pitchOverride_ = kMinNote + n * (kMaxNote - kMinNote);
            applyPitch();
            r.handled = true;
            return r;
        case 123:
            allNotesOff();
            r.handled = true;
            return r;
        default:
            return r;
    }
    setParamNormalised(id, n);
    r.handled = true;
    r.paramId = id;
    r.real = target_[id];
    return r;
}

void Controller::tick()
{
    for (int i = 0; i < kNumParams; ++i) {
        const ParamInfo &p = paramInfo(i);
        if (p.toggle || coef_[i] >= 1.0f) {
            current_[i] = target_[i];
        } else if (p.curve == Curve::Log) {
            // glide in the log domain so a sweep is even in pitch / frequency terms
            const float lc = std::log(current_[i]), lt = std::log(target_[i]);
            const float d = lt - lc;
            current_[i] = std::fabs(d) < 1e-5f ? target_[i] : std::exp(lc + d * coef_[i]);
        } else {
            const float d = target_[i] - current_[i];
            const float span = p.max - p.min;
            current_[i] = std::fabs(d) < span * 1e-6f ? target_[i] : current_[i] + d * coef_[i];
        }
        apply(i, false);
    }
    // When the LFO is switched off the engine stops writing the body delay: put the Body value back.
    const bool lfoOn = current_[kLfoOn] >= 0.5f;
    if (lfoWasOn_ && !lfoOn) apply(kBody, true);
    lfoWasOn_ = lfoOn;
}

void Controller::process(float in, float &outL, float &outR)
{
    if (!ready_) { outL = outR = 0.0f; return; }
    if (counter_ <= 0) { tick(); counter_ = kControlBlock; }
    --counter_;
    engine_->Process(in, outL, outR);
}

} // namespace audrey
