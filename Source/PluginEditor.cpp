#include "PluginEditor.h"

//==============================================================================
AcidLookAndFeel::AcidLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::Label::textColourId, ui::dim);
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::acid.withAlpha (0.35f));
    setColour (juce::ComboBox::backgroundColourId, ui::bg);
    setColour (juce::ComboBox::outlineColourId, ui::line);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::arrowColourId, ui::dim);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::acid);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::bg);
    setColour (juce::TextButton::buttonColourId, ui::bg);
    setColour (juce::TextButton::buttonOnColourId, ui::acid);
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::TextButton::textColourOnId, ui::bg);
    setColour (juce::ComboBox::buttonColourId, ui::bg);
}

void AcidLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto colour = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const juce::PathStrokeType stroke (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (ui::line);
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (colour);
    g.strokePath (value, stroke);

    const float body = radius - 7.0f;
    g.setColour (juce::Colour (0xff1d231a));
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour (ui::line);
    g.drawEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -body + 3.0f, 3.0f, body * 0.55f, 1.5f);
    g.setColour (colour);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

//==============================================================================
void Knob::init (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramId, const juce::String& name, juce::Colour colour)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    parent.addAndMakeVisible (slider);

    caption.setText (name, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (caption);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

void Knob::setBounds (juce::Rectangle<int> area)
{
    caption.setBounds (area.removeFromTop (16));
    slider.setBounds (area);
}

//==============================================================================
StepGrid::StepGrid (AcidGlitchProcessor& p) : proc (p)
{
    startTimerHz (30);
}

juce::Rectangle<int> StepGrid::noteArea() const
{
    return getLocalBounds().withTrimmedLeft (gutter).withTrimmedBottom (4 * (rowH + rowGap));
}

juce::Rectangle<int> StepGrid::rowArea (int row) const
{
    const int top = getHeight() - (4 - row) * (rowH + rowGap) + rowGap;
    return { gutter, top, getWidth() - gutter, rowH };
}

void StepGrid::paint (juce::Graphics& g)
{
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* rowNames[]  = { "GATE", "ACCENT", "SLIDE", "GLITCH" };
    const juce::Colour rowColours[] = { ui::acid, ui::accent, ui::slide, ui::glitch };

    const int root   = (int) proc.apvts.getRawParameterValue ("root")->load();
    const int length = (int) proc.apvts.getRawParameterValue ("length")->load();
    const int play   = proc.engine.uiStep.load();
    const auto notes = noteArea();
    const float colW = (float) notes.getWidth() / (float) acid::kMaxSteps;

    g.setFont (ui::font (10.5f, true));
    g.setColour (ui::dim);
    g.drawText ("NOTE", 0, notes.getY(), gutter - 8, 16, juce::Justification::centredRight);
    for (int r = 0; r < 4; ++r)
        g.drawText (rowNames[r], 0, rowArea (r).getY(), gutter - 8, rowH, juce::Justification::centredRight);

    for (int i = 0; i < acid::kMaxSteps; ++i)
    {
        const acid::Step s = proc.engine.pattern.get (i);
        const float alpha = i < length ? 1.0f : 0.25f;
        const auto col = juce::Rectangle<float> ((float) notes.getX() + colW * (float) i, (float) notes.getY(),
                                                 colW, (float) notes.getHeight()).reduced (2.0f, 0.0f);

        // column background, brighter on each beat and on the playhead
        g.setColour (i == play ? ui::acid.withAlpha (0.16f)
                               : (i % 4 == 0 ? juce::Colour (0xff1b2117) : juce::Colour (0xff11150f)));
        g.fillRoundedRectangle (col, 3.0f);

        // pitch bar
        const float norm = (float) (s.semi - acid::kMinSemi) / (float) (acid::kMaxSemi - acid::kMinSemi);
        const float barH = 12.0f + norm * (col.getHeight() - 32.0f);
        const auto bar = col.withTop (col.getBottom() - barH).reduced (3.0f, 3.0f);
        const auto barColour = s.gate ? (s.accent ? ui::acid : ui::acid.withMultipliedSaturation (0.75f).darker (0.35f))
                                      : juce::Colour (0xff2c3327);
        g.setColour (barColour.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (bar, 2.0f);

        g.setColour ((s.gate ? ui::text : ui::dim).withMultipliedAlpha (alpha));
        const int n = ((root + s.semi) % 12 + 12) % 12;
        const int octave = 2 + (int) std::floor ((float) (root + s.semi) / 12.0f);
        g.drawText (juce::String (noteNames[n]) + juce::String (octave), col.withHeight (18.0f).toNearestInt(),
                    juce::Justification::centred);

        // toggle rows
        const bool on[] = { s.gate, s.accent, s.slide, s.glitch };
        for (int r = 0; r < 4; ++r)
        {
            const auto cell = rowArea (r).toFloat().withX ((float) gutter + colW * (float) i).withWidth (colW).reduced (2.0f, 0.0f);
            g.setColour (on[r] ? rowColours[r].withMultipliedAlpha (alpha) : juce::Colour (0xff171c14));
            g.fillRoundedRectangle (cell, 3.0f);
            if (! on[r])
            {
                g.setColour (ui::line);
                g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.0f);
            }
        }
    }
}

void StepGrid::handleMouse (const juce::MouseEvent& e, bool isDown)
{
    const auto notes = noteArea();
    const float colW = (float) notes.getWidth() / (float) acid::kMaxSteps;
    const int col = juce::jlimit (0, acid::kMaxSteps - 1, (int) ((float) (e.x - gutter) / colW));
    if (e.x < gutter && isDown) { dragRow = -2; return; }

    if (isDown)
    {
        dragRow = -1;   // -1 = note area
        for (int r = 0; r < 4; ++r)
            if (rowArea (r).expanded (0, rowGap / 2).contains (e.x, e.y))
                dragRow = r;
    }
    if (dragRow == -2) return;

    acid::Step s = proc.engine.pattern.get (col);
    if (dragRow == -1)
    {
        const float norm = 1.0f - (float) (e.y - notes.getY() - 20) / (float) juce::jmax (1, notes.getHeight() - 26);
        s.semi = juce::jlimit (acid::kMinSemi, acid::kMaxSemi,
                               acid::kMinSemi + juce::roundToInt (norm * (float) (acid::kMaxSemi - acid::kMinSemi)));
        s.gate = true;
    }
    else
    {
        bool* fields[] = { &s.gate, &s.accent, &s.slide, &s.glitch };
        if (isDown) paintValue = ! *fields[dragRow];
        *fields[dragRow] = paintValue;
    }
    proc.engine.pattern.set (col, s);
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e) { handleMouse (e, true); }
void StepGrid::mouseDrag (const juce::MouseEvent& e) { handleMouse (e, false); }

//==============================================================================
AcidGlitchEditor::AcidGlitchEditor (AcidGlitchProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p)
{
    setLookAndFeel (&lnf);
    auto& a = proc.apvts;

    tune.init    (*this, a, "tune",    "TUNE",     ui::acid);
    wave.init    (*this, a, "wave",    "SAW / SQ", ui::acid);
    cutoff.init  (*this, a, "cutoff",  "CUTOFF",   ui::acid);
    reso.init    (*this, a, "reso",    "RESO",     ui::acid);
    envmod.init  (*this, a, "envmod",  "ENV MOD",  ui::acid);
    decay.init   (*this, a, "decay",   "DECAY",    ui::acid);
    accent.init  (*this, a, "accent",  "ACCENT",   ui::accent);
    slide.init   (*this, a, "slide",   "SLIDE",    ui::slide);
    drive.init   (*this, a, "drive",   "DRIVE",    ui::acid);
    volume.init  (*this, a, "volume",  "VOLUME",   ui::text);

    swing.init   (*this, a, "swing",   "SWING",    ui::acid);
    length.init  (*this, a, "length",  "LENGTH",   ui::acid);
    density.init (*this, a, "density", "DENSITY",  ui::acid);

    gchance.init (*this, a, "gchance", "CHANCE",   ui::glitch);
    gmix.init    (*this, a, "gmix",    "MIX",      ui::glitch);
    crush.init   (*this, a, "crush",   "CRUSH",    ui::glitch);
    down.init    (*this, a, "down",    "DOWNSAMPLE", ui::glitch);

    initCombo (rate,  rateAtt,  "rate");
    initCombo (root,  rootAtt,  "root");
    initCombo (scale, scaleAtt, "scale");
    initCombo (gtype, gtypeAtt, "gtype");
    initCombo (source, sourceAtt, "source");

    for (int i = 0; i < AcidGlitchProcessor::numSlots; ++i)
    {
        auto& b = slotButtons[(size_t) i];
        b.setTooltip ("Click to load a bass sample (or drop a file here). The pattern's root step plays it at its original pitch.");
        b.onClick = [this, i] { slotClicked (i); };
        addAndMakeVisible (b);
    }

    seqButton.setClickingTogglesState (true);
    seqButton.setTooltip ("On: plays the pattern. Off: plays MIDI notes as a mono synth.");
    addAndMakeVisible (seqButton);
    seqAtt = std::make_unique<ButtonAttachment> (a, "seq", seqButton);

    for (auto* b : { &randomise, &mutate, &clear })
        addAndMakeVisible (*b);
    randomise.setColour (juce::TextButton::buttonColourId, ui::acid);
    randomise.setColour (juce::TextButton::textColourOffId, ui::bg);
    randomise.onClick = [this] { proc.randomisePattern(); grid.repaint(); };
    mutate.onClick    = [this] { proc.mutatePattern();    grid.repaint(); };
    clear.onClick     = [this] { proc.clearPattern();     grid.repaint(); };

    addAndMakeVisible (grid);
    setSize (900, 654);
    timerCallback();
    startTimerHz (8);
}

void AcidGlitchEditor::timerCallback()
{
    static const char* letters[] = { "A", "B", "C", "D" };
    const int current = (int) proc.apvts.getRawParameterValue ("source")->load();
    for (int i = 0; i < AcidGlitchProcessor::numSlots; ++i)
    {
        auto& b = slotButtons[(size_t) i];
        const auto name = proc.getSampleName (i);
        b.setButtonText (juce::String (letters[i]) + "   " + (name.isEmpty() ? juce::String ("click to load") : name));
        b.setToggleState (current == i + 1, juce::dontSendNotification);
    }
}

void AcidGlitchEditor::loadInto (int slot, const juce::File& file)
{
    if (proc.loadSample (slot, file))
        proc.selectSource (slot + 1);
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "AcidGlitch",
                                                "Couldn't read " + file.getFileName() + ". Try a WAV, AIFF, FLAC or MP3 file.");
    timerCallback();
}

void AcidGlitchEditor::chooseFile (int slot)
{
    chooser = std::make_unique<juce::FileChooser> ("Load a bass sample", juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, slot] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile()) loadInto (slot, file);
                          });
}

void AcidGlitchEditor::slotClicked (int slot)
{
    if (proc.getSampleName (slot).isEmpty()) { chooseFile (slot); return; }

    juce::PopupMenu menu;
    menu.addItem (1, "Use this sample");
    menu.addItem (2, "Load a different sample...");
    menu.addItem (3, "Remove");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slotButtons[(size_t) slot]),
                        [this, slot] (int choice)
                        {
                            if (choice == 1) proc.selectSource (slot + 1);
                            if (choice == 2) chooseFile (slot);
                            if (choice == 3)
                            {
                                proc.clearSample (slot);
                                if ((int) proc.apvts.getRawParameterValue ("source")->load() == slot + 1)
                                    proc.selectSource (0);
                            }
                            timerCallback();
                        });
}

void AcidGlitchEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (files.isEmpty()) return;
    int slot = -1;
    for (int i = 0; i < AcidGlitchProcessor::numSlots; ++i)
        if (slotButtons[(size_t) i].getBounds().contains (x, y))
            slot = i;
    if (slot < 0)   // dropped elsewhere: first empty slot, else slot A
    {
        slot = 0;
        for (int i = AcidGlitchProcessor::numSlots - 1; i >= 0; --i)
            if (proc.getSampleName (i).isEmpty())
                slot = i;
    }
    loadInto (slot, juce::File (files[0]));
}

AcidGlitchEditor::~AcidGlitchEditor()
{
    setLookAndFeel (nullptr);
}

void AcidGlitchEditor::initCombo (juce::ComboBox& box, std::unique_ptr<ComboAttachment>& att, const juce::String& paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<ComboAttachment> (proc.apvts, paramId, box);
}

void AcidGlitchEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);

    g.setColour (ui::acid);
    g.setFont (ui::font (30.0f, true));
    g.drawText ("ACID", 18, 10, 80, 36, juce::Justification::centredLeft);
    g.setColour (ui::glitch);
    g.drawText ("GLITCH", 92, 10, 140, 36, juce::Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (ui::font (11.5f));
    g.drawText ("Press play in your DAW, or hold a key to audition the pattern", 18, 40, 420, 16,
                juce::Justification::centredLeft);

    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title, juce::Colour colour)
    {
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (ui::line);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (colour);
        g.setFont (ui::font (11.0f, true));
        g.drawText (title, r.getX() + 12, r.getY() + 6, 200, 14, juce::Justification::centredLeft);
    };
    drawPanel (sourcePanel, {},         ui::acid);
    g.setColour (ui::acid);
    g.setFont (ui::font (11.0f, true));
    g.drawText ("SOURCE", sourcePanel.getX() + 12, sourcePanel.getY(), 60, sourcePanel.getHeight(), juce::Justification::centredLeft);
    drawPanel (synthPanel,  "SYNTH",    ui::acid);
    drawPanel (seqPanel,    "SEQUENCE", ui::acid);
    drawPanel (glitchPanel, "GLITCH",   ui::glitch);
    drawPanel (gridPanel,   "PATTERN",  ui::acid);

    g.setColour (ui::dim);
    g.setFont (ui::font (10.5f, true));
    g.drawText ("RATE",  rate.getX(),  rate.getY() - 14,  rate.getWidth(),  12, juce::Justification::centredLeft);
    g.drawText ("ROOT",  root.getX(),  root.getY() - 14,  root.getWidth(),  12, juce::Justification::centredLeft);
    g.drawText ("SCALE", scale.getX(), scale.getY() - 14, scale.getWidth(), 12, juce::Justification::centredLeft);
    g.drawText ("TYPE",  gtype.getX(), gtype.getY() - 14, gtype.getWidth(), 12, juce::Justification::centredLeft);
}

void AcidGlitchEditor::resized()
{
    const int margin = 12, knobW = 84, knobH = 96;
    auto area = getLocalBounds().reduced (margin);

    // header: combos on the right
    auto header = area.removeFromTop (50);
    auto controls = header.removeFromRight (400).withTrimmedTop (18).withHeight (26);
    seqButton.setBounds (controls.removeFromLeft (58));
    controls.removeFromLeft (10);
    rate.setBounds (controls.removeFromLeft (84));
    controls.removeFromLeft (10);
    root.setBounds (controls.removeFromLeft (70));
    controls.removeFromLeft (10);
    scale.setBounds (controls);

    area.removeFromTop (8);
    sourcePanel = area.removeFromTop (46);
    {
        auto row = sourcePanel.reduced (10, 10).withTrimmedLeft (58);
        source.setBounds (row.removeFromLeft (124));
        const int w = (row.getWidth() - 4 * 8) / AcidGlitchProcessor::numSlots;
        for (auto& b : slotButtons)
        {
            row.removeFromLeft (8);
            b.setBounds (row.removeFromLeft (w));
        }
    }

    area.removeFromTop (8);
    synthPanel = area.removeFromTop (128);
    {
        auto row = synthPanel.reduced (8, 0).withTrimmedTop (24).withHeight (knobH);
        const int w = row.getWidth() / 10;
        for (auto* k : { &tune, &wave, &cutoff, &reso, &envmod, &decay, &accent, &slide, &drive, &volume })
            k->setBounds (row.removeFromLeft (w));
    }

    area.removeFromTop (8);
    auto second = area.removeFromTop (128);
    seqPanel = second.removeFromLeft (430);
    second.removeFromLeft (8);
    glitchPanel = second;
    {
        auto row = seqPanel.reduced (8, 0).withTrimmedTop (24).withHeight (knobH);
        for (auto* k : { &swing, &length, &density })
            k->setBounds (row.removeFromLeft (knobW));
        row.removeFromLeft (10);
        auto buttons = row.reduced (0, 2);
        randomise.setBounds (buttons.removeFromTop (34));
        buttons.removeFromTop (6);
        mutate.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 3).withHeight (28));
        buttons.removeFromLeft (6);
        clear.setBounds (buttons.withHeight (28));
    }
    {
        auto row = glitchPanel.reduced (8, 0).withTrimmedTop (24).withHeight (knobH);
        gtype.setBounds (row.removeFromLeft (112).withTrimmedTop (30).withHeight (26).reduced (4, 0));
        const int w = row.getWidth() / 4;
        for (auto* k : { &gchance, &gmix, &crush, &down })
            k->setBounds (row.removeFromLeft (w));
    }

    area.removeFromTop (8);
    gridPanel = area;
    grid.setBounds (gridPanel.reduced (10).withTrimmedTop (16));
}
