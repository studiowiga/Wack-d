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
    // Intro de « My Way » (Limp Bizkit, 2000) : Wes Borland y utilisait un flanger
    // Ibanez reglage « whacked » (interview MusicRadar) -> Flanger + Wack'd :
    // son mouille, sans signal direct, ondulation moyenne et resonance metallique.
    { "My Road",               3.2f, 5.5f, 3.0f, 4.5f, 1, 1 },
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

    for (auto* f : { &preFilterA, &preFilterB, &postFilterA, &postFilterB })
    {
        f->reset();
        f->setLowpass (sr, 9000.0);
    }
    smWet = pKrazy->load() > 0.5f ? 1.0f : 0.72f;
    smDry = pKrazy->load() > 0.5f ? 0.0f : 0.72f;
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
// Traitement audio (v1.4)
//
// Chemin du signal de la CF7 (schema) : tampon d'entree -> filtre anti-repliement
// -> ligne a retard numerique M50198 (horloge modulee par le LFO triangle IC5)
// -> filtre de reconstruction -> boucle REGEN -> sommateurs de sortie.
//   NORMAL : OUT1 = direct + effet, OUT2 (INV) = direct - effet (stereo large).
//   WACK'D : le signal direct est coupe (vibrato / sons « Korny »), la vitesse du
//            LFO monte jusqu'au domaine audio (bandes laterales type ring-mod),
//            les aigus sont plus filtres et le niveau egal au bypass.
//
// Choix DSP (cf. J. O. Smith, « Physical Audio Signal Processing », Flanger) :
//   - chorus : LFO triangle arrondi (sans « cassure » de hauteur aux sommets),
//     excursion limitee en fonction de la vitesse pour garder un desaccord musical ;
//   - flanger : balayage exponentiel du retard (triangle sur une echelle
//     logarithmique) -> le peigne de filtres monte et descend regulierement ;
//   - REGEN : saturation douce + anti-DC + amortissement des aigus dans la boucle ;
//   - aucun ecretage sur la sortie dans l'usage normal (marge de niveau).
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
    const bool flanger = pMode->load()  > 0.5f;
    const bool wackd   = pKrazy->load() > 0.5f;
    const bool active  = pActive->load() > 0.5f;

    // --- SPEED ------------------------------------------------------------------
    const double rate = wackd ? 0.5  * std::pow (240.0, (double) sp)     // 0,5 .. 120 Hz
                              : 0.08 * std::pow (100.0, (double) sp);    // 0,08 .. 8 Hz

    // --- DELAY TIME / DEPTH / REGEN -------------------------------------------------
    double baseMs, excMs, cutoff, regenMax, dryG, wetG;
    bool expoSweep;

    if (! flanger)
    {
        expoSweep = false;
        if (! wackd)
        {
            baseMs   = 4.0 + 18.0 * dl;                                   // 4 .. 22 ms
            excMs    = dp * std::min (0.45 * baseMs, 9.0 / (rate + 0.6)); // crete a crete
            cutoff   = 9500.0 - 2500.0 * dl;
            regenMax = 0.60;
            dryG = 0.72; wetG = 0.72;
        }
        else
        {
            baseMs   = 1.0 + 7.0 * dl;
            excMs    = dp * std::min (0.9 * baseMs, 6.0 / (rate + 1.0));
            cutoff   = 5200.0;
            regenMax = 0.55;
            dryG = 0.0;  wetG = 1.0;
        }
    }
    else
    {
        expoSweep = true;
        if (! wackd)
        {
            baseMs   = 0.25 + 2.5 * std::pow ((double) dl, 1.6);         // retard mini du balayage
            excMs    = std::min (baseMs * (1.0 + 13.0 * dp), 14.0);      // retard maxi
            cutoff   = 12000.0 - 3000.0 * dl;
            regenMax = 0.92;
            dryG = 0.70; wetG = 0.70;
        }
        else
        {
            baseMs   = 0.40 + 3.0 * std::pow ((double) dl, 1.4);
            excMs    = std::min (baseMs * (1.0 + 4.0 * dp / (1.0 + rate / 15.0)), 12.0);
            cutoff   = 5600.0;
            regenMax = 0.90;
            dryG = 0.0;  wetG = 1.0;
        }
    }

    const float targetRegen = (float) (std::pow ((double) rg, 0.8) * regenMax);
    const float targetAct   = active ? 1.0f : 0.0f;

    smCutoff += (float) (cutoff - smCutoff) * 0.4f;
    preFilterA.setLowpass  (sr, smCutoff * 1.3, 0.707);
    postFilterA.setLowpass (sr, smCutoff, 0.54);
    postFilterB.setLowpass (sr, smCutoff, 1.31);
    const float dampCoef = (float) std::exp (-juce::MathConstants<double>::twoPi * 7000.0 / sr);

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

    for (int i = 0; i < numSamples; ++i)
    {
        smRate    += ((float) rate   - smRate)    * smoothCoef;
        smBaseMs  += ((float) baseMs - smBaseMs)  * smoothCoef;
        smRangeMs += ((float) excMs  - smRangeMs) * smoothCoef;
        smRegen   += (targetRegen    - smRegen)   * smoothCoef;
        smDry     += ((float) dryG   - smDry)     * smoothCoef;
        smWet     += ((float) wetG   - smWet)     * smoothCoef;
        smActive  += (targetAct      - smActive)  * activeCoef;

        // LFO : triangle (comme IC5) aux sommets arrondis
        lfoPhase += smRate / sr;
        if (lfoPhase >= 1.0) lfoPhase -= std::floor (lfoPhase);
        const float tri  = (float) (lfoPhase < 0.5 ? lfoPhase * 2.0 : 2.0 - lfoPhase * 2.0);
        const float sine = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * tri);
        const float lfo  = expoSweep ? 0.75f * tri + 0.25f * sine : 0.35f * tri + 0.65f * sine;

        const float in = 0.5f * (dryL[i] + dryR[i]);

        // ecriture (avec REGEN) apres le filtre anti-repliement
        delayLine[(size_t) writeIndex] = preFilterA.process (in + feedback * smRegen);

        // retard courant
        float dMs = expoSweep ? smBaseMs * std::pow (std::max (smRangeMs, smBaseMs) / smBaseMs, lfo)
                              : smBaseMs + smRangeMs * (lfo - 0.5f);
        float dSamp = juce::jlimit (2.0f, maxDelaySamples, dMs * msToSamples);

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
        float wet = ((c3 * frac + c2) * frac + c1) * frac + x0;

        writeIndex = (writeIndex + 1) & delayMask;

        wet = postFilterB.process (postFilterA.process (wet));

        // boucle REGEN : saturation douce, anti-DC, amortissement des aigus
        float fb = std::tanh (wet * 1.2f) * (1.0f / 1.2f);
        fbDcY = fb - fbDcX + 0.995f * fbDcY;
        fbDcX = fb;
        fbLP  = fbDcY + dampCoef * (fbLP - fbDcY);
        feedback = fbLP;

        const float dry = in * smDry;
        const float w   = wet * smWet;
        // Normal : OUT2 inverse l'effet (comme la pedale). Wack'd (sans direct) :
        // meme polarite sur les deux sorties pour rester compatible mono.
        const float wetR = w * (1.0f - 2.0f * juce::jlimit (0.0f, 1.0f, smDry / 0.72f));

        const float a = smActive;
        if (outR != nullptr)
        {
            outL[i] = dryL[i] * (1.0f - a) + safety (dry + w)    * a;
            outR[i] = dryR[i] * (1.0f - a) + safety (dry + wetR) * a;
        }
        else
        {
            outL[i] = dryL[i] * (1.0f - a) + safety (dry + w) * a;
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
