#include "SynthVoice.h"

SynthVoice::SynthVoice() {}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound) {
    return dynamic_cast<juce::SynthesiserSound*>(sound) != nullptr;
}

void SynthVoice::prepareToPlay(double sampleRate, int samplesPerBlock, int outputChannels) {
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = outputChannels;
    
    // We *must* call `prepare` on each of the juce::dsp processors.
    osc1.prepare(spec);
    osc2.prepare(spec);
    masterGain.prepare(spec);
    filter.prepare(spec);
    
    adsr.setSampleRate(sampleRate);
    isPrepared = true;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound *sound, int currentPitchWheelPosition) {
    std::cout << "Note on! Velocity: " << velocity << std::endl;
    baseFreqHz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    osc1.setFrequency(baseFreqHz, true);
    osc1.setLevel(velocity * osc1MixRatio);
    osc2.setFrequency(baseFreqHz * pow(2, detuneSemitones / 12.0), true);
    osc2.setLevel(velocity * (1.0 - osc1MixRatio));
    
    adsr.noteOn();
    currentVelocity = velocity;
}

void SynthVoice::stopNote(float velocity, bool allowTailOff) {
    std::cout << "Note off!" << std::endl;
    adsr.noteOff();
    if (!allowTailOff || !adsr.isActive()) {
        clearCurrentNote();
    }
}

void SynthVoice::controllerMoved(int controllerNumber, int newControllerValue) {
    
}

void SynthVoice::pitchWheelMoved(int newPitchWheelValue) {
    
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample, int numSamples) {
    jassert(isPrepared);
    if (!isVoiceActive()) {
        return;
    }
    
    // 0. Clear and resize a temp buffer to put our processed signal into
    osc1Buffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    osc2Buffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    osc1Buffer.clear();
    osc2Buffer.clear();
    juce::dsp::AudioBlock<float> osc1AudioBlock {osc1Buffer};
    juce::dsp::AudioBlock<float> osc2AudioBlock {osc2Buffer};
    
    // 1. Get sounds from the oscillators and add them
    osc1.process(juce::dsp::ProcessContextReplacing<float> (osc1AudioBlock));
    osc2.process(juce::dsp::ProcessContextReplacing<float> (osc2AudioBlock));
    osc2AudioBlock += osc1AudioBlock;
    
    // Apply filter
    if (filterIsOn) {
        filter.process(juce::dsp::ProcessContextReplacing<float>(osc2AudioBlock));
    }
    
    // 2. Apply amplitude ADSR
    adsr.applyEnvelopeToBuffer(osc2Buffer, 0, osc2Buffer.getNumSamples());
    
    // 2. Apply master gain
    masterGain.process(juce::dsp::ProcessContextReplacing<float>(osc2AudioBlock));
    
    // 3. Add the current channel's audio data to the larger outputBuffer
    for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++) {
        outputBuffer.addFrom(channel, startSample, osc2Buffer, channel, 0, numSamples);
    }
    
    if (!adsr.isActive()) {
        clearCurrentNote();
    }
}

// OSC Module Setters

void SynthVoice::setOscWaveform(int waveformId, int oscNum) {
    if (oscNum == 1) {
        osc1.setWaveform(waveformId);
    } else {
        osc2.setWaveform(waveformId);
    }
}

void SynthVoice::setOscGainRatios(float val) {
    jassert(0 <= val && val <= 1);
    osc1MixRatio = val;
    osc1.setLevel(currentVelocity * osc1MixRatio);
    osc2.setLevel(currentVelocity * (1.0 - osc1MixRatio));
}

void SynthVoice::setOscDetune(int semitones, int cents) {
    detuneSemitones = semitones + ((float) cents / 100.0);
    osc2.setFrequency(baseFreqHz * pow(2, detuneSemitones / 12.0), true);
}

void SynthVoice::setOscVibratoDepth(float semitones) {
    // TODO: Implement
    return;
}

void SynthVoice::setOscSineLevel(float percent) {
    // TODO: Implement
    return;
}

// Filter Module Setters

void SynthVoice::setFilterType(int filterTypeIdx) {
    dsp::LadderFilterMode modeToAssign;
    switch (filterTypeIdx) {
        case 0:
            modeToAssign = dsp::LadderFilterMode::LPF12;
            break;
        case 1:
            modeToAssign = dsp::LadderFilterMode::HPF12;
            break;
        case 2:
            modeToAssign = dsp::LadderFilterMode::BPF12;
            break;
        case 3:
            modeToAssign = dsp::LadderFilterMode::LPF24;
            break;
        case 4:
            modeToAssign = dsp::LadderFilterMode::HPF24;
            break;
        case 5:
            modeToAssign = dsp::LadderFilterMode::BPF24;
            break;
        default:
            jassertfalse;
            break;
    }
    filter.setMode(modeToAssign);
}

void SynthVoice::setFilterParams(float cutoffHz, float resonance, float lfoAmt, float envAmt) {
    filter.setCutoffFrequencyHz(cutoffHz);
    filter.setResonance(resonance);
    // TODO: Implement LFO/ENV
}

void SynthVoice::setFilterOnOff(bool filterShouldBeOn) {
    filterIsOn = filterShouldBeOn;
}

// Amp Module Setters

void SynthVoice::setMasterGain(float gainDecibels) {
    masterGain.setGainDecibels(gainDecibels);
}

// Vibrato / LFO Module Setters

void SynthVoice::setFilterLFOParams(int lfoShapeId, float ampPercent, float rateHz) {
    // TODO: Implement
    return;
}

void SynthVoice::setVibratoParams(int lfoShapeId, float ampPercent, float rateHz) {
    // TODO: Implement
    return;
}

// Envelope Module Setters

void SynthVoice::setAmpADSR(float attack, float decay, float sustain, float release){
    adsrParams.attack = attack;
    adsrParams.decay = decay;
    adsrParams.sustain = sustain;
    adsrParams.release = release;
    adsr.setParameters(adsrParams);
}

void SynthVoice::setFilterADSR(float attack, float decay, float sustain, float release) {
    // TODO: Implement
    return;
}
