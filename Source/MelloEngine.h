#pragma once
#include "Common.h"
#include "TapeBank.h"

namespace ml
{
struct MelloParams
{
    float attack = 0.02f, decay = 0.5f, sustain = 1.f, release = 0.25f;               // s, s, 0..1, s
    float flutter = 0.25f, satur = 0.2f, mech = 0.3f, noise = 0.15f, velVol = 0.7f, atFlutter = 0.5f;   // 0..1
    float volume = 0.8f, tone = 0.7f, pitchCents = 0.f;
    bool power = true;
    int bank = 0;                    // 0 A, 1 B, 2 C, 3 ALL
    float tapeLimit = 8.f;           // segundos que dura la cinta (Mellotron real: ~8 s)
    bool loop = false;               // false = autentico: la cinta se acaba
    int rootOverride[kBanks] { -1, -1, -1 };    // solo para sonidos del usuario
};

// Motor de cinta: 32 voces pre-asignadas, sin asignaciones de memoria durante render().
class MelloEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    void setBank (const TapeBank* b) { bank = b; }
    void setParams (const MelloParams& p) { P = p; }

    void noteOn (int note, float vel01);
    void noteOff (int note);
    void setPedal (bool down);
    void setAftertouch (float v01) { aftertouch = v01; }
    void setModWheel (float v01) { modWheel = v01; }
    void setPitchBend (float semitones) { bendSemis = semitones; }
    void allNotesOff();

    void render (float* L, float* R, int n);      // SUMA sobre L / R (deben venir a cero)

    std::array<std::atomic<float>, kNumKeys> keyLevel;   // para animar teclas y cintas
    std::atomic<float> motorOut { 0.f };

private:
    struct Voice
    {
        bool active = false, held = false, sustained = false;
        int note = 0, bankIdx = 0, stage = 0, tapeSamples = 0;
        const Tape* tape = nullptr;
        const Zone* zone = nullptr;
        double pos = 0.0, baseRate = 1.0, loopLen = 0.0;
        float vel = 1.f, env = 0.f, sus = 1.f, spin = 1.f, click = 0.f, gL = 1.f, gR = 1.f, layer = 1.f, detune = 1.f;
        uint32_t age = 0;
    };

    void renderChunk (float* L, float* R, int n);
    void startRelease (Voice& v);

    const TapeBank* bank = nullptr;
    MelloParams P;
    double fs = 44100.0;
    std::array<Voice, kVoices> voices;
    uint32_t ageCounter = 0;
    bool pedal = false;
    float aftertouch = 0.f, modWheel = 0.f, bendSemis = 0.f;

    float wowPh = 0.f, flPh = 0.f, drift = 0.f, driftT = 0.f, humPh1 = 0.f, humPh2 = 0.f, motor = 0.f;
    int driftCnt = 0;
    uint32_t rng = 2463534242u;
    float hissPrev = 0.f, thumpLp = 0.f;
    float lpL[2] {}, lpR[2] {};
};
}
