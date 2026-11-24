#pragma once
#include "PluginProcessor.h"

namespace bfdui
{
// StageDeck-style dark blue palette
const juce::Colour bgTop    { 0xff0f1a2c };
const juce::Colour bgBottom { 0xff09111d };
const juce::Colour panel    { 0xff122036 };
const juce::Colour panelEdge{ 0xff22385a };
const juce::Colour grid     { 0xff1b2d48 };
const juce::Colour text     { 0xffcfdced };
const juce::Colour textDim  { 0xff6f86a6 };
const juce::Colour accent   { 0xff3fa7ff };
const juce::Colour cut      { 0xffff8a3d };
const juce::Colour boost    { 0xff38e0c0 };
const juce::Colour harm     { 0xffb48cff };

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    float scale = 1.0f;
};
}

//==============================================================================
class CurveDisplay : public juce::Component
{
public:
    explicit CurveDisplay (BassFixDeckProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    float scale = 1.0f;

private:
    juce::Rectangle<float> plotArea() const;
    float xForHz (double hz) const;
    double hzForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
    juce::Point<float> boomHandle() const;
    juce::Point<float> bodyHandle() const;
    void setParam (const char* id, float value);

    BassFixDeckProcessor& proc;
    int dragging = 0, hover = 0;   // 1 = boom, 2 = body
};

//==============================================================================
class HistoryDisplay : public juce::Component
{
public:
    explicit HistoryDisplay (BassFixDeckProcessor& p) : proc (p) { pitch.fill (0); gain.fill (0); }
    void push (float hz, float db) { pitch[(size_t) pos] = hz; gain[(size_t) pos] = db; pos = (pos + 1) % N; }
    void paint (juce::Graphics&) override;
    float scale = 1.0f;

private:
    static constexpr int N = 240;
    BassFixDeckProcessor& proc;
    std::array<float, N> pitch, gain;
    int pos = 0;
};

//==============================================================================
class BassFixDeckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BassFixDeckEditor (BassFixDeckProcessor&);
    ~BassFixDeckEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String group;
    };

    BassFixDeckProcessor& proc;
    bfdui::LookAndFeel lnf;
    CurveDisplay curve;
    HistoryDisplay history;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::TextButton lookaheadBtn { "LOOKAHEAD" }, deltaBtn { "DELTA" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lookAtt, deltaAtt;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Rectangle<int> knobPanel;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassFixDeckEditor)
};
