#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

//==============================================================================
// WACK'D (Studio Wiga) — reproduction de la pedale Ibanez CF7 Chorus/Flanger
// (serie Tone-Lok). Voir README.md pour la documentation du circuit.
//==============================================================================

namespace wiga
{
    // Filtre biquad passe-bas (RBJ) — sert a imiter les filtres anti-repliement
    // et de reconstruction places autour de la puce de retard numerique M50198.
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void setLowpass (double sampleRate, double fc, double q = 0.7071)
        {
            fc = juce::jlimit (20.0, sampleRate * 0.45, fc);
            const double w     = juce::MathConstants<double>::twoPi * fc / sampleRate;
            const double c     = std::cos (w);
            const double s     = std::sin (w);
            const double alpha = s / (2.0 * q);
            const double a0    = 1.0 + alpha;

            b0 = (float) (((1.0 - c) * 0.5) / a0);
            b1 = (float) ((1.0 - c) / a0);
            b2 = b0;
            a1 = (float) ((-2.0 * c) / a0);
            a2 = (float) ((1.0 - alpha) / a0);
        }

        inline float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() noexcept { z1 = z2 = 0.0f; }
    };

    struct PresetData
    {
        const char* name;
        float speed, depth, delay, regen; // 0..10 comme sur la pedale
        int   mode;                       // 0 = Chorus, 1 = Flanger
        int   krazy;                      // 0 = Normal, 1 = Wack'd
    };
}

//==============================================================================
class WackdAudioProcessor : public juce::AudioProcessor,
                            private juce::Timer
{
public:
    WackdAudioProcessor();
    ~WackdAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.3; }

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Presets d'usine
    static int getNumFactoryPresets();
    static const wiga::PresetData& getFactoryPreset (int index);
    void applyFactoryPreset (int index);

    // Presets utilisateur (emplacements 1 a 9, sauvegardes sur le disque)
    bool saveUserPreset (int slot);
    bool loadUserPreset (int slot);
    bool hasUserPreset (int slot) const;
    static juce::File getUserPresetFile();

    // Informations pour la ligne d'etat de l'interface
    std::atomic<int> currentPreset { 0 };     // >=0 usine, 100+n utilisateur, -1 = modifie
    std::atomic<int> lastMidiCC    { -1 };
    std::atomic<int> lastMidiValue { 0 };
    std::atomic<int> lastMidiProgram { -1 };
    std::atomic<int> midiCounter   { 0 };
    std::atomic<float> lfoDisplay  { 0.0f };  // pour l'animation de la LED en mode Wack'd (optionnel)

    // Table MIDI (CC) — affichee aussi dans le menu clic droit
    enum MidiMap
    {
        ccSpeed      = 20,
        ccDepth      = 21,
        ccDelayTime  = 22,
        ccRegen      = 23,
        ccMode       = 24,
        ccKrazy      = 25,
        ccFootswitch = 26, // >= 64 : effet ON, < 64 : effet OFF
        ccFootToggle = 27  // chaque appui (passage >= 64) inverse l'etat
    };

private:
    void handleMidi (juce::MidiBuffer& midi);
    void setParam (const juce::String& id, float plainValue);

    // Anti-gel v2 : depuis le thread audio, aucun appel a l'hote ; les valeurs sont mises en file
    // et transmises au DAW par un timer du thread de messages (60 Hz).
    void timerCallback() override;
    void setParamFromAudio (const char* id, float plain);
    float queuedOrCurrent (const char* id, std::atomic<float>* current) const;
    static constexpr int numQueued = 7;
    static const char* queuedId (int i);
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];
    std::atomic<int>   pendingPreset { -1 };

    // Parametres (acces rapide)
    std::atomic<float>* pSpeed  = nullptr;
    std::atomic<float>* pDepth  = nullptr;
    std::atomic<float>* pDelay  = nullptr;
    std::atomic<float>* pRegen  = nullptr;
    std::atomic<float>* pMode   = nullptr;
    std::atomic<float>* pKrazy  = nullptr;
    std::atomic<float>* pActive = nullptr;
    std::atomic<float>* pStereo = nullptr;

    // Moteur de retard
    std::vector<float> dryBufL, dryBufR;    // copie du signal direct, preparee hors du thread audio
    std::vector<float> delayLine;
    int    delayMask  = 0;
    int    writeIndex = 0;
    double sr         = 44100.0;

    double lfoPhase   = 0.0;
    float  smRate     = 1.0f;
    float  smBaseMs   = 5.0f;
    float  smRangeMs  = 2.0f;
    float  smRegen    = 0.0f;
    float  smDry      = 0.72f;
    float  smWet      = 0.72f;
    float  fbLP       = 0.0f;
    float  smStereo   = 0.0f;
    float  smActive   = 1.0f;
    float  smCutoff   = 8000.0f;
    float  feedback   = 0.0f;
    float  fbDcX      = 0.0f, fbDcY = 0.0f;
    float  smoothCoef = 0.001f;
    float  activeCoef = 0.001f;

    wiga::Biquad preFilterA, preFilterB, postFilterA, postFilterB, postFilterC, postFilterD;

    int prevToggleValue = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WackdAudioProcessor)
};
