#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potard : filmstrip haute definition (calotte bronze usinee + jupe moletee),
// lumiere fixe en haut a gauche ; repere et moletage tournent avec la valeur
//==============================================================================
class NickelKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Image strip;   // filmstrip verticale : 128 images carrees
};

//==============================================================================
// Interrupteur a glissiere (MODE / KRAZY)
//==============================================================================
class SlideSwitch : public juce::Component
{
public:
    SlideSwitch (juce::RangedAudioParameter& p, juce::Image sprite);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    bool value = false;
    juce::Image sprite;   // interrupteur d'origine (levier a gauche)
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Zone cliquable du footswitch (dessine sur l'image de fond)
//==============================================================================
class FootSwitchArea : public juce::Component
{
public:
    explicit FootSwitchArea (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    bool isOn() const { return value; }

private:
    bool value = true;
    bool pressed = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Transmet le clic droit des commandes vers le menu de l'editeur
//==============================================================================
struct RightClickForwarder : public juce::MouseListener
{
    std::function<void()> callback;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && callback)
            callback();
    }
};

//==============================================================================
class WackdAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit WackdAudioProcessorEditor (WackdAudioProcessor&);
    ~WackdAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Repere de reference (100 %) ; taille par defaut = taille minimale
    static constexpr int baseW = 760;
    static constexpr int baseH = 1080;
    static constexpr int defaultH = 493;

private:
    void timerCallback() override;
    void showContextMenu();
    void setScaledHeight (int h);
    void updateBars (double dt);
    void drawLedAndBars (juce::Graphics&);
    void drawStatus (juce::Graphics&);
    void drawBrand (juce::Graphics&);
    void rebuildBrand();
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;

    WackdAudioProcessor& proc;
    NickelKnobLookAndFeel lnf;
    RightClickForwarder rightClickFwd;
    juce::Image background, barsLit;

    // « STUDIO WIGA » (police du logo)
    juce::Typeface::Ptr brandFace;
    juce::Path brandPath;


    juce::Slider speed, depth, delay, regen;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aSpeed, aDepth, aDelay, aRegen;

    SlideSwitch modeSwitch, krazySwitch;
    FootSwitchArea footswitch;

    // Animation LED / barres
    static constexpr int numBars = 9;
    float barFill[numBars] {};
    float barSpark[numBars] {};
    float ledFlicker = 1.0f;
    double animPhase = 0.0;
    juce::Random rng;
    double lastTime = 0.0;

    juce::String lastStatus, lastPreset;
    float lastKnobs[4] { -1.0f, -1.0f, -1.0f, -1.0f };
    double statusUntil = 0.0;
    float statusAlpha = 0.0f;
    int   meterKnob = -1;        // dernier potard tourne (-1 = moyenne)
    float meterLevel = 0.0f;
    int lastMidiCount = 0;
    int midiFlash = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WackdAudioProcessorEditor)
};
