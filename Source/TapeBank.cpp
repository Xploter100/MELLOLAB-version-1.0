#include "TapeBank.h"

namespace ml
{
void TapeBank::set (int slot, std::shared_ptr<Tape> t)
{
    if (slot < 0 || slot >= kBanks) return;
    const juce::ScopedLock sl (lock);
    if (owned[(size_t) slot] != nullptr) retired.push_back ({ owned[(size_t) slot], juce::Time::getMillisecondCounter() });
    owned[(size_t) slot] = t;
    raw[(size_t) slot].store (t.get(), std::memory_order_release);
}

juce::String TapeBank::getName (int slot) const
{
    const juce::ScopedLock sl (lock);
    auto t = owned[(size_t) juce::jlimit (0, kBanks - 1, slot)];
    return t != nullptr ? t->name : juce::String();
}

bool TapeBank::isUser (int slot) const
{
    const juce::ScopedLock sl (lock);
    auto t = owned[(size_t) juce::jlimit (0, kBanks - 1, slot)];
    return t != nullptr && t->user;
}

void TapeBank::collectGarbage()
{
    const juce::ScopedLock sl (lock);
    const auto now = juce::Time::getMillisecondCounter();
    for (size_t i = retired.size(); i-- > 0;)
        if (now - retired[i].when > 10000u) retired.erase (retired.begin() + (long) i);
}

//==============================================================================
float detectPitch (const juce::AudioBuffer<float>& b, double sr, float& confidence)
{
    confidence = 0.f;
    const int n = b.getNumSamples();
    if (n < 4096) return -1.f;

    // mono y reducido a ~22 kHz para que el analisis sea rapido
    const int dec = juce::jmax (1, (int) std::round (sr / 22050.0));
    const double sr2 = sr / (double) dec;
    std::vector<float> m;
    m.reserve ((size_t) (n / dec + 1));
    const int chs = b.getNumChannels();
    for (int i = 0; i + dec <= n; i += dec)
    {
        float s = 0.f;
        for (int d = 0; d < dec; ++d) for (int c = 0; c < chs; ++c) s += b.getSample (c, i + d);
        m.push_back (s / (float) (dec * chs));
    }

    // la zona mas fuerte (0.3 s) suele ser la mas estable
    const int blk = (int) (0.1 * sr2), nb = (int) m.size() / blk;
    if (nb < 4) return -1.f;
    std::vector<double> e ((size_t) nb, 0.0);
    for (int k = 0; k < nb; ++k) for (int i = 0; i < blk; ++i) e[(size_t) k] += (double) m[(size_t) (k * blk + i)] * m[(size_t) (k * blk + i)];
    int bestK = 0; double bestE = -1.0;
    for (int k = 0; k + 2 < nb; ++k) { const double s = e[(size_t) k] + e[(size_t) k + 1] + e[(size_t) k + 2]; if (s > bestE) { bestE = s; bestK = k; } }

    const int W = 1500, maxLag = (int) (sr2 / 50.0), minLag = juce::jmax (2, (int) (sr2 / 2000.0));
    std::vector<float> found, confs;
    std::vector<float> d ((size_t) maxLag + 2), cm ((size_t) maxLag + 2);
    for (int f = 0; f < 5; ++f)
    {
        const int s0 = bestK * blk + f * 700;
        if (s0 + W + maxLag + 2 >= (int) m.size()) break;

        d[0] = 0.f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < W; ++j) { const double df = (double) m[(size_t) (s0 + j)] - m[(size_t) (s0 + j + tau)]; sum += df * df; }
            d[(size_t) tau] = (float) sum;
        }
        cm[0] = 1.f;
        double run = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = run > 0.0 ? (float) (d[(size_t) tau] * tau / run) : 1.f; }

        int tau = -1;
        for (int t = minLag; t < maxLag; ++t)
            if (cm[(size_t) t] < 0.15f) { while (t + 1 < maxLag && cm[(size_t) t + 1] < cm[(size_t) t]) ++t; tau = t; break; }
        if (tau < 0)
        {
            float mn = 1e9f;
            for (int t = minLag; t < maxLag; ++t) if (cm[(size_t) t] < mn) { mn = cm[(size_t) t]; tau = t; }
        }
        if (tau < minLag || cm[(size_t) tau] > 0.35f) continue;

        float tf = (float) tau;
        if (tau > 1 && tau < maxLag)    // interpolacion parabolica
        {
            const float a = cm[(size_t) tau - 1], c = cm[(size_t) tau], dd = cm[(size_t) tau + 1], den = a - 2.f * c + dd;
            if (std::abs (den) > 1e-9f) tf += juce::jlimit (-1.f, 1.f, 0.5f * (a - dd) / den);
        }
        found.push_back (69.f + 12.f * std::log2 ((float) (sr2 / tf) / 440.f));
        confs.push_back (1.f - cm[(size_t) tau]);
    }
    if (found.empty()) return -1.f;
    std::sort (found.begin(), found.end());
    std::sort (confs.begin(), confs.end());
    confidence = confs[confs.size() / 2];
    return found[found.size() / 2];
}

std::shared_ptr<Tape> loadTapeFile (const juce::File& f, double maxSeconds)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 1024) return nullptr;

    const int n = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxSeconds));
    Zone z;
    z.buf.setSize (2, n);
    reader->read (&z.buf, 0, n, 0, true, true);
    if (reader->numChannels == 1) z.buf.copyFrom (1, 0, z.buf, 0, 0, n);
    z.rate = reader->sampleRate;

    // recortar silencio inicial y normalizar
    int start = 0;
    while (start < n - 1 && std::abs (z.buf.getSample (0, start)) < 0.002f && std::abs (z.buf.getSample (1, start)) < 0.002f) ++start;
    start = juce::jmax (0, start - (int) (0.002 * z.rate));
    if (start > 0)
    {
        juce::AudioBuffer<float> t (2, n - start);
        for (int c = 0; c < 2; ++c) t.copyFrom (c, 0, z.buf, c, start, n - start);
        z.buf = std::move (t);
    }
    z.frames = z.buf.getNumSamples();
    const float pk = juce::jmax (z.buf.getMagnitude (0, 0, z.frames), z.buf.getMagnitude (1, 0, z.frames));
    if (pk > 0.0001f) z.buf.applyGain (0.8f / pk);
    const int fin = juce::jmin (z.frames, (int) (0.004 * z.rate)), fout = juce::jmin (z.frames, (int) (0.02 * z.rate));
    for (int c = 0; c < 2; ++c)
    {
        float* d = z.buf.getWritePointer (c);
        for (int i = 0; i < fin; ++i) d[i] *= (float) i / (float) fin;
        for (int i = 0; i < fout; ++i) d[z.frames - 1 - i] *= (float) i / (float) fout;
    }

    auto t = std::make_shared<Tape>();
    t->user = true;
    t->name = f.getFileNameWithoutExtension();
    float conf = 0.f;
    const float midi = detectPitch (z.buf, z.rate, conf);
    t->confidence = conf;
    z.rootF = (midi > 0.f && conf >= 0.5f) ? midi : 60.f;      // sin tono claro: se asume DO central (se puede corregir a mano)
    t->zones.push_back (std::move (z));
    return t;
}
} // namespace ml
