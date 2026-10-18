#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Presets d'usine — reglages notes comme sur la pedale (0 a 10)
//==============================================================================
static const wiga::PresetData factoryPresets[] =
{
    //  nom                   speed depth delay regen mode krazy
    { "Classic Chorus",        3.0f, 5.0f, 4.0f, 1.0f, 0, 0 },
    { "Lush Stereo Chorus",    2.2f, 7.0f, 7.0f, 2.0f, 0, 0 },
    { "Jet Flanger",           2.0f, 8.0f, 2.0f, 7.5f, 1, 0 },
    { "Metallic Flange",       4.5f, 3.0f, 0.5f, 9.0f, 1, 0 },
    { "Slow Deep Sweep",       1.0f, 10.f, 3.0f, 6.0f, 1, 0 },
    { "Wack'd Vibrato",        4.0f, 4.0f, 3.0f, 0.0f, 0, 1 },
    { "Korny Chirps",          7.0f, 6.0f, 2.0f, 7.0f, 1, 1 },
    { "Ring Mod Madness",      9.5f, 7.0f, 5.0f, 3.0f, 0, 1 },
    // Intro de « My Way » (Limp Bizkit, 2000), reglages releves par l'utilisateur :
    // Speed 95,6 % · Depth 1,6 % · Delay 73,5 % · Regen 12,3 % · Chorus · Wack'd
    { "My Road",               9.56f, 0.16f, 7.35f, 1.23f, 0, 1 },
};

int WackdAudioProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }

const wiga::PresetData& WackdAudioProcessor::getFactoryPreset (int index)
{
    return factoryPresets[(size_t) juce::jlimit (0, getNumFactoryPresets() - 1, index)];
}

//==============================================================================
WackdAudioProcessor::WackdAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WACKD", createLayout())
{
    pSpeed  = apvts.getRawParameterValue ("speed");
    pDepth  = apvts.getRawParameterValue ("depth");
    pDelay  = apvts.getRawParameterValue ("delay");
    pRegen  = apvts.getRawParameterValue ("regen");
    pMode   = apvts.getRawParameterValue ("mode");
    pKrazy  = apvts.getRawParameterValue ("krazy");
    pActive = apvts.getRawParameterValue ("active");
    pStereo = apvts.getRawParameterValue ("stereo");
}

juce::AudioProcessorValueTreeState::ParameterLayout WackdAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto knob = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return String (v, 1); }));
    };

    p.push_back (knob ("speed", "Speed",      3.0f));
    p.push_back (knob ("depth", "Depth",      5.0f));
    p.push_back (knob ("delay", "Delay Time", 4.0f));
    p.push_back (knob ("regen", "Regen.",     0.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 },  "Mode",
                                                         StringArray { "Chorus", "Flanger" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "krazy", 1 }, "Krazy",
                                                         StringArray { "Normal", "Wack'd" }, 0));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "active", 1 }, "Footswitch", true));
    // Sortie : 0 = stereo compatible mono (par defaut, utilisable avant un simulateur
    // d'ampli mono), 1 = stereo CF7 d'origine (OUT2 = direct - effet, a placer apres).
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "stereo", 1 }, "Output",
                                                         StringArray { "Mono compatible", "CF7 stereo (INV)" }, 0));

    return { p.begin(), p.end() };
}

bool WackdAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void WackdAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    // 60 ms de memoire largement suffisants (retard maxi ~ 28 ms)
    int size = 1;
    while (size < (int) (sr * 0.06) + 8)
        size <<= 1;

    delayLine.assign ((size_t) size, 0.0f);
    delayMask  = size - 1;
    writeIndex = 0;
    lfoPhase   = 0.0;
    feedback   = 0.0f;
    fbDcX = fbDcY = 0.0f;
    fbLP = 0.0f;

    smoothCoef = (float) (1.0 - std::exp (-1.0 / (0.030 * sr))); // ~30 ms
    activeCoef = (float) (1.0 - std::exp (-1.0 / (0.010 * sr))); // ~10 ms

    smActive = pActive->load() > 0.5f ? 1.0f : 0.0f;

    for (auto* f : { &preFilterA, &preFilterB, &postFilterA, &postFilterB, &postFilterC, &postFilterD })
    {
        f->reset();
        f->setLowpass (sr, 9000.0);
    }
    smWet = pKrazy->load() > 0.5f ? 1.0f : 0.80f;
    smDry = pKrazy->load() > 0.5f ? 0.0f : 0.70f;
    smStereo = pStereo->load() > 0.5f ? 1.0f : 0.0f;
}

//==============================================================================
void WackdAudioProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void WackdAudioProcessor::handleMidi (juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();

        if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const int v  = m.getControllerValue();
            const float knobValue = (float) v / 127.0f * 10.0f;

            bool known = true;
            switch (cc)
            {
                case ccSpeed:      setParam ("speed", knobValue);           break;
                case ccDepth:      setParam ("depth", knobValue);           break;
                case ccDelayTime:  setParam ("delay", knobValue);           break;
                case ccRegen:      setParam ("regen", knobValue);           break;
                case ccMode:       setParam ("mode",  v >= 64 ? 1.0f : 0.0f);  break;
                case ccKrazy:      setParam ("krazy", v >= 64 ? 1.0f : 0.0f);  break;
                case ccFootswitch: setParam ("active", v >= 64 ? 1.0f : 0.0f); break;
                case ccFootToggle:
                    if (v >= 64 && prevToggleValue < 64)
                        setParam ("active", pActive->load() > 0.5f ? 0.0f : 1.0f);
                    prevToggleValue = v;
                    break;
                default: known = false; break;
            }

            if (known && cc <= ccRegen)
                currentPreset = -1;

            lastMidiCC    = cc;
            lastMidiValue = v;
            ++midiCounter;
        }
        else if (m.isProgramChange())
        {
            const int prog = m.getProgramChangeNumber();
            if (prog < getNumFactoryPresets())
                applyFactoryPreset (prog);

            lastMidiProgram = prog;
            lastMidiCC      = -2; // indique un Program Change
            ++midiCounter;
        }
    }

    midi.clear();
}

//==============================================================================
// Traitement audio (v1.5) — modele d'apres le schema de la CF7
//
//  - LFO IC5 (integrateur + trigger de Schmitt) : triangle. SPEED = VR1 500 kC (log)
//    + R56 5 k -> plage ~1:100. Le switch MODE (SW1) commute les condensateurs de
//    temporisation : plages differentes en chorus et en flanger.
//  - KRAZY (SW2) injecte une modulation fixe via R44 47 k (independante de DEPTH),
//    accelere le LFO, coupe le signal direct et assombrit l'effet.
//  - DELAY TIME (VR3 10 kB + R69/R70) fixe l'horloge de la M50198 = retard de base.
//  - Sommateurs IC3/IC2 a 47 k : direct et effet a poids egal ; OUT2 inverse l'effet.
//  - Chorus : triangle arrondi ; flanger : balayage exponentiel du retard
//    (J. O. Smith, « Physical Audio Signal Processing », Flanger).
//==============================================================================
void WackdAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    handleMidi (midi);

    const int numSamples = buffer.getNumSamples();
    const int inChans    = getTotalNumInputChannels();
    const int outChans   = getTotalNumOutputChannels();

    if (numSamples == 0 || delayLine.empty())
        return;

    const float sp = pSpeed->load() / 10.0f;
    const float dp = pDepth->load() / 10.0f;
    const float dl = pDelay->load() / 10.0f;
    const float rg = pRegen->load() / 10.0f;
    const bool flanger   = pMode->load()   > 0.5f;
    const bool wackd     = pKrazy->load()  > 0.5f;
    const bool active    = pActive->load() > 0.5f;
    const bool cf7Stereo = pStereo->load() > 0.5f;

    // --- SPEED (triangle, plage 1:100 en Normal) -------------------------------------
    double rate;
    if (! wackd) rate = flanger ? 0.05 * std::pow (100.0, (double) sp)    // 0,05 .. 5 Hz
                                : 0.10 * std::pow (100.0, (double) sp);   // 0,1  .. 10 Hz
    else         rate = flanger ? 0.30 * std::pow (66.7, (double) sp)     // 0,3  .. 20 Hz
                                : 0.60 * std::pow (66.7, (double) sp);    // 0,6  .. 40 Hz

    // --- DEPTH : en Wack'd, R44 ajoute une modulation fixe -------------------------------
    const double eff = wackd ? 0.35 + 0.65 * dp : (double) dp;

    double baseMs, excMs, cutoff, regenMax, dryG, wetG;
    const bool expoSweep = flanger;

    if (! flanger)
    {
        if (! wackd)
        {
            baseMs   = 5.0 + 20.0 * dl;                                     // 5 .. 25 ms
            excMs    = eff * std::min (0.6 * baseMs, 14.0 / (rate + 0.7));  // crete a crete
            cutoff   = 10000.0 - 2000.0 * dl;
            regenMax = 0.65;
            dryG = 0.70; wetG = 0.80;
        }
        else
        {
            baseMs   = 1.5 + 8.0 * dl;
            excMs    = eff * std::min (0.9 * baseMs, 40.0 / (rate + 2.0));
            cutoff   = 6000.0;
            regenMax = 0.60;
            dryG = 0.0;  wetG = 1.0;
        }
    }
    else
    {
        if (! wackd)
        {
            baseMs   = 0.20 + 2.3 * std::pow ((double) dl, 1.5);            // retard mini
            excMs    = std::min (baseMs * (1.0 + 18.0 * eff), 16.0);        // retard maxi
            cutoff   = 13000.0 - 3000.0 * dl;
            regenMax = 0.95;
            dryG = 0.70; wetG = 0.80;
        }
        else
        {
            baseMs   = 0.35 + 3.0 * std::pow ((double) dl, 1.4);
            excMs    = std::min (baseMs * (1.0 + 6.0 * eff), 12.0);
            cutoff   = 6500.0;
            regenMax = 0.92;
            dryG = 0.0;  wetG = 1.0;
        }
    }

    const float targetRegen = (float) (std::pow ((double) rg, 0.7) * regenMax);
    const float targetAct   = active ? 1.0f : 0.0f;

    smCutoff += (float) (cutoff - smCutoff) * 0.4f;
    preFilterA.setLowpass  (sr, smCutoff * 1.3, 0.707);
    postFilterA.setLowpass (sr, smCutoff, 0.54);
    postFilterB.setLowpass (sr, smCutoff, 1.31);
    postFilterC.setLowpass (sr, smCutoff, 0.54);
    postFilterD.setLowpass (sr, smCutoff, 1.31);
    const float dampCoef = (float) std::exp (-juce::MathConstants<double>::twoPi * 7500.0 / sr);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = inChans > 1 ? buffer.getReadPointer (1) : inL;

    juce::HeapBlock<float> dryL ((size_t) numSamples), dryR ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i) { dryL[i] = inL[i]; dryR[i] = inR[i]; }

    float* outL = buffer.getWritePointer (0);
    float* outR = outChans > 1 ? buffer.getWritePointer (1) : nullptr;

    const float msToSamples = (float) (sr / 1000.0);
    const float maxDelaySamples = (float) (delayMask - 4);

    auto safety = [] (float x)
    {
        const float a = std::abs (x);
        if (a < 0.95f) return x;
        return std::copysign (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f), x);
    };

    auto readTap = [this, maxDelaySamples] (float dSamp)
    {
        dSamp = juce::jlimit (2.0f, maxDelaySamples, dSamp);
        const float readPos = (float) writeIndex - dSamp;
        const int   idx     = (int) std::floor (readPos);
        const float frac    = readPos - (float) idx;
        const float xm1 = delayLine[(size_t) ((idx - 1) & delayMask)];
        const float x0  = delayLine[(size_t) ( idx      & delayMask)];
        const float x1  = delayLine[(size_t) ((idx + 1) & delayMask)];
        const float x2  = delayLine[(size_t) ((idx + 2) & delayMask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + x0;
    };

    auto shape = [expoSweep] (double phase)
    {
        const float tri  = (float) (phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0);
        const float sine = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * tri);
        return expoSweep ? 0.8f * tri + 0.2f * sine : 0.45f * tri + 0.55f * sine;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        smRate    += ((float) rate   - smRate)    * smoothCoef;
        smBaseMs  += ((float) baseMs - smBaseMs)  * smoothCoef;
        smRangeMs += ((float) excMs  - smRangeMs) * smoothCoef;
        smRegen   += (targetRegen    - smRegen)   * smoothCoef;
        smDry     += ((float) dryG   - smDry)     * smoothCoef;
        smWet     += ((float) wetG   - smWet)     * smoothCoef;
        smActive  += (targetAct      - smActive)  * activeCoef;
        smStereo  += ((cf7Stereo ? 1.0f : 0.0f) - smStereo) * activeCoef;

        lfoPhase += smRate / sr;
        if (lfoPhase >= 1.0) lfoPhase -= std::floor (lfoPhase);
        double phaseR = lfoPhase + 0.25;              // 2e lecture a 90 deg (stereo compatible mono)
        if (phaseR >= 1.0) phaseR -= 1.0;

        const float lfoL = shape (lfoPhase);
        const float lfoR = shape (phaseR);

        const float in = 0.5f * (dryL[i] + dryR[i]);
        delayLine[(size_t) writeIndex] = preFilterA.process (in + feedback * smRegen);

        auto delayFor = [&] (float lfo)
        {
            const float dMs = expoSweep ? smBaseMs * std::pow (std::max (smRangeMs, smBaseMs) / smBaseMs, lfo)
                                        : smBaseMs + smRangeMs * (lfo - 0.5f);
            return dMs * msToSamples;
        };

        float wetL = postFilterB.process (postFilterA.process (readTap (delayFor (lfoL))));
        float wet2 = postFilterD.process (postFilterC.process (readTap (delayFor (lfoR))));

        writeIndex = (writeIndex + 1) & delayMask;

        // boucle REGEN : saturation douce, anti-DC, amortissement des aigus
        float fb = std::tanh (wetL * 1.2f) * (1.0f / 1.2f);
        fbDcY = fb - fbDcX + 0.995f * fbDcY;
        fbDcX = fb;
        fbLP  = fbDcY + dampCoef * (fbLP - fbDcY);
        feedback = fbLP;

        const float dry = in * smDry;
        const float wL  = wetL * smWet;

        // Droite :
        //  - CF7 stereo : OUT2 = direct - effet (d'origine ; s'annule si la piste est sommee en mono)
        //  - compatible mono : Normal = direct + 2e lecture a 90 deg ; Wack'd = identique a gauche
        const float wackdAmt = 1.0f - juce::jlimit (0.0f, 1.0f, smDry / 0.70f);
        const float rMono = dry + (wet2 * smWet) * (1.0f - wackdAmt) + wL * wackdAmt;
        const float rCf7  = dry - wL;
        const float right = rMono * (1.0f - smStereo) + rCf7 * smStereo;

        const float a = smActive;
        if (outR != nullptr)
        {
            outL[i] = dryL[i] * (1.0f - a) + safety (dry + wL) * a;
            outR[i] = dryR[i] * (1.0f - a) + safety (right)    * a;
        }
        else
        {
            outL[i] = dryL[i] * (1.0f - a) + safety (dry + wL) * a;
        }
    }

    for (int ch = 2; ch < outChans; ++ch)
        buffer.clear (ch, 0, numSamples);

    lfoDisplay = (float) lfoPhase;
}

//==============================================================================
// Programmes (menu de presets du DAW)
//==============================================================================
int WackdAudioProcessor::getNumPrograms() { return getNumFactoryPresets(); }

int WackdAudioProcessor::getCurrentProgram()
{
    const int c = currentPreset.load();
    return (c >= 0 && c < getNumFactoryPresets()) ? c : 0;
}

void WackdAudioProcessor::setCurrentProgram (int index) { applyFactoryPreset (index); }

const juce::String WackdAudioProcessor::getProgramName (int index)
{
    return getFactoryPreset (index).name;
}

void WackdAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = getFactoryPreset (index);
    setParam ("speed", p.speed);
    setParam ("depth", p.depth);
    setParam ("delay", p.delay);
    setParam ("regen", p.regen);
    setParam ("mode",  (float) p.mode);
    setParam ("krazy", (float) p.krazy);
    currentPreset = juce::jlimit (0, getNumFactoryPresets() - 1, index);
}

//==============================================================================
// Presets utilisateur
//==============================================================================
juce::File WackdAudioProcessor::getUserPresetFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Studio Wiga")
             .getChildFile ("Wackd")
             .getChildFile ("UserPresets.xml");
}

bool WackdAudioProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9)
        return false;

    auto file = getUserPresetFile();
    file.getParentDirectory().createDirectory();

    std::unique_ptr<juce::XmlElement> root;
    if (file.existsAsFile())
        root = juce::XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("UserPresets"))
        root = std::make_unique<juce::XmlElement> ("UserPresets");

    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
    {
        if (e->getIntAttribute ("slot") == slot)
        {
            root->removeChildElement (e, true);
            break;
        }
    }

    auto* e = root->createNewChildElement ("Preset");
    e->setAttribute ("slot",  slot);
    e->setAttribute ("speed", (double) pSpeed->load());
    e->setAttribute ("depth", (double) pDepth->load());
    e->setAttribute ("delay", (double) pDelay->load());
    e->setAttribute ("regen", (double) pRegen->load());
    e->setAttribute ("mode",  pMode->load()  > 0.5f ? 1 : 0);
    e->setAttribute ("krazy", pKrazy->load() > 0.5f ? 1 : 0);

    const bool ok = root->writeTo (file);
    if (ok)
        currentPreset = 100 + slot;
    return ok;
}

bool WackdAudioProcessor::loadUserPreset (int slot)
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile())
        return false;

    auto root = juce::XmlDocument::parse (file);
    if (root == nullptr)
        return false;

    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
    {
        if (e->getIntAttribute ("slot") == slot)
        {
            setParam ("speed", (float) e->getDoubleAttribute ("speed", 3.0));
            setParam ("depth", (float) e->getDoubleAttribute ("depth", 5.0));
            setParam ("delay", (float) e->getDoubleAttribute ("delay", 4.0));
            setParam ("regen", (float) e->getDoubleAttribute ("regen", 0.0));
            setParam ("mode",  (float) e->getIntAttribute ("mode", 0));
            setParam ("krazy", (float) e->getIntAttribute ("krazy", 0));
            currentPreset = 100 + slot;
            return true;
        }
    }
    return false;
}

bool WackdAudioProcessor::hasUserPreset (int slot) const
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile())
        return false;

    if (auto root = juce::XmlDocument::parse (file))
        for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
            if (e->getIntAttribute ("slot") == slot)
                return true;

    return false;
}

//==============================================================================
void WackdAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WackdAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("currentPreset", 0);
            apvts.replaceState (state);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* WackdAudioProcessor::createEditor()
{
    return new WackdAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WackdAudioProcessor();
}
