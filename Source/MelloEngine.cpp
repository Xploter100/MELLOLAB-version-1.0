#include "MelloEngine.h"

namespace ml
{
namespace
{
constexpr float kPi = 3.14159265f, kTwoPi = 6.2831853f;

inline float fastSin (float x)      // x en [-pi, pi]
{
    const float y = 1.2732395f * x - 0.4052847f * x * std::abs (x);
    return 0.225f * (y * std::abs (y) - y) + y;
}
inline float rnd (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.f / 2147483648.f); }
}

void MelloEngine::prepare (double sampleRate)
{
    fs = sampleRate;
    for (auto& k : keyLevel) k.store (0.f);
    reset();
}

void MelloEngine::reset()
{
    for (auto& v : voices) v = Voice();
    pedal = false; aftertouch = modWheel = bendSemis = 0.f;
    wowPh = flPh = drift = driftT = humPh1 = humPh2 = 0.f;
    driftCnt = 0; motor = P.power ? 1.f : 0.f;
    hissPrev = thumpLp = 0.f;
    for (int i = 0; i < 2; ++i) lpL[i] = lpR[i] = 0.f;
}

void MelloEngine::startRelease (Voice& v) { v.stage = 3; }

void MelloEngine::noteOn (int note, float vel)
{
    if (bank == nullptr) return;
    const bool all = (P.bank == 3);
    const int first = all ? 0 : juce::jlimit (0, 2, P.bank), last = all ? 2 : first;

    for (int b = first; b <= last; ++b)
    {
        const Tape* t = bank->getRaw (b);
        if (t == nullptr || t->zones.empty()) continue;

        const Zone* z = &t->zones[0];                 // la zona de cinta grabada mas cerca de la nota
        float bestD = 1e9f;
        for (const auto& zz : t->zones)
        {
            const float d = std::abs ((float) note - zz.rootF);
            if (d < bestD) { bestD = d; z = &zz; }
        }
        float root = z->rootF;
        if (t->user && P.rootOverride[b] >= 0) root += (float) P.rootOverride[b] - std::round (z->rootF);
        const double rate = juce::jlimit (0.02, 8.0, (z->rate / fs) * std::pow (2.0, ((double) note - (double) root) / 12.0));

        int idx = -1;
        for (int i = 0; i < kVoices; ++i) if (! voices[(size_t) i].active) { idx = i; break; }
        if (idx < 0)
        {
            uint32_t oldest = 0xffffffffu;
            for (int i = 0; i < kVoices; ++i) if (voices[(size_t) i].age < oldest) { oldest = voices[(size_t) i].age; idx = i; }
        }

        Voice& v = voices[(size_t) idx];
        v = Voice();
        v.active = true; v.held = true; v.note = note; v.bankIdx = b;
        v.tape = t; v.zone = z; v.baseRate = rate;
        v.vel = juce::jlimit (0.f, 1.f, vel);
        v.stage = 0; v.env = P.attack <= 0.0006f ? 1.f : 0.f;
        v.sus = P.sustain; v.spin = 1.f; v.click = 1.f;
        v.layer = all ? 0.62f : 1.f;
        v.detune = 1.f + rnd (rng) * 0.0008f;
        v.age = ++ageCounter;
        const float pan = all ? ((float) b - 1.f) * 0.35f : rnd (rng) * 0.1f;     // A izquierda, C derecha al combinar
        const float ang = (pan + 1.f) * kPi * 0.25f;
        v.gL = std::cos (ang) * 1.4142f;
        v.gR = std::sin (ang) * 1.4142f;
    }
}

void MelloEngine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == note)
        {
            v.held = false;
            if (pedal) v.sustained = true; else startRelease (v);
        }
}

void MelloEngine::setPedal (bool down)
{
    pedal = down;
    if (! down) for (auto& v : voices) if (v.active && v.sustained) { v.sustained = false; startRelease (v); }
}

void MelloEngine::allNotesOff()
{
    for (auto& v : voices) if (v.active) { v.held = false; v.sustained = false; startRelease (v); }
}

//==============================================================================
void MelloEngine::render (float* L, float* R, int n)
{
    int pos = 0;
    while (pos < n)
    {
        const int len = juce::jmin (256, n - pos);
        renderChunk (L + pos, R + pos, len);
        pos += len;
    }
}

void MelloEngine::renderChunk (float* L, float* R, int n)
{
    const float sr = (float) fs;
    float pm[256], mot[256], thumpSum[256];

    // ---- movimiento global de la cinta: flutter / wow / deriva + motor (power) ----
    const float amt = juce::jlimit (0.f, 1.5f, P.flutter + aftertouch * P.atFlutter + modWheel * 0.6f);
    const float wowInc = kTwoPi * 0.6f / sr, flInc = kTwoPi * 6.5f / sr;
    const float pitchMul = std::pow (2.f, (P.pitchCents * 0.01f + bendSemis) / 12.f);
    const float cUp = 1.f - std::exp (-1.f / (0.35f * sr)), cDn = 1.f - std::exp (-1.f / (0.9f * sr));
    const float mt = P.power ? 1.f : 0.f;
    for (int i = 0; i < n; ++i)
    {
        wowPh += wowInc; if (wowPh > kPi) wowPh -= kTwoPi;
        flPh += flInc;   if (flPh > kPi) flPh -= kTwoPi;
        if (--driftCnt <= 0) { driftT = rnd (rng); driftCnt = (int) (0.4f * sr); }
        drift += (driftT - drift) * 0.0004f;
        motor += (mt - motor) * (mt > motor ? cUp : cDn);
        const float dev = amt * (0.010f * fastSin (wowPh) + 0.0022f * fastSin (flPh) + 0.005f * drift);
        pm[i] = pitchMul * (1.f + dev) * motor;
        mot[i] = motor;
        thumpSum[i] = 0.f;
    }

    // ---- voces ----
    const float atkInc = P.attack > 0.0006f ? 1.f / (P.attack * sr) : 1.f;
    const float decStep = (1.f - P.sustain) / (juce::jmax (0.005f, P.decay) * sr);
    const float relCoef = std::exp (-1.f / (juce::jmax (0.01f, P.release) * sr));
    const float fastCoef = std::exp (-1.f / (0.02f * sr));
    const float spinDecay = std::exp (-1.f / (0.09f * sr));
    const double limitSamples = (double) P.tapeLimit * fs;

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const float* a = v.zone->buf.getReadPointer (0);
        const float* b = v.zone->buf.getNumChannels() > 1 ? v.zone->buf.getReadPointer (1) : a;
        const int frames = v.zone->frames;
        const float vg = ((1.f - P.velVol) + P.velVol * v.vel) * v.layer;
        const double loopStart = (double) frames * 0.3, loopLen = (double) (frames - 1) - loopStart;
        const double xf = juce::jmin (loopLen * 0.5, 0.15 * v.zone->rate);

        for (int i = 0; i < n; ++i)
        {
            switch (v.stage)
            {
                case 0: v.env += atkInc; if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } break;
                case 1: v.env -= decStep; if (v.env <= P.sustain) { v.env = P.sustain; v.stage = 2; } break;
                case 2: v.env = P.sustain; break;
                case 3: v.env *= relCoef; if (v.env < 1e-4f) v.active = false; break;
                default: v.env *= fastCoef; if (v.env < 1e-4f) v.active = false; break;
            }
            if (! v.active) break;

            v.spin *= spinDecay;
            const double rate = v.baseRate * (double) v.detune * (double) pm[i] * (1.0 - (double) (P.mech * 0.08f * v.spin));

            if (v.pos >= (double) (frames - 1))
            {
                if (P.loop) v.pos -= loopLen;
                else { if (v.stage != 4) v.stage = 4; v.pos = (double) (frames - 2); }     // se acabo la cinta
            }
            const int ip = juce::jlimit (0, frames - 2, (int) v.pos);
            const float fr = (float) (v.pos - (double) ip);
            float sl = a[ip] + fr * (a[ip + 1] - a[ip]);
            float sr2 = b[ip] + fr * (b[ip + 1] - b[ip]);

            if (P.loop && v.pos > (double) (frames - 1) - xf)       // cruce suave al volver al inicio del bucle
            {
                const double q = v.pos - loopLen;
                const int iq = juce::jlimit (0, frames - 2, (int) q);
                const float fq = (float) (q - (double) iq);
                const float t = (float) ((v.pos - ((double) (frames - 1) - xf)) / xf);
                sl += t * ((a[iq] + fq * (a[iq + 1] - a[iq])) - sl);
                sr2 += t * ((b[iq] + fq * (b[iq + 1] - b[iq])) - sr2);
            }

            const float g = v.env * vg;
            L[i] += sl * g * v.gL;
            R[i] += sr2 * g * v.gR;
            v.pos += rate;

            if (! P.loop && (double) ++v.tapeSamples > limitSamples && v.stage != 4) v.stage = 4;   // limite de cinta
            thumpSum[i] += v.click;
            v.click *= 0.9965f;
        }
    }

    // ---- cadena global: mecanica, ruido, saturacion de cinta, tono, volumen ----
    const float noiseLevel = std::pow (P.noise, 1.5f) * 0.03f;
    const float drive = 1.f + P.satur * 5.f, satNorm = 1.f + 0.3f * (drive - 1.f);
    const float aLP = 1.f - std::exp (-kTwoPi * (800.f * std::pow (20.f, P.tone)) / sr);
    const float aHF = 1.f - std::exp (-kTwoPi * (16000.f - P.satur * 9000.f) / sr);   // la saturacion de cinta tambien apaga agudos
    const float vol = P.volume * P.volume * 1.5f;
    const float humAmt = noiseLevel * 0.25f + P.mech * 0.002f;
    const float h1 = kTwoPi * 100.f / sr, h2 = kTwoPi * 250.f / sr;

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];

        thumpLp += 0.03f * (rnd (rng) - thumpLp);                       // golpe grave del mecanismo
        const float th = thumpLp * thumpSum[i] * P.mech * 3.f;
        l += th; r += th;

        humPh1 += h1; if (humPh1 > kPi) humPh1 -= kTwoPi;
        humPh2 += h2; if (humPh2 > kPi) humPh2 -= kTwoPi;
        const float nz = rnd (rng);
        const float hiss = (nz - hissPrev) * 0.5f; hissPrev = nz;
        const float hum = fastSin (humPh1) * 0.6f + fastSin (humPh2) * 0.3f;
        l += (hiss * noiseLevel + hum * humAmt) * mot[i];
        r += (hiss * noiseLevel * 0.9f + hum * humAmt) * mot[i];

        l = std::tanh (l * drive) / drive * satNorm;
        r = std::tanh (r * drive) / drive * satNorm;

        lpL[0] += aLP * (l - lpL[0]); lpL[1] += aHF * (lpL[0] - lpL[1]); l = lpL[1];
        lpR[0] += aLP * (r - lpR[0]); lpR[1] += aHF * (lpR[0] - lpR[1]); r = lpR[1];

        const float g = vol * mot[i] * mot[i];
        L[i] = l * g; R[i] = r * g;
    }

    if (! P.power && motor < 0.01f) for (auto& v : voices) v.active = false;

    float lvl[kNumKeys] {};
    for (const auto& v : voices)
        if (v.active) { const int k = v.note - kFirstKey; if (k >= 0 && k < kNumKeys) lvl[k] = juce::jmax (lvl[k], v.env); }
    for (int k = 0; k < kNumKeys; ++k) keyLevel[(size_t) k].store (lvl[k]);
    motorOut.store (motor);
}
} // namespace ml
