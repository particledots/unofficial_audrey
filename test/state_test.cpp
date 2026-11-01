// Plugin-level test (needs JUCE): parameters, state save/restore, MIDI CC -> host parameter.
// Built and run by CI on macOS. Exit code 0 = pass.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdio>
#include "PluginProcessor.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); ++failures; } } while (0)

using namespace audrey;

static float raw (AudreyProcessor& p, int id) { return p.apvts.getRawParameterValue (paramInfo (id).id)->load(); }

static void setNormalised (AudreyProcessor& p, int id, float n)
{
    p.apvts.getParameter (paramInfo (id).id)->setValueNotifyingHost (n);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;   // message manager (the CC -> host timer runs on it)

    // 1. Parameters exist, with the right defaults and ranges
    {
        AudreyProcessor p;
        CHECK (p.getParameters().size() == kNumParams, "parameter count %d", (int) p.getParameters().size());
        for (int i = 0; i < kNumParams; ++i)
        {
            auto* par = p.apvts.getParameter (paramInfo (i).id);
            CHECK (par != nullptr, "missing parameter %s", paramInfo (i).id);
            if (par == nullptr) continue;
            CHECK (std::abs (raw (p, i) - paramInfo (i).def) <= 1e-4f * (1.0f + std::abs (paramInfo (i).def)), "%s default %g", paramInfo (i).id, raw (p, i));
            CHECK (par->getName (64) == paramInfo (i).name, "%s name", paramInfo (i).id);
            // text shown by the host round-trips
            const float v = toReal (i, 0.37f);
            const auto text = par->getText (par->convertTo0to1 (v), 64);
            const float back = par->convertFrom0to1 (par->getValueForText (text));
            CHECK (std::abs (back - v) <= 0.01f * (std::abs (v) + 0.1f), "%s text '%s' -> %g (expected %g)", paramInfo (i).id, text.toRawUTF8(), back, v);
        }
        std::printf ("body default text: %s\n", p.apvts.getParameter ("body")->getText (p.apvts.getParameter ("body")->getValue(), 64).toRawUTF8());
    }

    // 2. State save and restore
    {
        AudreyProcessor a;
        for (int i = 0; i < kNumParams; ++i)
            setNormalised (a, i, paramInfo (i).toggle ? (float) (i % 2) : 0.13f + 0.047f * (float) i);
        juce::MemoryBlock state;
        a.getStateInformation (state);
        CHECK (state.getSize() > 100, "state suspiciously small (%d bytes)", (int) state.getSize());

        AudreyProcessor b;   // fresh, defaults
        b.setStateInformation (state.getData(), (int) state.getSize());
        int differing = 0;
        for (int i = 0; i < kNumParams; ++i)
        {
            const float x = raw (a, i), y = raw (b, i);
            if (std::abs (x - y) > 1e-5f * (1.0f + std::abs (x))) { ++differing; std::printf ("  %s: saved %g restored %g\n", paramInfo (i).id, x, y); }
        }
        CHECK (differing == 0, "%d parameters differ after restore", differing);
        std::printf ("state: %d bytes, %d of %d parameters restored\n", (int) state.getSize(), kNumParams - differing, kNumParams);

        // garbage and empty state must be ignored, not crash or reset values
        const float before = raw (b, kFeedbackGain);
        b.setStateInformation ("not xml", 7);
        b.setStateInformation (state.getData(), 0);
        CHECK (raw (b, kFeedbackGain) == before, "garbage state changed a parameter");

        // a second save of the restored plugin gives the same values (stable round trip)
        juce::MemoryBlock state2; b.getStateInformation (state2);
        AudreyProcessor c; c.setStateInformation (state2.getData(), (int) state2.getSize());
        for (int i = 0; i < kNumParams; ++i)
            CHECK (std::abs (raw (c, i) - raw (a, i)) <= 1e-5f * (1.0f + std::abs (raw (a, i))), "%s drifted over a second round trip", paramInfo (i).id);

        // restoring state before prepareToPlay must be what the audio engine starts with
        AudreyProcessor d;
        d.setStateInformation (state.getData(), (int) state.getSize());
        d.setRateAndBufferSizeDetails (48000.0, 512);
        d.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
        double energy = 0.0; bool finite = true;
        for (int blk = 0; blk < 200; ++blk)
        {
            buf.clear();
            d.processBlock (buf, midi);
            midi.clear();
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 512; ++s) { const float v = buf.getSample (ch, s); if (! std::isfinite (v)) finite = false; energy += (double) v * v; }
        }
        CHECK (finite, "non-finite audio after state restore");
        std::printf ("audio after restore: energy %g\n", energy);
    }

    // 3. MIDI CC moves the host parameter (through the message-thread timer)
    {
        AudreyProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 75, 127), 10);   // feedback gain -> +12 dB
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 0), 20);     // LPF -> 100 Hz
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 76, 127), 30);   // body -> 100 ms
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 99), 40);     // mod wheel: not in the map
        buf.clear();
        p.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);       // let the 30 Hz timer run
        CHECK (std::abs (raw (p, kFeedbackGain) - 12.0f) < 1e-3f, "CC75 -> feedback gain %g", raw (p, kFeedbackGain));
        CHECK (std::abs (raw (p, kLpfCutoff) - 100.0f) < 0.05f, "CC74 -> LPF %g", raw (p, kLpfCutoff));
        CHECK (std::abs (raw (p, kBody) - 0.1f) < 1e-5f, "CC76 -> body %g", raw (p, kBody));
        CHECK (std::abs (raw (p, kHpfCutoff) - paramInfo (kHpfCutoff).def) < 1e-3f, "unmapped CC moved a parameter");

        // the host parameter echo must not undo a later host change
        setNormalised (p, kFeedbackGain, 0.5f);
        buf.clear(); juce::MidiBuffer none; p.processBlock (buf, none);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        CHECK (std::abs (raw (p, kFeedbackGain) - (-24.0f)) < 1e-3f, "host change after CC lost (%g)", raw (p, kFeedbackGain));
        std::printf ("CC -> host: gain %g dB, LPF %g Hz, body %g s\n", raw (p, kFeedbackGain), raw (p, kLpfCutoff), raw (p, kBody));
    }

    std::printf (failures ? "FAILED (%d)\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
