#pragma once
#include "Common.h"

namespace ml
{
// Tres cintas (A, B, C). El hilo de audio solo lee punteros atomicos; las cintas reemplazadas se guardan 10 s
// antes de liberarse desde el hilo de interfaz (el audio nunca libera memoria).
class TapeBank
{
public:
    TapeBank() { for (auto& r : raw) r.store (nullptr); }

    void set (int slot, std::shared_ptr<Tape> t);
    const Tape* getRaw (int slot) const { return raw[(size_t) slot].load (std::memory_order_acquire); }   // hilo de audio
    juce::String getName (int slot) const;
    bool isUser (int slot) const;
    void collectGarbage();

private:
    struct Retired { std::shared_ptr<Tape> t; juce::uint32 when; };
    std::array<std::atomic<const Tape*>, kBanks> raw;
    std::array<std::shared_ptr<Tape>, kBanks> owned;
    std::vector<Retired> retired;
    mutable juce::CriticalSection lock;
};

// Detecta el tono (algoritmo YIN). Devuelve la nota MIDI con decimales, o -1 si no hay un tono claro.
float detectPitch (const juce::AudioBuffer<float>& b, double sampleRate, float& confidence);

// Decodifica .wav / .mp3 / .flac / .aif / .ogg (hilo de trabajo). nullptr si falla.
std::shared_ptr<Tape> loadTapeFile (const juce::File& f, double maxSeconds = 30.0);
}
