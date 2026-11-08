#include "TapeSynth.h"

namespace ml
{
namespace
{
constexpr double kFs = 44100.0;
constexpr float kTwoPi = 6.2831853f;

struct Rng
{
    uint32_t s = 2463534242u;
    float bi() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.f / 2147483648.f); }
};

struct Biquad
{
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void bandpass (double f, double Q)
    {
        const double w = 2.0 * 3.14159265358979 * f / kFs, al = std::sin (w) / (2.0 * Q), a0 = 1.0 + al;
        b0 = (float) (al / a0); b1 = 0.f; b2 = (float) (-al / a0);
        a1 = (float) (-2.0 * std::cos (w) / a0); a2 = (float) ((1.0 - al) / a0);
    }
    float run (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

struct Voice { float phase = 0.f; float detCents = 0.f; };

// Recetas: 0 flauta, 1 3 violines, 2 8 violines, 3 coro Aah, 4 coro Ooh, 5 chelo
void renderZone (int idx, float f0, juce::AudioBuffer<float>& out)
{
    const int N = out.getNumSamples();
    float* o = out.getWritePointer (0);
    Rng rng;
    float peak = 1e-6f;

    if (idx == 0)   // flauta: parciales suaves + soplido + deriva lenta (el caracter "cinta")
    {
        static const float amp[5] { 1.f, 0.30f, 0.12f, 0.05f, 0.025f };
        float ph[5] {}, drift = 0.f, driftT = 0.f, lp = 0.f;
        Biquad breath; breath.bandpass (f0 * 3.0, 1.2);
        int cnt = 0;
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) kFs;
            if (--cnt <= 0) { driftT = rng.bi() * 0.0012f; cnt = (int) (0.3 * kFs); }
            drift += (driftT - drift) * 0.0004f;
            const float f = f0 * (1.f + drift);
            float s = 0.f;
            for (int p = 0; p < 5; ++p)
            {
                ph[p] += kTwoPi * f * (float) (p + 1) / (float) kFs;
                if (ph[p] > kTwoPi) ph[p] -= kTwoPi;
                s += amp[p] * std::sin (ph[p]);
            }
            const float n = rng.bi();
            lp += 0.35f * (n - lp);
            s += breath.run (n) * 0.9f + lp * 0.02f;
            s *= juce::jmin (1.f, t / 0.025f) * (1.f + 0.03f * std::sin (kTwoPi * 0.3f * t));
            o[i] = s;
            peak = juce::jmax (peak, std::abs (s));
        }
    }
    else if (idx == 3 || idx == 4)   // coro: fuente glotal (diente de sierra) + formantes de la vocal
    {
        static const float fA[4] { 800.f, 1150.f, 2900.f, 3900.f }, gA[4] { 1.f, 0.6f, 0.25f, 0.1f }, qA[4] { 10.f, 12.f, 24.f, 30.f };
        static const float fO[4] { 350.f, 800.f, 2700.f, 3500.f }, gO[4] { 1.f, 0.35f, 0.15f, 0.08f }, qO[4] { 6.f, 9.f, 22.f, 28.f };
        const float* F = idx == 3 ? fA : fO; const float* G = idx == 3 ? gA : gO; const float* Q = idx == 3 ? qA : qO;
        Biquad fmt[4];
        for (int k = 0; k < 4; ++k) fmt[k].bandpass (F[k], Q[k]);
        Voice v[4]; static const float det[4] { -9.f, -3.f, 3.f, 9.f };
        for (int k = 0; k < 4; ++k) { v[k].detCents = det[k]; v[k].phase = 0.21f * (float) k; }
        float lp = 0.f;
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) kFs;
            const float vib = 7.f * std::sin (kTwoPi * 5.2f * t) * juce::jmin (1.f, t / 0.9f);
            float src = 0.f;
            for (auto& vo : v)
            {
                const float f = f0 * std::pow (2.f, (vo.detCents + vib) / 1200.f), dt = f / (float) kFs;
                vo.phase += dt; if (vo.phase >= 1.f) vo.phase -= 1.f;
                src += 2.f * vo.phase - 1.f - polyBlep (vo.phase, dt);
            }
            src *= 0.25f;
            lp += 0.25f * (src - lp);
            float s = 0.f;
            for (int k = 0; k < 4; ++k) s += G[k] * fmt[k].run (lp * 4.f);
            s += rng.bi() * 0.01f;
            s *= juce::jmin (1.f, t / 0.22f);
            o[i] = s;
            peak = juce::jmax (peak, std::abs (s));
        }
    }
    else    // cuerdas: varias sierras desafinadas + vibrato + resonancias del cuerpo
    {
        const int nv = idx == 2 ? 8 : (idx == 1 ? 3 : 2);
        std::vector<Voice> v ((size_t) nv);
        for (int k = 0; k < nv; ++k) { v[(size_t) k].detCents = ((float) k - (float) (nv - 1) * 0.5f) * (idx == 2 ? 4.f : 7.f); v[(size_t) k].phase = 0.13f * (float) k; }
        const double bodyF[3] = { idx == 5 ? 250.0 : 450.0, idx == 5 ? 700.0 : 1500.0, idx == 5 ? 1500.0 : 3200.0 };
        Biquad body[3]; for (int k = 0; k < 3; ++k) body[k].bandpass (bodyF[k], 2.5);
        const float atk = idx == 5 ? 0.25f : 0.18f;
        float lp = 0.f;
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) kFs;
            const float vib = 5.f * std::sin (kTwoPi * 5.4f * t) * juce::jmin (1.f, t / 1.2f);
            float src = 0.f;
            for (auto& vo : v)
            {
                const float f = f0 * std::pow (2.f, (vo.detCents + vib) / 1200.f), dt = f / (float) kFs;
                vo.phase += dt; if (vo.phase >= 1.f) vo.phase -= 1.f;
                src += 2.f * vo.phase - 1.f - polyBlep (vo.phase, dt);
            }
            src /= (float) nv;
            float s = 0.3f * src;
            for (int k = 0; k < 3; ++k) s += 0.9f * body[k].run (src);
            lp += 0.3f * (s - lp);
            s = lp * juce::jmin (1.f, t / atk);
            o[i] = s;
            peak = juce::jmax (peak, std::abs (s));
        }
    }

    const float g = 0.7f / peak;
    for (int i = 0; i < N; ++i) o[i] *= g;
    const int fo = (int) (0.02 * kFs);
    for (int i = 0; i < fo; ++i) o[N - 1 - i] *= (float) i / (float) fo;
}
} // namespace

const char* synthName (int idx)
{
    static const char* n[kSynthCount] { "Flute (Strawberry style)", "3 Violins", "8 Violins (Strings)", "Choir Aah", "Choir Ooh", "Cello" };
    return n[juce::jlimit (0, kSynthCount - 1, idx)];
}

std::shared_ptr<Tape> synthTape (int idx)
{
    idx = juce::jlimit (0, kSynthCount - 1, idx);
    auto t = std::make_shared<Tape>();
    t->name = synthName (idx);
    t->user = false;
    static const int roots[3] { 48, 60, 72 };           // DO3, DO4, DO5
    for (int r : roots)
    {
        Zone z;
        z.rate = kFs;
        z.frames = (int) (10.0 * kFs);                  // 10 s de "cinta"
        z.buf.setSize (1, z.frames);
        z.rootF = (float) r;
        renderZone (idx, 440.f * std::pow (2.f, ((float) r - 69.f) / 12.f), z.buf);
        t->zones.push_back (std::move (z));
    }
    return t;
}
} // namespace ml
