#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

static constexpr double kLookaheadMs = 50.0;
static constexpr double kHoldSeconds = 0.18;
static constexpr int    kControlInterval = 16;

//==============================================================================
static juce::NormalisableRange<float> hzRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi,
        [] (float a, float b, float v) { return a * std::pow (b / a, v); },
        [] (float a, float b, float x) { return std::log (x / a) / std::log (b / a); });
    r.interval = 0.0f;
    return r;
}

static juce::String hzToText (float hz, int)
{
    const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
    return juce::String (bfd::noteName (midi)) + juce::String (midi / 12 - 1) + "  " + juce::String (hz, 1) + " Hz";
}

APVTS::ParameterLayout BassFixDeckProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    auto hzAttr = AudioParameterFloatAttributes().withStringFromValueFunction (hzToText)
                                                 .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });
    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB");

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "boomNote", 1 }, "Boom Below", hzRange (30.0f, 160.0f), 55.0f, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "boomDepth", 1 }, "Boom Cut", NormalisableRange<float> (0.0f, 18.0f, 0.1f), 6.0f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "bodyNote", 1 }, "Body Above", hzRange (50.0f, 400.0f), 98.0f, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "bodyDepth", 1 }, "Body Boost", NormalisableRange<float> (0.0f, 18.0f, 0.1f), 6.0f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "range", 1 }, "Range", NormalisableRange<float> (1.0f, 24.0f, 0.1f), 12.0f,
                                                  AudioParameterFloatAttributes().withLabel ("st")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "focus", 1 }, "Focus", NormalisableRange<float> (0.3f, 4.0f, 0.01f, 0.5f), 1.0f,
                                                  AudioParameterFloatAttributes().withLabel ("Q")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "harm", 1 }, "Harmonics", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Speed", NormalisableRange<float> (2.0f, 300.0f, 0.1f, 0.4f), 20.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, dbAttr));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "lookahead", 1 }, "Lookahead", true));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "delta", 1 }, "Delta Listen", false));
    return l;
}

BassFixDeckProcessor::BassFixDeckProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pBoomNote  = apvts.getRawParameterValue ("boomNote");
    pBoomDepth = apvts.getRawParameterValue ("boomDepth");
    pBodyNote  = apvts.getRawParameterValue ("bodyNote");
    pBodyDepth = apvts.getRawParameterValue ("bodyDepth");
    pRange     = apvts.getRawParameterValue ("range");
    pFocus     = apvts.getRawParameterValue ("focus");
    pHarm      = apvts.getRawParameterValue ("harm");
    pSpeed     = apvts.getRawParameterValue ("speed");
    pMix       = apvts.getRawParameterValue ("mix");
    pOut       = apvts.getRawParameterValue ("out");
    pLookahead = apvts.getRawParameterValue ("lookahead");
    pDelta     = apvts.getRawParameterValue ("delta");
}

bool BassFixDeckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

bfd::CurveParams BassFixDeckProcessor::getCurveParams() const
{
    bfd::CurveParams p;
    p.boomNote  = pBoomNote->load();
    p.boomDepth = pBoomDepth->load();
    p.bodyNote  = pBodyNote->load();
    p.bodyDepth = pBodyDepth->load();
    p.rangeSt   = pRange->load();
    return p;
}

void BassFixDeckProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    tracker.prepare (sr);
    lookaheadSamples = (int) std::lround (kLookaheadMs * 0.001 * sr);

    for (auto& c : ch)
    {
        c.eq.reset(); c.harmBand.reset(); c.harmHp1.reset(); c.harmHp2.reset();
        c.env = 0;
        c.delay.assign ((size_t) std::max (1, lookaheadSamples), 0.0f);
        c.delayPos = 0;
    }

    heldPitch = smoothPitch = 0;
    silentTime = 1.0;
    gainDbSmoothed = harmSmoothed = 0;
    controlCounter = 0;
    outGainSmoothed = juce::Decibels::decibelsToGain (pOut->load());
    mixSmoothed = pMix->load() * 0.01f;
    lastLookahead = pLookahead->load() > 0.5f;
    setLatencySamples (lastLookahead ? lookaheadSamples : 0);
    updateControl (0);
}

void BassFixDeckProcessor::updateControl (int numSamples)
{
    const double dt = numSamples / sr;
    const auto cp = getCurveParams();

    if (tracker.takeNewResult())
    {
        const double p = tracker.getPitch();
        if (p > 0 && tracker.getConfidence() > 0.55)
        {
            if (smoothPitch <= 0 || std::abs (12.0 * std::log2 (p / smoothPitch)) > 1.0)
                smoothPitch = p;           // new note: jump straight to it
            heldPitch = p;
            silentTime = 0;
        }
    }
    silentTime += dt;

    // small glides / vibrato are followed smoothly
    if (heldPitch > 0 && smoothPitch > 0 && dt > 0)
    {
        const double a = 1.0 - std::exp (-dt / 0.012);
        smoothPitch = std::exp (std::log (smoothPitch) + a * (std::log (heldPitch) - std::log (smoothPitch)));
    }

    const bool active = smoothPitch > 0 && silentTime < kHoldSeconds;
    const double targetDb   = active ? bfd::correctionDb (smoothPitch, cp) : 0.0;
    const double targetHarm = active ? (pHarm->load() * 0.01) * bfd::boomAmount (smoothPitch, cp) : 0.0;

    if (dt > 0)
    {
        const double tc = std::max (0.001, pSpeed->load() * 0.001);
        const double a = 1.0 - std::exp (-dt / tc);
        gainDbSmoothed += a * (targetDb - gainDbSmoothed);
        harmSmoothed   += a * (targetHarm - harmSmoothed);
    }
    else
    {
        gainDbSmoothed = targetDb; harmSmoothed = targetHarm;
    }

    const double f = smoothPitch > 0 ? smoothPitch : 60.0;
    const double q = pFocus->load();
    for (auto& c : ch)
    {
        c.eq.setBell (sr, f, q, gainDbSmoothed);
        c.harmBand.setBand (sr, f, 1.2);
        c.harmHp1.setBand (sr, f * 1.7, 0.707);
        c.harmHp2.setBand (sr, f * 1.7, 0.707);
    }

    uiPitch.store (active ? (float) smoothPitch : 0.0f);
    uiGainDb.store ((float) gainDbSmoothed);
    uiHarm.store ((float) harmSmoothed);
    uiConfidence.store ((float) tracker.getConfidence());
}

void BassFixDeckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (2, getTotalNumInputChannels());
    const int n = buffer.getNumSamples();
    for (int c = numCh; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (numCh == 0) return;

    const bool lookahead = pLookahead->load() > 0.5f;
    if (lookahead != lastLookahead)
    {
        lastLookahead = lookahead;
        setLatencySamples (lookahead ? lookaheadSamples : 0);
    }
    const bool delta = pDelta->load() > 0.5f;
    const float outTarget = juce::Decibels::decibelsToGain (pOut->load());
    const float mixTarget = pMix->load() * 0.01f;
    const float rampA = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));

    const double envAtt = 1.0 - std::exp (-1.0 / (0.004 * sr));
    const double envRel = 1.0 - std::exp (-1.0 / (0.060 * sr));

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        double mono = data[0][i];
        if (numCh > 1) mono = 0.5 * (mono + data[1][i]);
        tracker.push (mono);

        if (++controlCounter >= kControlInterval)
        {
            controlCounter = 0;
            updateControl (kControlInterval);
        }

        outGainSmoothed += rampA * (outTarget - outGainSmoothed);
        mixSmoothed     += rampA * (mixTarget - mixSmoothed);

        for (int c = 0; c < numCh; ++c)
        {
            auto& s = ch[(size_t) c];
            float in = data[c][i];

            if (lookahead && lookaheadSamples > 0)
            {
                const float delayed = s.delay[(size_t) s.delayPos];
                s.delay[(size_t) s.delayPos] = in;
                s.delayPos = (s.delayPos + 1) % (int) s.delay.size();
                // buffer holds exactly lookaheadSamples slots -> exact reported latency
                in = delayed;
            }

            double wet = s.eq.bell (in);

            // Harmonics: rebuild the cut low end as upper harmonics so low notes
            // stay audible on small speakers (Renaissance Bass style).
            const double bp = s.harmBand.band (in);
            const double ab = std::abs (bp);
            s.env += (ab > s.env ? envAtt : envRel) * (ab - s.env);
            if (harmSmoothed > 1e-4)
            {
                const double nrm = std::clamp (bp / (s.env * 1.6 + 1e-9), -1.5, 1.5);
                const double shaped = 0.55 * nrm * nrm + 0.45 * std::tanh (2.5 * nrm) * std::abs (nrm);
                double h = shaped * s.env;
                h = s.harmHp2.high (s.harmHp1.high (h));
                wet += harmSmoothed * 2.2 * h;
            }
            else
            {
                s.harmHp2.high (s.harmHp1.high (0.0));
            }

            const double mixed = in + mixSmoothed * (wet - in);
            const double out = delta ? (mixed - in) : mixed;
            data[c][i] = (float) (out * outGainSmoothed);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* BassFixDeckProcessor::createEditor()
{
    return new BassFixDeckEditor (*this);
}

void BassFixDeckProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("editorW", editorWidth.load(), nullptr);
    state.setProperty ("editorH", editorHeight.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void BassFixDeckProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType (apvts.state.getType()))
        {
            editorWidth.store ((int) tree.getProperty ("editorW", editorWidth.load()));
            editorHeight.store ((int) tree.getProperty ("editorH", editorHeight.load()));
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassFixDeckProcessor();
}
