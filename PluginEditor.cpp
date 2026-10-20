#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    Font font (float h, bool bold = true)
    {
        return Font (FontOptions (h, bold ? Font::bold : Font::plain));
    }

    // Direction de la lumiere (haut-gauche), dans le repere ecran (y vers le bas)
    const float lightAngle = -2.35619f;                 // atan2(-1, -1)
    const Point<float> lightVec { -0.7071f, -0.7071f };

    inline float lit (float screenAngle)                // 0 (ombre) .. 1 (face a la lumiere)
    {
        return 0.5f + 0.5f * std::cos (screenAngle - lightAngle);
    }

    // Positions dans le repere de reference 760 x 1080 (visuel Wack'd sur fond noir)
    const float knobY = 148.0f;
    const float knobX[4] = { 169.5f, 311.5f, 454.0f, 597.0f };   // centres des potards
    const float knobR = 56.0f;                                   // rayon de la jupe moletee
    const float knobStripHalf = 1.20f;                           // marge de l'image (ombre)
    const int   knobFrames = 128;

    const Rectangle<float> modeRect  { 179.0f, 317.0f, 83.0f, 40.0f };
    const Rectangle<float> krazyRect { 499.0f, 317.0f, 83.0f, 40.0f };
    const Point<float>     ledPos    { 379.5f, 334.5f };
    const Rectangle<float> barsArea  { 319.0f, 368.0f, 121.0f, 57.0f };
    const float slots[9][4] =
    {
        { 327.0f, 376.0f, 335.5f, 415.5f }, { 339.5f, 385.0f, 347.5f, 415.5f }, { 351.5f, 390.5f, 359.5f, 415.5f },
        { 363.5f, 394.0f, 371.5f, 415.5f }, { 375.5f, 395.0f, 383.5f, 415.5f }, { 387.5f, 394.0f, 395.5f, 415.5f },
        { 399.5f, 390.5f, 407.0f, 415.0f }, { 412.0f, 384.0f, 419.5f, 415.5f }, { 424.0f, 376.0f, 431.5f, 415.5f }
    };
    const Rectangle<float> statusRect{ 149.5f, 430.0f, 460.0f, 58.0f };
    const Point<float>     footPos   { 379.0f, 818.0f };
    const float            footR     = 50.0f;
    const Rectangle<float> brandRect { 259.0f, 871.0f, 240.0f, 34.0f };

    const Colour iceCore  (0xffeafaff);
    const Colour iceMid   (0xffa9e9ff);
    const Colour iceDeep  (0xff4bb8e8);
    // flanger : jaune-orange, meme intensite que le bleu glace
    const Colour ambCore  (0xfffff6e2);
    const Colour ambMid   (0xffffc25c);
    const Colour ambDeep  (0xffec8a16);
}

//==============================================================================
// POTARD
//==============================================================================
void NickelKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                              float pos, float, float, Slider&)
{
    // Filmstrip haute definition (128 images) : calotte bronze usinee dont le reflet
    // reste face a la lumiere, repere argente et moletage qui tournent avec la valeur.
    if (! strip.isValid())
        return;

    const int fs = strip.getWidth();
    const int frame = jlimit (0, knobFrames - 1, roundToInt (pos * (float) (knobFrames - 1)));

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, x, y, w, h, 0, frame * fs, fs, fs);
}

//==============================================================================
// INTERRUPTEUR A GLISSIERE
//==============================================================================
SlideSwitch::SlideSwitch (RangedAudioParameter& p, Image left, Image right)
    : spriteLeft (left), spriteRight (right),
      attachment (p, [this] (float v) { value = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void SlideSwitch::paint (Graphics& g)
{
    // levier a gauche = CHORUS / NORMAL, levier a droite = FLANGER / WACK'D
    const auto& img = value ? spriteRight : spriteLeft;
    if (! img.isValid())
        return;
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (img, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

void SlideSwitch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
}

//==============================================================================
// FOOTSWITCH
//==============================================================================
FootSwitchArea::FootSwitchArea (RangedAudioParameter& p)
    : attachment (p, [this] (float v) { value = v > 0.5f; if (auto* par = getParentComponent()) par->repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool FootSwitchArea::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * 0.5f;
}

void FootSwitchArea::paint (Graphics& g)
{
    if (pressed)
    {
        g.setColour (Colours::black.withAlpha (0.22f));
        g.fillEllipse (getLocalBounds().toFloat().reduced ((float) getWidth() * 0.22f));
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    pressed = true;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
}

void FootSwitchArea::mouseUp (const MouseEvent&)
{
    pressed = false;
    repaint();
}

//==============================================================================
// EDITEUR
//==============================================================================
WackdAudioProcessorEditor::WackdAudioProcessorEditor (WackdAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      modeSwitch  (*p.apvts.getParameter ("mode"),
                   ImageCache::getFromMemory (BinaryData::mode_sw_png,    BinaryData::mode_sw_pngSize),
                   ImageCache::getFromMemory (BinaryData::mode_sw_r_png,  BinaryData::mode_sw_r_pngSize)),
      krazySwitch (*p.apvts.getParameter ("krazy"),
                   ImageCache::getFromMemory (BinaryData::krazy_sw_png,   BinaryData::krazy_sw_pngSize),
                   ImageCache::getFromMemory (BinaryData::krazy_sw_r_png, BinaryData::krazy_sw_r_pngSize)),
      footswitch  (*p.apvts.getParameter ("active"))
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    barsLit    = ImageCache::getFromMemory (BinaryData::bars_lit_png,       BinaryData::bars_lit_pngSize);
    barsLitAmber = ImageCache::getFromMemory (BinaryData::bars_lit_amber_png, BinaryData::bars_lit_amber_pngSize);
    modeBlend  = proc.apvts.getRawParameterValue ("mode")->load() > 0.5f ? 1.0f : 0.0f;
    lnf.strip  = ImageCache::getFromMemory (BinaryData::knob_strip_png, BinaryData::knob_strip_pngSize);
    brandFace  = Typeface::createSystemTypefaceFor (BinaryData::MontserratBold_ttf, (size_t) BinaryData::MontserratBold_ttfSize);

    // valeurs de depart : la ligne d'etat ne s'affiche qu'apres un mouvement de potard
    const char* ids[4] = { "speed", "depth", "delay", "regen" };
    for (int i = 0; i < 4; ++i)
        lastKnobs[i] = proc.apvts.getRawParameterValue (ids[i])->load();
    rightClickFwd.callback = [this] { showContextMenu(); };

    auto setupKnob = [this] (Slider& s, const char* id,
                             std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.setLookAndFeel (&lnf);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.addMouseListener (&rightClickFwd, false);
        addAndMakeVisible (s);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
    };

    setupKnob (speed, "speed", aSpeed);
    setupKnob (depth, "depth", aDepth);
    setupKnob (delay, "delay", aDelay);
    setupKnob (regen, "regen", aRegen);

    for (auto* c : { (Component*) &modeSwitch, (Component*) &krazySwitch, (Component*) &footswitch })
    {
        c->addMouseListener (&rightClickFwd, false);
        addAndMakeVisible (c);
    }

    // Taille par defaut = taille minimale (impossible de reduire en dessous)
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (defaultW, defaultH);

    lastTime = Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

WackdAudioProcessorEditor::~WackdAudioProcessorEditor()
{
    stopTimer();
    for (auto* s : { &speed, &depth, &delay, &regen })
    {
        s->removeMouseListener (&rightClickFwd);
        s->setLookAndFeel (nullptr);
    }
    for (auto* c : { (Component*) &modeSwitch, (Component*) &krazySwitch, (Component*) &footswitch })
        c->removeMouseListener (&rightClickFwd);
}

void WackdAudioProcessorEditor::setScaledHeight (int h)
{
    h = jmax ((int) defaultH, h);
    setSize (roundToInt ((float) h * (float) baseW / (float) baseH), h);
}

Rectangle<int> WackdAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / (float) baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

void WackdAudioProcessorEditor::resized()
{
    Slider* knobs[4] = { &speed, &depth, &delay, &regen };
    const float half = knobR * knobStripHalf;
    for (int i = 0; i < 4; ++i)
        knobs[i]->setBounds (scaled ({ knobX[i] - half, knobY - half, half * 2.0f, half * 2.0f }));

    modeSwitch.setBounds  (scaled (modeRect));
    krazySwitch.setBounds (scaled (krazyRect));
    footswitch.setBounds  (scaled ({ footPos.x - footR, footPos.y - footR, footR * 2.0f, footR * 2.0f }));
}

//==============================================================================
// Ligne d'etat
//==============================================================================
String WackdAudioProcessorEditor::buildStatusLine() const
{
    auto v = [this] (const char* id) { return String (proc.apvts.getRawParameterValue (id)->load(), 1); };
    return "SPEED " + v ("speed") + "  DEPTH " + v ("depth")
         + "  DELAY " + v ("delay") + "  REGEN " + v ("regen");
}

String WackdAudioProcessorEditor::buildPresetLine() const
{
    const bool fl = proc.apvts.getRawParameterValue ("mode")->load()  > 0.5f;
    const bool wk = proc.apvts.getRawParameterValue ("krazy")->load() > 0.5f;
    const bool on = proc.apvts.getRawParameterValue ("active")->load() > 0.5f;

    String s = String (fl ? "FLANGER" : "CHORUS") + " / " + (wk ? "WACK'D" : "NORMAL");
    if (! on) s << " / BYPASS";

    const int cp = proc.currentPreset.load();
    if (cp >= 0 && cp < WackdAudioProcessor::getNumFactoryPresets())
        s << "  |  P" << (cp + 1) << " " << WackdAudioProcessor::getFactoryPreset (cp).name;
    else if (cp > 100)
        s << "  |  USER " << (cp - 100);

    const int cc = proc.lastMidiCC.load();
    if (cc >= 0)       s << "  |  CC" << cc << "=" << proc.lastMidiValue.load();
    else if (cc == -2) s << "  |  PC " << proc.lastMidiProgram.load();

    return s;
}

//==============================================================================
// Jauge sous la LED : progression horizontale de gauche (0 %) a droite (100 %)
// = valeur du dernier potard tourne (au demarrage : moyenne des 4 potards).
// Les barres allumees scintillent legerement (plus fort avec REGEN).
//==============================================================================
void WackdAudioProcessorEditor::updateBars (double)
{
    const char* ids[4] = { "speed", "depth", "delay", "regen" };
    const bool on = proc.apvts.getRawParameterValue ("active")->load() > 0.5f;
    const float rg = proc.apvts.getRawParameterValue ("regen")->load() / 10.0f;

    float level = 0.0f;
    if (meterKnob >= 0)
        level = proc.apvts.getRawParameterValue (ids[meterKnob])->load() / 10.0f;
    else
    {
        for (auto* id : ids)
            level += proc.apvts.getRawParameterValue (id)->load() / 40.0f;
    }

    const float target = on ? jlimit (0.0f, 1.0f, level) : 0.0f;
    meterLevel += (target - meterLevel) * 0.35f;

    const float shimmer = 0.05f + 0.20f * rg;
    const float lit = meterLevel * (float) numBars;          // nombre de barres allumees (fractionnaire)

    for (int i = 0; i < numBars; ++i)
    {
        barFill[i]  = jlimit (0.0f, 1.0f, lit - (float) i);  // 1 = pleine, 0..1 = barre en cours
        barSpark[i] = on ? 1.0f - shimmer * rng.nextFloat() : 1.0f;
    }

    ledFlicker = on ? 0.95f + 0.05f * rng.nextFloat() : 1.0f;
}

void WackdAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = jlimit (0.0, 0.1, (now - lastTime) * 0.001);
    lastTime = now;

    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    lastStatus = buildStatusLine();
    lastPreset = buildPresetLine();

    // La ligne d'etat n'apparait que lorsqu'un potard bouge (souris, automation ou MIDI),
    // reste 2,5 s puis s'efface en douceur.
    {
        const char* ids[4] = { "speed", "depth", "delay", "regen" };
        for (int i = 0; i < 4; ++i)
        {
            const float v = proc.apvts.getRawParameterValue (ids[i])->load();
            if (std::abs (v - lastKnobs[i]) > 1.0e-4f)
            {
                lastKnobs[i] = v;
                statusUntil = now + 2500.0;
                meterKnob = i;
            }
        }
        Slider* sl[4] = { &speed, &depth, &delay, &regen };
        for (int i = 0; i < 4; ++i)
            if (sl[i]->isMouseButtonDown())
            {
                statusUntil = now + 2500.0;
                meterKnob = i;
            }

        const float target = now < statusUntil ? 1.0f : 0.0f;
        statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.12f);
        if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    }

    updateBars (dt);

    {
        const float target = proc.apvts.getRawParameterValue ("mode")->load() > 0.5f ? 1.0f : 0.0f;
        modeBlend += (target - modeBlend) * 0.25f;
        if (std::abs (target - modeBlend) < 0.002f) modeBlend = target;
    }

    // On ne redessine que la LED, les barres et la ligne d'etat
    repaint (scaled ({ ledPos.x - 60.0f, ledPos.y - 60.0f, 120.0f, 100.0f }).getUnion (scaled (barsArea.expanded (8.0f)))
                 .getUnion (scaled (statusRect.expanded (4.0f))));
}

//==============================================================================
void WackdAudioProcessorEditor::drawLedAndBars (Graphics& g)
{
    const bool on = footswitch.isOn();
    const float mb = modeBlend;
    const Colour core = iceCore.interpolatedWith (ambCore, mb);
    const Colour mid  = iceMid .interpolatedWith (ambMid,  mb);
    const Colour deep = iceDeep.interpolatedWith (ambDeep, mb);

    // --- jauge : barres allumees de gauche a droite, bleu (chorus) / jaune-orange (flanger)
    for (int i = 0; i < numBars; ++i)
    {
        const float f = barFill[i];
        if (f <= 0.01f)
            continue;

        const auto* s = slots[i];
        Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (Rectangle<float> (s[0] - 4.0f, s[1] - 4.0f, (s[2] - s[0]) + 8.0f, (s[3] - s[1]) + 8.0f)
                                .getSmallestIntegerContainer());
        const float op = jlimit (0.0f, 1.0f, f * barSpark[i]);
        if (mb < 0.999f && barsLit.isValid())
        {
            g.setOpacity (op * (1.0f - mb));
            g.drawImage (barsLit, barsArea, RectanglePlacement::stretchToFit);
        }
        if (mb > 0.001f && barsLitAmber.isValid())
        {
            g.setOpacity (op * mb);
            g.drawImage (barsLitAmber, barsArea, RectanglePlacement::stretchToFit);
        }
    }

    // --- LED (lentille redessinee dans la bague du visuel) ------------------------
    const auto c = ledPos;
    const float rLens = 26.5f;   // lentille dans la bague du visuel (taille v1.4)

    if (on)
    {
        ColourGradient bloom (mid.withAlpha (0.42f * ledFlicker), c.x, c.y,
                              mid.withAlpha (0.0f), c.x + 62.0f, c.y, true);
        g.setGradientFill (bloom);
        g.fillEllipse (c.x - 62.0f, c.y - 62.0f, 124.0f, 124.0f);

        ColourGradient lens (core, c.x - 3.0f, c.y - 4.0f,
                             deep.withMultipliedBrightness (ledFlicker), c.x + rLens, c.y + rLens * 0.3f, true);
        lens.addColour (0.42, mid.withMultipliedBrightness (ledFlicker));
        g.setGradientFill (lens);
    }
    else
    {
        ColourGradient lens (Colour (0xff3a4a54), c.x - 6.0f, c.y - 8.0f,
                             Colour (0xff0c141a), c.x + rLens, c.y + rLens, true);
        g.setGradientFill (lens);
    }
    g.fillEllipse (c.x - rLens, c.y - rLens, rLens * 2.0f, rLens * 2.0f);

    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawEllipse (c.x - rLens, c.y - rLens, rLens * 2.0f, rLens * 2.0f, 1.2f);

    // reflet de verre fixe (cote lumiere)
    ColourGradient gl (Colours::white.withAlpha (on ? 0.55f : 0.35f), c.x - rLens * 0.3f, c.y - rLens * 0.6f,
                       Colours::white.withAlpha (0.0f), c.x - rLens * 0.3f, c.y + rLens * 0.1f, false);
    g.setGradientFill (gl);
    g.fillEllipse (c.x - rLens * 0.7f, c.y - rLens * 0.82f, rLens * 1.1f, rLens * 0.6f);
}

void WackdAudioProcessorEditor::drawStatus (Graphics& g)
{
    const float a = statusAlpha;
    if (a <= 0.0f)
        return;

    const Colour accent = iceMid.interpolatedWith (ambMid, modeBlend);

    g.setColour (Colours::black.withAlpha (0.72f * a));
    g.fillRoundedRectangle (statusRect, 9.0f);
    g.setColour (accent.withAlpha (0.45f * a));
    g.drawRoundedRectangle (statusRect.reduced (0.75f), 9.0f, 1.5f);

    g.setColour (Colours::white.withAlpha (a));
    g.setFont (font (20.0f, true));
    g.drawText (lastStatus.isEmpty() ? buildStatusLine() : lastStatus,
                statusRect.withHeight (31.0f).translated (0.0f, 3.0f), Justification::centred);

    g.setColour ((midiFlash > 0 ? Colours::white : accent).withMultipliedAlpha (a));
    g.setFont (font (15.5f, true));
    g.drawText (lastPreset.isEmpty() ? buildPresetLine() : lastPreset,
                statusRect.withTrimmedTop (31.0f).withTrimmedBottom (5.0f), Justification::centred);
}

void WackdAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));

    // Les prises jack (identiques a Textural) font partie de l'image de fond.
    drawLedAndBars (g);
    drawStatus (g);
    drawBrand (g);
}

//==============================================================================
// « STUDIO WIGA » : police du logo Studio Wiga (Montserrat Bold), imprime
// dans la meme encre que « Wack'd » et « flangerus »
//==============================================================================
void WackdAudioProcessorEditor::rebuildBrand()
{
    brandPath.clear();
    if (brandFace != nullptr)
    {
        GlyphArrangement ga;
        ga.addLineOfText (Font (FontOptions (brandFace).withHeight (29.0f)).withExtraKerningFactor (0.02f),
                          "STUDIO WIGA", 0.0f, 0.0f);
        ga.createPath (brandPath);
    }
    if (brandPath.isEmpty())
        return;

    const auto b = brandPath.getBounds();
    brandPath.applyTransform (AffineTransform::translation (brandRect.getCentreX() - b.getCentreX(),
                                                            brandRect.getCentreY() - b.getCentreY()));
}

void WackdAudioProcessorEditor::drawBrand (Graphics& g)
{
    if (brandPath.isEmpty())
        rebuildBrand();

    // voile creme (couleur du fond de la pedale) sous les lettres, comme sous le titre :
    // garde la lecture nette au-dessus des traits bleu-vert du dessin
    const Colour paper (0xffeae2cc);
    const float widths[] = { 13.0f, 9.0f, 5.5f };
    const float alphas[] = { 0.10f, 0.18f, 0.35f };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (paper.withAlpha (alphas[i]));
        g.strokePath (brandPath, PathStrokeType (widths[i], PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (paper.withAlpha (0.4f));
    g.fillPath (brandPath);

    // encre du titre « Wack'd / flangerus »
    g.setColour (Colour (0xff1b1a16));
    g.fillPath (brandPath);
}

//==============================================================================
void WackdAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu();
}

void WackdAudioProcessorEditor::showContextMenu()
{
    PopupMenu menu;

    PopupMenu sizes;
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    sizes.addItem (1, "Default (" + String (defaultW) + " x " + String (defaultH) + ")", true, getHeight() == defaultH);
    sizes.addItem (2, "75 %",  true, getHeight() == roundToInt (baseH * 0.75f));
    sizes.addItem (3, "100 %", true, getHeight() == baseH);
    menu.addSubMenu ("Window size", sizes);
    menu.addSeparator();

    PopupMenu factory;
    for (int i = 0; i < WackdAudioProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + WackdAudioProcessor::getFactoryPreset (i).name,
                         true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);

    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists,
                      proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    const bool cf7Stereo = proc.apvts.getRawParameterValue ("stereo")->load() > 0.5f;
    PopupMenu outMenu;
    outMenu.addItem (40, "Mono compatible (before amp sim)", true, ! cf7Stereo);
    outMenu.addItem (41, "CF7 stereo: OUT2 inverted (after amp sim)", true, cf7Stereo);
    menu.addSubMenu ("Output", outMenu);
    menu.addSeparator();

    PopupMenu midi;
    midi.addItem (900, "CC 20  Speed",                            false);
    midi.addItem (901, "CC 21  Depth",                            false);
    midi.addItem (902, "CC 22  Delay Time",                       false);
    midi.addItem (903, "CC 23  Regen.",                           false);
    midi.addItem (904, "CC 24  Mode (0-63 Chorus / 64+ Flanger)", false);
    midi.addItem (905, "CC 25  Krazy (0-63 Normal / 64+ Wack'd)", false);
    midi.addItem (906, "CC 26  Footswitch (64+ ON / 0-63 OFF)",   false);
    midi.addItem (907, "CC 27  Footswitch toggle (each press)",   false);
    midi.addItem (908, "Program Change 1-9  Factory presets",     false);
    menu.addSubMenu ("MIDI map", midi);

    menu.addSeparator();
    menu.addItem (999, "Wack'd flangerus v1.5.4 - STUDIO WIGA", false);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
        [safe = Component::SafePointer<WackdAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0)
                return;

            auto& ed = *safe;
            if      (r == 1) ed.setScaledHeight (defaultH);
            else if (r == 2) ed.setScaledHeight (roundToInt (baseH * 0.75f));
            else if (r == 3) ed.setScaledHeight (baseH);
            else if (r == 40 || r == 41)
            {
                if (auto* prm = ed.proc.apvts.getParameter ("stereo"))
                    prm->setValueNotifyingHost (r == 41 ? 1.0f : 0.0f);
            }
            else if (r >= 100 && r < 200) ed.proc.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  ed.proc.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  ed.proc.saveUserPreset (r - 300);

            ed.repaint();
        });
}
