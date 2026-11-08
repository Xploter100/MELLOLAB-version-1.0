#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace ml
{
constexpr int kBanks = 3;          // cintas A, B, C
constexpr int kFirstKey = 41;      // F2
constexpr int kNumKeys = 36;       // F2 .. E5
constexpr int kVoices = 32;

inline juce::String noteName (int midi)
{
    static const char* n[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (n[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

// Una "zona" es un trozo de cinta grabado a una nota. Las notas vecinas se obtienen cambiando la velocidad
// de la cinta (igual que en un Mellotron). Los sonidos sinteticos traen 3 zonas; los del usuario, 1.
struct Zone
{
    juce::AudioBuffer<float> buf;   // 1 o 2 canales
    double rate = 44100.0;
    float rootF = 60.f;             // nota MIDI (con decimales) en la que esta grabada
    int frames = 0;
};

struct Tape
{
    std::vector<Zone> zones;
    juce::String name;
    bool user = false;              // true = sonido del usuario (la nota raiz se puede corregir)
    float confidence = 1.f;         // seguridad de la deteccion de tono (0..1)
};
}
