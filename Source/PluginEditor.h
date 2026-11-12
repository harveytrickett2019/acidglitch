#pragma once
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg       { 0xff0b0d0a };
const juce::Colour panel    { 0xff141812 };
const juce::Colour line     { 0xff2a3124 };
const juce::Colour text     { 0xffd9e2cf };
const juce::Colour dim      { 0xff7c8870 };
const juce::Colour acid     { 0xffc6ff00 };
const juce::Colour glitch   { 0xffff3df0 };
const juce::Colour slide    { 0xff38e8ff };
const juce::Colour accent   { 0xffffb020 };

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}
}

class AcidLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AcidLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override           { return ui::font (12.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override     { return ui::font (13.0f, true); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return ui::font (12.5f, true); }
};

// A rotary knob with a caption, bound to a parameter.
struct Knob
{
    juce::Slider slider;
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts,
               const juce::String& paramId, const juce::String& name, juce::Colour colour);
    void setBounds (juce::Rectangle<int> area);
};

// 16-step editor: pitch bars on top, gate / accent / slide / glitch toggles below.
class StepGrid : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (AcidGlitchProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void handleMouse (const juce::MouseEvent&, bool isDown);
    juce::Rectangle<int> noteArea() const;
    juce::Rectangle<int> rowArea (int row) const;

    static constexpr int gutter = 62, rowH = 24, rowGap = 4;
    AcidGlitchProcessor& proc;
    int dragRow = -1;
    bool paintValue = false;
};

class AcidGlitchEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit AcidGlitchEditor (AcidGlitchProcessor&);
    ~AcidGlitchEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void slotClicked (int slot);
    void chooseFile (int slot);
    void loadInto (int slot, const juce::File& file);

    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void initCombo (juce::ComboBox&, std::unique_ptr<ComboAttachment>&, const juce::String& paramId);

    AcidGlitchProcessor& proc;
    AcidLookAndFeel lnf;

    Knob tune, wave, cutoff, reso, envmod, decay, accent, slide, drive, volume;
    Knob swing, length, density;
    Knob gchance, gmix, crush, down;

    juce::ComboBox rate, root, scale, gtype, source;
    std::unique_ptr<ComboAttachment> rateAtt, rootAtt, scaleAtt, gtypeAtt, sourceAtt;
    std::array<juce::TextButton, AcidGlitchProcessor::numSlots> slotButtons;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };
    juce::TextButton seqButton { "SEQ" }, randomise { "RANDOMISE" }, mutate { "MUTATE" }, clear { "CLEAR" };
    std::unique_ptr<ButtonAttachment> seqAtt;

    StepGrid grid;
    juce::Rectangle<int> sourcePanel, synthPanel, seqPanel, glitchPanel, gridPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidGlitchEditor)
};
