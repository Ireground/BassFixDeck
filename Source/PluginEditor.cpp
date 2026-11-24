#include "PluginEditor.h"

using namespace bfdui;

static constexpr double kMinHz = 25.0, kMaxHz = 420.0, kDbRange = 18.0;

static juce::String noteText (double hz)
{
    if (hz <= 0) return "--";
    const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
    return juce::String (bfd::noteName (midi)) + juce::String (midi / 12 - 1);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, accent);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f * scale);
    const float size = std::min (b.getWidth(), b.getHeight());
    b = b.withSizeKeepingCentre (size, size);
    const auto c = b.getCentre();
    const float r = size * 0.5f;
    const float thick = std::max (2.5f, r * 0.13f);
    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);

    // body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d3150), c.x, c.y - r,
                                             juce::Colour (0xff0e1828), c.x, c.y + r, false));
    g.fillEllipse (b.reduced (thick * 1.6f));
    g.setColour (panelEdge);
    g.drawEllipse (b.reduced (thick * 1.6f), 1.0f);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0, start, end, true);
    g.setColour (grid.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar knobs fill from the centre)
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float zeroAng = bipolar ? start + (end - start) * (float) s.valueToProportionOfLength (0.0) : start;
    const float ang = start + pos * (end - start);
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0, std::min (zeroAng, ang), std::max (zeroAng, ang), true);
    g.setColour (col.withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    const float pr = r - thick * 2.4f;
    juce::Point<float> p1 (c.x + pr * 0.35f * std::sin (ang), c.y - pr * 0.35f * std::cos (ang));
    juce::Point<float> p2 (c.x + pr * std::sin (ang), c.y - pr * std::cos (ang));
    g.setColour (text);
    g.drawLine ({ p1, p2 }, std::max (2.0f, thick * 0.6f));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto col = b.findColour (juce::TextButton::buttonOnColourId);
    g.setColour (on ? col.withAlpha (0.22f) : panel.brighter (over ? 0.15f : 0.0f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.3f);
    g.setColour (on ? col : panelEdge.brighter (over ? 0.3f : 0.0f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.3f, 1.2f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    g.setColour (on ? b.findColour (juce::TextButton::buttonOnColourId).brighter (0.3f) : textDim);
    g.setFont (juce::FontOptions (b.getHeight() * 0.42f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::textColourId, text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setFont (juce::FontOptions (12.5f * scale));
    return l;
}

//==============================================================================
juce::Rectangle<float> CurveDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f * scale, 10.0f * scale).withTrimmedBottom (14.0f * scale).withTrimmedLeft (22.0f * scale);
}
float CurveDisplay::xForHz (double hz) const
{
    auto a = plotArea();
    return a.getX() + a.getWidth() * (float) (std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz));
}
double CurveDisplay::hzForX (float x) const
{
    auto a = plotArea();
    return kMinHz * std::pow (kMaxHz / kMinHz, (double) ((x - a.getX()) / a.getWidth()));
}
float CurveDisplay::yForDb (double db) const
{
    auto a = plotArea();
    return a.getCentreY() - (float) (db / kDbRange) * a.getHeight() * 0.5f;
}
double CurveDisplay::dbForY (float y) const
{
    auto a = plotArea();
    return (double) ((a.getCentreY() - y) / (a.getHeight() * 0.5f)) * kDbRange;
}
juce::Point<float> CurveDisplay::boomHandle() const
{
    auto p = proc.getCurveParams();
    return { xForHz (p.boomNote * std::pow (2.0, -p.rangeSt / 12.0)), yForDb (-p.boomDepth) };
}
juce::Point<float> CurveDisplay::bodyHandle() const
{
    auto p = proc.getCurveParams();
    return { xForHz (p.bodyNote * std::pow (2.0, p.rangeSt / 12.0)), yForDb (p.bodyDepth) };
}

void CurveDisplay::setParam (const char* id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void CurveDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 8.0f * scale);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f * scale, 1.0f);

    const auto a = plotArea();
    const auto cp = proc.getCurveParams();
    const float fs = 11.5f * scale;

    // grid: every C and A, dB lines
    g.setFont (juce::FontOptions (fs));
    for (int midi = 24; midi <= 68; ++midi)
    {
        const double hz = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        if (hz < kMinHz || hz > kMaxHz) continue;
        const float x = xForHz (hz);
        const bool isC = midi % 12 == 0, isA = midi % 12 == 9;
        g.setColour (isC ? grid.brighter (0.35f) : grid.withAlpha (isA ? 0.9f : 0.45f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        if (isC || isA)
        {
            g.setColour (isC ? text.withAlpha (0.75f) : textDim);
            g.drawText (noteText (hz), juce::Rectangle<float> (x - 20 * scale, a.getBottom() + 1, 40 * scale, 14 * scale),
                        juce::Justification::centred);
        }
    }
    for (int db = -18; db <= 18; db += 6)
    {
        const float y = yForDb (db);
        g.setColour (db == 0 ? panelEdge.brighter (0.4f) : grid);
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (a.getX() - 24 * scale, y - 7 * scale, 21 * scale, 14 * scale),
                    juce::Justification::centredRight);
    }

    // threshold markers
    auto marker = [&] (double hz, juce::Colour c, const juce::String& label)
    {
        const float x = xForHz (hz);
        const float dash[] = { 4.0f * scale, 4.0f * scale };
        g.setColour (c.withAlpha (0.6f));
        g.drawDashedLine ({ x, a.getY(), x, a.getBottom() }, dash, 2, 1.2f);
        g.setFont (juce::FontOptions (fs, juce::Font::bold));
        g.drawText (label, juce::Rectangle<float> (x - 50 * scale, a.getY() + 2, 100 * scale, 14 * scale), juce::Justification::centred);
    };
    marker (cp.boomNote, cut, "BOOM < " + noteText (cp.boomNote));
    marker (cp.bodyNote, boost, "BODY > " + noteText (cp.bodyNote));

    // correction curve
    juce::Path line, cutFill, boostFill;
    const float zeroY = yForDb (0);
    const int steps = (int) a.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float x = a.getX() + (float) i;
        const double db = bfd::correctionDb (hzForX (x), cp);
        const float y = yForDb (db);
        if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }
    juce::Path fill (line);
    fill.lineTo (a.getRight(), zeroY);
    fill.lineTo (a.getX(), zeroY);
    fill.closeSubPath();
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (a.withBottom (zeroY).toNearestInt());
        g.setGradientFill (juce::ColourGradient (boost.withAlpha (0.35f), 0, a.getY(), boost.withAlpha (0.03f), 0, zeroY, false));
        g.fillPath (fill);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (a.withTop (zeroY).toNearestInt());
        g.setGradientFill (juce::ColourGradient (cut.withAlpha (0.03f), 0, zeroY, cut.withAlpha (0.35f), 0, a.getBottom(), false));
        g.fillPath (fill);
    }
    g.setColour (text.withAlpha (0.9f));
    g.strokePath (line, juce::PathStrokeType (2.0f * scale));

    // handles
    auto handle = [&] (juce::Point<float> p, juce::Colour c, bool hot)
    {
        const float r = (hot ? 8.5f : 7.0f) * scale;
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (p.x - r * 1.8f, p.y - r * 1.8f, r * 3.6f, r * 3.6f);
        g.setColour (c);
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
        g.setColour (bgBottom);
        g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 1.5f);
    };
    handle (boomHandle(), cut, hover == 1 || dragging == 1);
    handle (bodyHandle(), boost, hover == 2 || dragging == 2);

    // live note
    const float f0 = proc.uiPitch.load();
    const float gdb = proc.uiGainDb.load();
    if (f0 > 0)
    {
        const float x = xForHz (juce::jlimit ((double) kMinHz, (double) kMaxHz, (double) f0));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        const float y = yForDb (gdb);
        const auto c = gdb < -0.1f ? cut : (gdb > 0.1f ? boost : text);
        g.setColour (c.withAlpha (0.3f));
        g.fillEllipse (x - 12 * scale, y - 12 * scale, 24 * scale, 24 * scale);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x - 5 * scale, y - 5 * scale, 10 * scale, 10 * scale);
    }

    // readout
    auto ro = juce::Rectangle<float> (a.getX() + 8 * scale, a.getY() + 20 * scale, 190 * scale, 58 * scale);
    g.setColour (bgBottom.withAlpha (0.75f));
    g.fillRoundedRectangle (ro, 6 * scale);
    g.setColour (text);
    g.setFont (juce::FontOptions (22.0f * scale, juce::Font::bold));
    g.drawText (f0 > 0 ? noteText (f0) : "--", ro.withHeight (30 * scale).reduced (10 * scale, 0), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (13.0f * scale));
    g.setColour (textDim);
    g.drawText (f0 > 0 ? juce::String (f0, 1) + " Hz" : "no note", ro.withHeight (30 * scale).reduced (10 * scale, 0), juce::Justification::centredRight);
    const auto gc = gdb < -0.1f ? cut : (gdb > 0.1f ? boost : textDim);
    g.setColour (gc);
    g.setFont (juce::FontOptions (14.0f * scale, juce::Font::bold));
    juce::String status = (gdb > 0 ? "+" : "") + juce::String (gdb, 1) + " dB";
    status += gdb < -0.1f ? "  boom cut" : (gdb > 0.1f ? "  body boost" : "");
    g.drawText (status, ro.withTrimmedTop (30 * scale).reduced (10 * scale, 0), juce::Justification::centredLeft);
    const float hm = proc.uiHarm.load();
    if (hm > 0.01f)
    {
        g.setColour (harm);
        g.drawText ("+harm", ro.withTrimmedTop (30 * scale).reduced (10 * scale, 0), juce::Justification::centredRight);
    }
}

void CurveDisplay::mouseMove (const juce::MouseEvent& e)
{
    const float rr = 14.0f * scale;
    int h = 0;
    if (e.position.getDistanceFrom (boomHandle()) < rr) h = 1;
    else if (e.position.getDistanceFrom (bodyHandle()) < rr) h = 2;
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void CurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    mouseMove (e);
    dragging = hover;
    if (dragging == 1) { proc.apvts.getParameter ("boomNote")->beginChangeGesture(); proc.apvts.getParameter ("boomDepth")->beginChangeGesture(); }
    if (dragging == 2) { proc.apvts.getParameter ("bodyNote")->beginChangeGesture(); proc.apvts.getParameter ("bodyDepth")->beginChangeGesture(); }
}

void CurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const auto cp = proc.getCurveParams();
    const double hz = hzForX (e.position.x);
    const double db = dbForY (e.position.y);
    // The handle sits at the point of full correction (threshold +/- range),
    // so dragging it sideways moves the threshold, keeping the range.
    if (dragging == 1)
    {
        setParam ("boomNote", (float) juce::jlimit (30.0, 160.0, hz * std::pow (2.0, cp.rangeSt / 12.0)));
        setParam ("boomDepth", (float) juce::jlimit (0.0, 18.0, -db));
    }
    else
    {
        setParam ("bodyNote", (float) juce::jlimit (50.0, 400.0, hz * std::pow (2.0, -cp.rangeSt / 12.0)));
        setParam ("bodyDepth", (float) juce::jlimit (0.0, 18.0, db));
    }
    repaint();
}

void CurveDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging == 1) { proc.apvts.getParameter ("boomNote")->endChangeGesture(); proc.apvts.getParameter ("boomDepth")->endChangeGesture(); }
    if (dragging == 2) { proc.apvts.getParameter ("bodyNote")->endChangeGesture(); proc.apvts.getParameter ("bodyDepth")->endChangeGesture(); }
    dragging = 0;
}

void CurveDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    auto reset = [this] (const char* id)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
    };
    if (hover == 1) { reset ("boomNote"); reset ("boomDepth"); }
    if (hover == 2) { reset ("bodyNote"); reset ("bodyDepth"); }
    repaint();
}

//==============================================================================
void HistoryDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 8.0f * scale);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f * scale, 1.0f);

    auto a = bounds.reduced (12.0f * scale, 8.0f * scale).withTrimmedLeft (24.0f * scale);
    const double lo = 25.0, hi = 420.0;
    auto yFor = [&] (double hz) { return a.getBottom() - a.getHeight() * (float) (std::log (hz / lo) / std::log (hi / lo)); };
    const auto cp = proc.getCurveParams();

    g.setFont (juce::FontOptions (10.5f * scale));
    for (int oct = 1; oct <= 4; ++oct)
    {
        const double hz = 32.703 * std::pow (2.0, oct - 1);
        g.setColour (grid);
        g.drawHorizontalLine ((int) yFor (hz), a.getX(), a.getRight());
        g.setColour (textDim);
        g.drawText ("C" + juce::String (oct), juce::Rectangle<float> (a.getX() - 24 * scale, yFor (hz) - 6 * scale, 20 * scale, 12 * scale),
                    juce::Justification::centredRight);
    }
    const float dash[] = { 3.0f * scale, 4.0f * scale };
    g.setColour (cut.withAlpha (0.55f));
    g.drawDashedLine ({ a.getX(), yFor (cp.boomNote), a.getRight(), yFor (cp.boomNote) }, dash, 2, 1.0f);
    g.setColour (boost.withAlpha (0.55f));
    g.drawDashedLine ({ a.getX(), yFor (cp.bodyNote), a.getRight(), yFor (cp.bodyNote) }, dash, 2, 1.0f);

    const float dx = a.getWidth() / (float) (N - 1);
    for (int i = 1; i < N; ++i)
    {
        const int i0 = (pos + i - 1) % N, i1 = (pos + i) % N;
        const float p0 = pitch[(size_t) i0], p1 = pitch[(size_t) i1];
        if (p0 <= 0 || p1 <= 0) continue;
        const float gdb = gain[(size_t) i1];
        const float amt = juce::jlimit (0.0f, 1.0f, std::abs (gdb) / 9.0f);
        const auto c = gdb < 0 ? textDim.interpolatedWith (cut, amt) : textDim.interpolatedWith (boost, amt);
        g.setColour (c);
        const float x0 = a.getX() + dx * (float) (i - 1), x1 = x0 + dx;
        const float y0 = yFor (juce::jlimit (lo, hi, (double) p0)), y1 = yFor (juce::jlimit (lo, hi, (double) p1));
        if (std::abs (y1 - y0) > a.getHeight() * 0.08f)
            g.drawLine (x1, y1, x1, y1 + 0.01f, 3.0f * scale);
        else
            g.drawLine (x0, y0, x1, y1, 3.0f * scale);
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f * scale, juce::Font::bold));
    g.drawText ("NOTE HISTORY", a.reduced (4 * scale), juce::Justification::topRight);
}

//==============================================================================
BassFixDeckEditor::BassFixDeckEditor (BassFixDeckProcessor& p)
    : AudioProcessorEditor (p), proc (p), curve (p), history (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (curve);
    addAndMakeVisible (history);

    struct Def { const char* id; const char* name; juce::Colour col; const char* group; };
    const Def defs[] = {
        { "boomNote",  "BOOM BELOW", cut,    "BOOM" },
        { "boomDepth", "BOOM CUT",   cut,    "BOOM" },
        { "bodyNote",  "BODY ABOVE", boost,  "BODY" },
        { "bodyDepth", "BODY BOOST", boost,  "BODY" },
        { "range",     "RANGE",      accent, "SHAPE" },
        { "focus",     "FOCUS",      accent, "SHAPE" },
        { "harm",      "HARMONICS",  harm,   "SHAPE" },
        { "speed",     "SPEED",      accent, "OUTPUT" },
        { "mix",       "MIX",        accent, "OUTPUT" },
        { "out",       "OUTPUT",     accent, "OUTPUT" },
    };

    for (auto& d : defs)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, d.col);
        k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, k->slider);
        k->label.setText (d.name, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setColour (juce::Label::textColourId, textDim);
        k->group = d.group;
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        knobs.push_back (std::move (k));
    }
    // note knobs show the note name, not just Hz
    for (int i : { 0, 2 })
    {
        auto& s = knobs[(size_t) i]->slider;
        s.textFromValueFunction = [] (double v) { return noteText (v) + "  " + juce::String (v, 1) + " Hz"; };
        s.updateText();
    }

    for (auto* b : { &lookaheadBtn, &deltaBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    lookaheadBtn.setColour (juce::TextButton::buttonOnColourId, accent);
    deltaBtn.setColour (juce::TextButton::buttonOnColourId, harm);
    lookaheadBtn.setTooltip ("Delays the audio 50 ms so corrections land exactly on each new note (latency is compensated by REAPER).");
    deltaBtn.setTooltip ("Listen only to what BassFixDeck is adding or removing.");
    lookAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "lookahead", lookaheadBtn);
    deltaAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "delta", deltaBtn);

    // Read the remembered size BEFORE anything can resize the window:
    // setResizable/setResizeLimits trigger resized() with a temporary size,
    // which used to overwrite the saved one.
    const int savedW = juce::jlimit (640, 2000, proc.editorWidth.load());
    const int savedH = juce::jlimit (400, 1250, proc.editorHeight.load());
    setResizable (true, true);
    setResizeLimits (640, 400, 2000, 1250);
    setSize (savedW, savedH);
    sizeRestored = true;
    startTimerHz (30);
}

BassFixDeckEditor::~BassFixDeckEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BassFixDeckEditor::timerCallback()
{
    history.push (proc.uiPitch.load(), proc.uiGainDb.load());
    curve.repaint();
    history.repaint();
}

void BassFixDeckEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0, 0, bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    const float s = scale;
    auto header = getLocalBounds().removeFromTop ((int) (46 * s)).reduced ((int) (16 * s), 0);
    g.setColour (text);
    g.setFont (juce::FontOptions (22.0f * s, juce::Font::bold));
    g.drawText ("BassFix", header, juce::Justification::centredLeft);
    const int w1 = juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (22.0f * s, juce::Font::bold)), "BassFix");
    g.setColour (accent);
    g.drawText ("Deck", header.withTrimmedLeft (w1), juce::Justification::centredLeft);
    const int w2 = juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (22.0f * s, juce::Font::bold)), "Deck");
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f * s));
    g.drawText ("pitch-tracking bass balancer", header.withTrimmedLeft (w1 + w2 + (int) (12 * s)), juce::Justification::centredLeft);

    // knob panel + group captions
    g.setColour (panel);
    g.fillRoundedRectangle (knobPanel.toFloat(), 8.0f * s);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (knobPanel.toFloat().reduced (0.5f), 8.0f * s, 1.0f);

    juce::String lastGroup;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        if (knobs[i]->group == lastGroup) continue;
        lastGroup = knobs[i]->group;
        size_t j = i;
        while (j + 1 < knobs.size() && knobs[j + 1]->group == lastGroup) ++j;
        const int x0 = knobs[i]->slider.getX(), x1 = knobs[j]->slider.getRight();
        const int y = knobPanel.getY() + (int) (6 * s);
        const auto col = lastGroup == "BOOM" ? cut : lastGroup == "BODY" ? boost : textDim;
        g.setColour (col.withAlpha (0.8f));
        g.setFont (juce::FontOptions (10.5f * s, juce::Font::bold));
        g.drawText (lastGroup, juce::Rectangle<int> (x0, y, x1 - x0, (int) (12 * s)), juce::Justification::centred);
        g.setColour (col.withAlpha (0.35f));
        g.drawHorizontalLine (y + (int) (16 * s), (float) x0 + 8 * s, (float) x1 - 8 * s);
    }
}

void BassFixDeckEditor::resized()
{
    if (sizeRestored)
    {
        proc.editorWidth.store (getWidth());
        proc.editorHeight.store (getHeight());
    }

    scale = std::min (getWidth() / 860.0f, getHeight() / 540.0f);
    lnf.scale = scale;
    curve.scale = history.scale = scale;
    const float s = scale;

    auto r = getLocalBounds();
    auto header = r.removeFromTop ((int) (46 * s)).reduced ((int) (16 * s), (int) (10 * s));
    deltaBtn.setBounds (header.removeFromRight ((int) (84 * s)));
    header.removeFromRight ((int) (8 * s));
    lookaheadBtn.setBounds (header.removeFromRight ((int) (110 * s)));

    r.reduce ((int) (12 * s), 0);
    r.removeFromBottom ((int) (12 * s));
    knobPanel = r.removeFromBottom ((int) (150 * s));
    r.removeFromBottom ((int) (10 * s));
    auto histArea = r.removeFromBottom ((int) (r.getHeight() * 0.28f));
    r.removeFromBottom ((int) (8 * s));
    curve.setBounds (r);
    history.setBounds (histArea);

    auto kp = knobPanel.reduced ((int) (8 * s), 0);
    kp.removeFromTop ((int) (26 * s));
    kp.removeFromBottom ((int) (8 * s));
    const int kw = kp.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto cell = kp.removeFromLeft (kw);
        k->label.setFont (juce::FontOptions (10.5f * s, juce::Font::bold));
        k->label.setBounds (cell.removeFromTop ((int) (16 * s)));
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kw, (int) (18 * s));
        k->slider.setBounds (cell);
        k->slider.sendLookAndFeelChange();
    }
}
