#pragma once
#include <JuceHeader.h>

// Rack interno: detecta si xQUILER, FRAGATIME y XPACEVERB estan instalados (VST3) y los aloja DENTRO de MELLOLAB
// como inserts (despues del sonido del Mellotron, en ese orden). Si no estan, la interfaz recomienda descargarlos.
class FxRack
{
public:
    static constexpr int kSlots = 3;
    enum Status { NotFound = 0, Found, Loading, Loaded, Failed };

    static const char* slotName (int i) { static const char* n[kSlots] { "xQUILER", "FRAGATIME", "XPACEVERB" }; return n[i]; }
    static const char* slotRole (int i) { static const char* n[kSlots] { "Parametric EQ", "Granular effect", "Reverb" }; return n[i]; }

    FxRack();
    ~FxRack();

    void prepare (double sampleRate, int blockSize);
    void setExtraFolders (const juce::StringArray& f);
    void rescan();                                   // busca en las carpetas VST3 (hilo de trabajo)
    bool isScanning() const { return scanning.load(); }

    Status getStatus (int i) const { return (Status) st[(size_t) i].load(); }
    juce::File getPath (int i) const;
    juce::String getMessage (int i) const;
    juce::StringArray getSearchedFolders() const;

    void load (int i);                               // asincrono
    void unload (int i);
    juce::AudioPluginInstance* getInstance (int i) const { return fx[(size_t) i].get(); }   // solo hilo de interfaz

    void process (juce::AudioBuffer<float>& buffer, const bool* bypass);                   // hilo de audio

    void saveStates (juce::ValueTree& t);
    void restoreStates (const juce::ValueTree& t);

private:
    void finishLoad (int i, std::unique_ptr<juce::AudioPluginInstance> inst);
    void fail (int i, const juce::String& why);

    juce::AudioPluginFormatManager fm;
    std::array<std::unique_ptr<juce::AudioPluginInstance>, kSlots> fx;
    juce::SpinLock lock;
    std::array<std::atomic<int>, kSlots> st;
    std::atomic<bool> scanning { false };
    juce::File paths[kSlots];
    juce::String msgs[kSlots];
    juce::StringArray extraFolders, searched;
    mutable juce::CriticalSection info;
    bool wantLoad[kSlots] {};
    juce::MemoryBlock pendingState[kSlots];
    double sr = 44100.0;
    int block = 512;
    juce::MidiBuffer midiScratch;
    std::shared_ptr<std::atomic<bool>> alive;
    juce::ThreadPool pool { 1 };
};
