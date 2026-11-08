#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TapeSynth.h"

using namespace ml;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MelloAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto addF = [&] (const String& id, const String& name, float lo, float hi, float step, float skew, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step, skew), def));
    };

    // envolvente de amplitud
    addF ("attack",  "Attack",  0.001f, 2.f,  0.001f, 0.4f, 0.02f);
    addF ("decay",   "Decay",   0.01f,  3.f,  0.001f, 0.5f, 0.5f);
    addF ("sustain", "Sustain", 0.f,    100.f, 0.1f, 1.f, 100.f);
    addF ("release", "Release", 0.01f,  4.f,  0.001f, 0.4f, 0.25f);
    // cinta
    addF ("flutter",   "Flutter",         0.f, 100.f, 0.1f, 1.f, 25.f);
    addF ("satur",     "Tape Saturation", 0.f, 100.f, 0.1f, 1.f, 20.f);
    addF ("mech",      "Mechanics",       0.f, 100.f, 0.1f, 1.f, 30.f);
    addF ("noise",     "Noise Floor",     0.f, 100.f, 0.1f, 1.f, 15.f);
    addF ("velvol",    "Vel > Volume",    0.f, 100.f, 0.1f, 1.f, 70.f);
    addF ("atflutter", "AT > Flutter",    0.f, 100.f, 0.1f, 1.f, 50.f);
    // salida
    addF ("volume", "Volume", 0.f, 100.f, 0.1f, 1.f, 80.f);
    addF ("tone",   "Tone",   0.f, 100.f, 0.1f, 1.f, 70.f);
    addF ("pitch",  "Pitch",  -200.f, 200.f, 0.1f, 1.f, 0.f);          // centesimas de tono
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Power", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bank", 1 }, "Tape Bank", StringArray { "A", "B", "C", "ALL" }, 0));
    // avanzado
    addF ("tapelen", "Tape Length", 2.f, 30.f, 0.1f, 1.f, 8.f);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "loop", 1 }, "Loop Tape End", false));
    for (int i = 0; i < kBanks; ++i)
    {
        l.add (std::make_unique<AudioParameterInt> (ParameterID { "root" + String (i), 1 }, "Root " + String ("ABC").substring (i, i + 1), 24, 96, 60));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "fxbyp" + String (i), 1 }, "Bypass " + String (i + 1), false));
    }
    return l;
}

MelloAudioProcessor::MelloAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& p : pendingRoot) p.store (-1);
    engine.setBank (&bank);
    startTimer (1000);
    rack.rescan();

    static const int defaults[kBanks] { 0, 1, 3 };      // A flauta, B 3 violines, C coro
    for (int i = 0; i < kBanks; ++i) setBankSynth (i, defaults[i]);
}

MelloAudioProcessor::~MelloAudioProcessor()
{
    stopTimer();
    cancelPendingUpdate();
    pool.removeAllJobs (true, 10000);
}

bool MelloAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MelloAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    engine.prepare (sampleRate);
    rack.prepare (sampleRate, samplesPerBlock);
}

//==============================================================================
MelloParams MelloAudioProcessor::readParams() const
{
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    MelloParams m;
    m.attack = P ("attack"); m.decay = P ("decay"); m.sustain = P ("sustain") * 0.01f; m.release = P ("release");
    m.flutter = P ("flutter") * 0.01f; m.satur = P ("satur") * 0.01f; m.mech = P ("mech") * 0.01f;
    m.noise = P ("noise") * 0.01f; m.velVol = P ("velvol") * 0.01f; m.atFlutter = P ("atflutter") * 0.01f;
    m.volume = P ("volume") * 0.01f; m.tone = P ("tone") * 0.01f; m.pitchCents = P ("pitch");
    m.power = P ("power") > 0.5f;
    m.bank = juce::jlimit (0, 3, (int) std::round (P ("bank")));
    m.tapeLimit = P ("tapelen");
    m.loop = P ("loop") > 0.5f;
    for (int b = 0; b < kBanks; ++b)
    {
        const Tape* t = bank.getRaw (b);
        m.rootOverride[b] = (t != nullptr && t->user) ? (int) std::round (P (("root" + juce::String (b)).toRawUTF8())) : -1;
    }
    return m;
}

void MelloAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())          engine.noteOff (m.getNoteNumber());
    else if (m.isChannelPressure())  engine.setAftertouch ((float) m.getChannelPressureValue() / 127.f);
    else if (m.isAftertouch())       engine.setAftertouch ((float) m.getAfterTouchValue() / 127.f);
    else if (m.isPitchWheel())       engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.f * 2.f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc == 1) engine.setModWheel ((float) v / 127.f);
        else if (cc == 64) engine.setPedal (v >= 64);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

// Hilo de audio: sin asignaciones de memoria ni bloqueos (el rack usa try-lock).
void MelloAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || n == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    engine.setParams (readParams());

    {   // teclas tocadas con el raton
        int s1, z1, s2, z2;
        uiFifo.prepareToRead (uiFifo.getNumReady(), s1, z1, s2, z2);
        for (int i = 0; i < z1 + z2; ++i)
        {
            const int code = uiBuf[i < z1 ? s1 + i : s2 + (i - z1)];
            const bool on = ((code >> 16) & 1) != 0;
            const int note = (code >> 8) & 0xff, vel = code & 0xff;
            if (on) engine.noteOn (note, (float) vel / 127.f); else engine.noteOff (note);
        }
        uiFifo.finishedRead (z1 + z2);
    }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int p = juce::jlimit (pos, n, meta.samplePosition);
        if (p > pos) { engine.render (L + pos, R + pos, p - pos); pos = p; }
        handleMidi (meta.getMessage());
    }
    engine.render (L + pos, R + pos, n - pos);

    // inserts alojados: xQUILER -> FRAGATIME -> XPACEVERB
    bool byp[FxRack::kSlots];
    for (int i = 0; i < FxRack::kSlots; ++i) byp[i] = apvts.getRawParameterValue ("fxbyp" + juce::String (i))->load() > 0.5f;
    rack.process (buffer, byp);

    float pk = 0.f;
    for (int i = 0; i < n; ++i)
    {
        L[i] = std::tanh (L[i]); R[i] = std::tanh (R[i]);
        pk = juce::jmax (pk, std::abs (L[i]), std::abs (R[i]));
    }
    outLevel.store (outLevel.load() * 0.85f + pk * 0.15f);
}

void MelloAudioProcessor::uiNote (int note, float vel, bool on)
{
    int s1, z1, s2, z2;
    uiFifo.prepareToWrite (1, s1, z1, s2, z2);
    if (z1 > 0)
    {
        uiBuf[s1] = ((on ? 1 : 0) << 16) | ((juce::jlimit (0, 127, note)) << 8) | (int) (juce::jlimit (0.f, 1.f, vel) * 127.f);
        uiFifo.finishedWrite (1);
    }
}

//==============================================================================
float MelloAudioProcessor::bankConfidence (int slot) const
{
    const Tape* t = bank.getRaw (slot);
    return t != nullptr ? t->confidence : 1.f;
}

void MelloAudioProcessor::setBankSynth (int slot, int synthIdx)
{
    apvts.state.setProperty ("src" + juce::String (slot), 1, nullptr);
    apvts.state.setProperty ("synth" + juce::String (slot), synthIdx, nullptr);
    pool.addJob ([this, slot, synthIdx] { bank.set (slot, synthTape (synthIdx)); });
}

void MelloAudioProcessor::setBankFile (int slot, const juce::File& f, bool applyDetectedRoot)
{
    apvts.state.setProperty ("src" + juce::String (slot), 2, nullptr);
    apvts.state.setProperty ("path" + juce::String (slot), f.getFullPathName(), nullptr);
    pool.addJob ([this, slot, f, applyDetectedRoot]
    {
        if (auto t = loadTapeFile (f))
        {
            const int root = (int) std::round (t->zones[0].rootF);
            bank.set (slot, t);
            if (applyDetectedRoot) { pendingRoot[slot].store (root); triggerAsyncUpdate(); }   // la nota detectada pasa a ser la raiz
        }
    });
}

void MelloAudioProcessor::handleAsyncUpdate()        // hilo de interfaz
{
    for (int s = 0; s < kBanks; ++s)
    {
        const int r = pendingRoot[s].exchange (-1);
        if (r >= 0)
            if (auto* p = apvts.getParameter ("root" + juce::String (s)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) r));
    }
}

//==============================================================================
juce::AudioProcessorEditor* MelloAudioProcessor::createEditor()
{
    return new MelloAudioProcessorEditor (*this);
}

void MelloAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    rack.saveStates (apvts.state);
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MelloAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    auto& st = apvts.state;

    for (int i = 0; i < kBanks; ++i)
    {
        const int src = (int) st.getProperty ("src" + juce::String (i), 1);
        if (src == 2)
        {
            const juce::File f (st.getProperty ("path" + juce::String (i)).toString());
            if (f.existsAsFile()) setBankFile (i, f, false);          // se conserva la raiz guardada
        }
        else
        {
            static const int defaults[kBanks] { 0, 1, 3 };
            setBankSynth (i, (int) st.getProperty ("synth" + juce::String (i), defaults[i]));
        }
    }
    rack.restoreStates (st);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MelloAudioProcessor();
}
