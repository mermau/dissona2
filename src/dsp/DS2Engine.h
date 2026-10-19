#pragma once
// =============================================================================================
//  Motore di emulazione del Crumar DS-2 (1978), ricostruito dagli schemi originali (DWG1–18),
//  dal manuale d'uso e dal reverse engineering di Enrico Bassi.
//
//  DCO (P-435, P-434, P-436, P-448)
//    Il master clock di ogni oscillatore è un multivibratore a due 74LS221 (≈1 MHz) la cui
//    frequenza segue il PITCH manuale, la leva di bending e il vibrato dell'LFO mixer (ingresso
//    VIB via 22K1: modulazione LINEARE in Hz). Il clock passa in un moltiplicatore di frequenza
//    binario a 12 bit (rate multiplier, "accordatore digitale") pilotato dal codice a 12 bit della
//    nota (P-432): f = f_clk·N/4096, con N proporzionale alla frequenza della nota (DO acuto = 4095).
//    Gli impulsi del rate multiplier non sono equidistanti: questo produce il leggero "jitter" dei
//    DCO originali. Seguono il divisore di registro (4'…32') e il contatore della forma d'onda:
//      dente di sega 16 gradini, triangolo 32 gradini (16 livelli), quadra, impulso con larghezza
//      quantizzata a 1/32 di periodo (50 % … 6 %). Le forme d'onda sono a gradini, senza filtri di
//      ricostruzione: i gradini sono resi con polyBLEP, quindi le immagini spettrali reali dei
//      gradini restano (il suono "ringy/chirpy" del DS-2) ma senza aliasing digitale.
//  GLIDE (P-432/P-433): un contatore avanti/indietro porta il codice N verso la nuova nota di un'unità
//    per impulso di clock: il portamento è quindi lineare in Hz e procede a piccoli gradini.
//  KEYBOARD CONTROL: la DAC P-433 converte lo stesso codice N in una tensione proporzionale alla
//    frequenza (−0,62 V primo FA … −7,8 V ultimo DO) che diventa la corrente di riferimento del
//    convertitore esponenziale del filtro (al posto della tensione fissa di −5,6 V).
//  VCF (P-446, DWG3): quattro CA3080 con buffer JFET BF244. Ogni stadio: ingresso + 33K/330Ω,
//    controreazione locale 33K/220Ω → guadagno DC 1,5 e saturazione tanh dell'OTA. I primi tre stadi
//    hanno 150 pF, il quarto 22 pF (polo 6,8 volte più alto). Risonanza: potenziometro 100K lin.
//    reostato + 3K3 + trimmer RV3 verso l'ingresso invertente del primo stadio; l'autooscillazione
//    parte a ≈ 8½ come indicato nel manuale.
//  ADSR (P-440, DWG6): condensatore 4,7 µF, attacco verso +5 V terminato a 4,3 V dal comparatore,
//    decadimento verso il sustain (5K lin. sul riferimento 4,3 V), rilascio verso il diodo di
//    clamp; reostati 1M antilog.
//  LFO (P-443, DWG9): integratore + trigger di Schmitt, 0,3…30 Hz con reostato 1M lin. a filo.
//    LFO1: dente di sega ottenuto commutando la polarità del triangolo (frequenza doppia), sega
//    inversa, triangolo, quadra. LFO2: contatore HBF4727 → scala discendente a 16 gradini e
//    "random" ripetitivo (stessi bit con pesi invertiti), triangolo, quadra.
//  LFO DELAY (P-453): rampa avviata dal gate che apre un CA3080 sul segnale dell'LFO.
//  POLY (P-444, P-450, P-463): generatore MK50240 (divisori 478…253) + divisori HBF4727; ogni
//    nota è una scala a 8 gradini ottenuta sommando tre ottave di onda quadra (47K/100K/220K).
//    I tasti collegano direttamente le note (nessun rilascio). Equalizzatore Baxandall ±15 dB,
//    poi nel VCF (via 5K6) insieme ai DCO e al rumore.
//  NOISE (P-439, DWG15): bianco, oppure "pink" filtrato dalla rete 270K/47K+10n/1n dell'LM307.
// =============================================================================================
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ds2
{
constexpr double kPi = 3.14159265358979323846;

struct EngineParams
{
    // LFO's MIXER (selettori: 0 = OUT, 1 = LFO1, 2 = LFO2, 3 = 1+2; PW: 0 = MAN, 1, 2)
    float mxO1Amt = 0.0f;  int mxO1Src = 0;
    float mxO2Amt = 0.0f;  int mxO2Src = 0;
    float mxVcfAmt = 0.0f; int mxVcfSrc = 0;
    float mxVcaAmt = 0.0f; int mxVcaSrc = 0;
    float mxPwAmt = 0.0f;  int mxPwSrc = 0;
    // LFO (forme LFO1: 0 sega inversa, 1 triangolo, 2 quadra, 3 sega; LFO2: 0 S&H, 1 tri, 2 quadra, 3 scala)
    float lfo1Rate = 0.5f; int lfo1Wave = 1; bool lfo1Delay = false;
    float lfo2Rate = 0.5f; int lfo2Wave = 1; bool lfo2Delay = false;
    float lfoDelay = 0.3f;
    // DCO (registro: 0 = 32', 1 = 16', 2 = 8', 3 = 4'; forma: 0 sega, 1 tri, 2 quadra, 3 impulso)
    float o1Vol = 0.7f, o1Pitch = 0.0f; int o1Range = 2, o1Wave = 0;
    float o2Vol = 0.0f, o2Pitch = 0.0f; int o2Range = 2, o2Wave = 1;
    bool  kbdCtrl = false;
    // VCF / VCA
    float vcfEnv = 0.5f, vcfCut = 0.6f, vcfRes = 0.0f;
    float vcfA = 0.0f, vcfD = 0.5f, vcfS = 0.7f, vcfR = 0.1f;
    float vcaEnv = 0.8f;
    float vcaA = 0.0f, vcaD = 0.5f, vcaS = 0.7f, vcaR = 0.1f;
    bool  vcaIn = false;   // deviatore tra VCF e VCA
    int   vcaSwMode = 0;   // 0 = come il manuale Crumar (IN: l'ADSR del VCF comanda anche il VCA)
                           // 1 = come la macchina di E. Bassi (IN: ADSR del VCA, OUT: solo gate)
    // NOISE
    float noiseVol = 0.0f; bool noisePink = false;
    // POLY
    float polyLfoAmt = 0.0f; int polyLfoSrc = 0;
    float polyHigh = 0.0f, polyLow = 0.0f;   // −1 … +1  (±15 dB)
    float polyVol = 0.6f; bool polyOn = false;
    int   polyOct = 1;                       // 0 = 16', 1 = 8', 2 = 4'
    float polyPitch = 0.0f;                  // −1 … +1  (±4 semitoni)
    // uscita
    bool  headphones = false; float volume = 0.7f;
    // tastiera
    bool  glideOn = false, glideLong = false; float glideSpeed = 0.5f;
    float bend = 0.0f, bendRange = 3.0f;     // leva: −1 … +1, ±terza minore come l'originale
    bool  retrigger = false;
    bool  originalRange = false;             // solo i 44 tasti FA…DO (MIDI 41…84)
    bool  polySplit = false;                 // uscita POLY separata (canale destro)
};

//==============================================================================================
inline float fastTanh (float x)
{
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)))
             / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2)));
}
inline float tanhGain (float x) { return std::fabs (x) < 1.0e-4f ? 1.0f : fastTanh (x) / x; }

class Rng
{
public:
    explicit Rng (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float bipolar() { return (float) (int32_t) next() * (1.0f / 2147483648.0f); }
private:
    uint32_t s;
};

// potenziometro lineare usato come partitore, caricato da rLoad verso massa
inline float loadedPot (float a, float rLoad, float rPot)
{
    a = std::clamp (a, 0.0f, 1.0f);
    return a * rLoad / (rLoad + a * (1.0f - a) * rPot);
}
// curva logaritmica (≈10 % a metà corsa) e antilogaritmica dei potenziometri/slider
inline double logTaper (double k) { k = std::clamp (k, 0.0, 1.0); return (std::pow (100.0, k) - 1.0) / 99.0; }

// Accumulo dei gradini con polyBLEP a due campioni (un campione di latenza).
// d = distanza dell'evento dalla fine del campione corrente, in campioni (0…1).
struct BlepLine
{
    float pending = 0.0f, corrPrev = 0.0f, corrCur = 0.0f;
    inline void step (float h, float d)
    {
        corrPrev += h * 0.5f * d * d;
        const float e = 1.0f - d;
        corrCur  -= h * 0.5f * e * e;
    }
    inline float out (float naive)
    {
        const float y = pending + corrPrev;
        pending = naive + corrCur;
        corrPrev = 0.0f; corrCur = 0.0f;
        return y;
    }
    void reset() { pending = corrPrev = corrCur = 0.0f; }
};

// passa-alto a un polo (accoppiamenti in alternata)
struct DcBlock
{
    float a = 0.9999f, x1 = 0.0f, y1 = 0.0f;
    void set (double hz, double fs) { a = (float) std::exp (-2.0 * kPi * hz / fs); }
    inline float tick (float x) { const float y = a * (y1 + x - x1); x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

// filtro biquad (forma diretta trasposta II)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float tick (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    // trasformazione bilineare di (n0 + n1 s + n2 s²)/(d0 + d1 s + d2 s²)
    void setAnalog (double n0, double n1, double n2, double d0, double d1, double d2, double fs)
    {
        const double k = 2.0 * fs, k2 = k * k;
        const double A0 = d0 + d1 * k + d2 * k2, A1 = 2.0 * d0 - 2.0 * d2 * k2, A2 = d0 - d1 * k + d2 * k2;
        const double B0 = n0 + n1 * k + n2 * k2, B1 = 2.0 * n0 - 2.0 * n2 * k2, B2 = n0 - n1 * k + n2 * k2;
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    // shelving RBJ (pendenza S = 1)
    void setShelf (bool high, double f0, double dB, double fs)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * f0 / fs;
        const double cw = std::cos (w), sw = std::sin (w), alpha = sw / 2.0 * std::sqrt (2.0);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        double B0, B1, B2, A0, A1, A2;
        if (! high)
        {
            B0 = A * ((A + 1) - (A - 1) * cw + sA); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sA);
            A0 = (A + 1) + (A - 1) * cw + sA;       A1 = -2 * ((A - 1) + (A + 1) * cw);    A2 = (A + 1) + (A - 1) * cw - sA;
        }
        else
        {
            B0 = A * ((A + 1) + (A - 1) * cw + sA); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sA);
            A0 = (A + 1) - (A - 1) * cw + sA;       A1 = 2 * ((A - 1) - (A + 1) * cw);      A2 = (A + 1) - (A - 1) * cw - sA;
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

//==============================================================================================
// Generatore d'inviluppo P-440 (valori in volt sul condensatore da 4,7 µF)
class Envelope
{
public:
    static constexpr float kPeak = 4.3f;   // soglia del comparatore (fine attacco) = livello massimo

    void prepare (double sampleRate) { fs = sampleRate; last = { -1, -1, -1, -1 }; }
    void reset() { v = -0.6f; stage = Stage::Release; }
    void trigger() { stage = Stage::Attack; }

    void set (float a, float d, float s, float r)
    {
        if (a == last[0] && d == last[1] && s == last[2] && r == last[3]) return;
        last = { a, d, s, r };
        constexpr double C = 4.7e-6, rOn = 900.0;                       // 4016 a 5 V
        kA = coef ((rOn + 1.0e6 * logTaper (a)) * C);
        const double wiper = 5000.0 * s * (1.0 - s);                     // resistenza vista dal cursore del sustain
        kD = coef ((rOn + wiper + 1.0e6 * logTaper (d)) * C);
        kR = coef ((rOn + 2200.0 + 1.0e6 * logTaper (r)) * C);
        sus = kPeak * std::clamp (s, 0.0f, 1.0f);
    }

    inline float tick (bool gate)
    {
        if (! gate) stage = Stage::Release;
        switch (stage)
        {
            case Stage::Attack:
                v += (5.0f - v) * kA;
                if (v >= kPeak) { v = kPeak; stage = Stage::Decay; }
                break;
            case Stage::Decay:   v += (sus - v) * kD; break;
            case Stage::Release: v += (-0.6f - v) * kR; break;
        }
        return v / kPeak;      // 1 = picco; leggermente negativo a riposo (clamp del diodo)
    }
    bool active() const { return stage != Stage::Release || v > -0.55f; }

private:
    enum class Stage { Attack, Decay, Release };
    float coef (double tau) const { return (float) (1.0 - std::exp (-1.0 / (tau * fs))); }
    double fs = 96000.0;
    float v = -0.6f, kA = 0.01f, kD = 0.01f, kR = 0.01f, sus = 3.0f;
    Stage stage = Stage::Release;
    std::array<float, 4> last { -1, -1, -1, -1 };
};

//==============================================================================================
// Oscillatore a bassa frequenza P-443
class Lfo
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hp.set (1.0 / (2.0 * kPi * 38500.0 * 50.0e-6), fs);   // 50 µF + (10K + rete dei pesi)
        hpR = hp;
    }
    void reset() { tri = 0.0; dir = 1; count = 0; hp.reset(); hpR.reset(); }

    // reostato da 1M a filo in serie a 10K: f = 30 Hz · 10K / (10K + R)
    static double rateHz (float knob) { return 30.0 * 10.0 / (10.0 + 990.0 * (1.0 - std::clamp ((double) knob, 0.0, 1.0))); }

    inline void tick (float rate)
    {
        const double inc = 4.0 * rateHz (rate) / fs;
        tri += dir * inc;
        if (tri >= 1.0)       { tri = 2.0 - tri;  dir = -1; }
        else if (tri <= -1.0) { tri = -2.0 - tri; dir = 1; count = (count + 1) & 15; }   // HBF4727 sul fronte

        const float t = (float) tri;
        triOut = t;
        sqOut = (float) dir;
        sawOut = t * (float) dir;           // commutazione di polarità: sega a frequenza doppia
        // scala discendente (pesi 47K…470K, amplificatore invertente) e "random" (pesi invertiti)
        const float stair = 1.0f - 2.0f * (float) count / 15.0f;
        const int rev = ((count & 1) << 3) | ((count & 2) << 1) | ((count & 4) >> 1) | ((count & 8) >> 3);
        const float rnd = 0.62f * (2.0f * (float) rev / 15.0f - 1.0f);
        stairOut = hp.tick (stair);
        randOut = hpR.tick (rnd);
    }
    float led() const { return dir > 0 ? 1.0f : 0.0f; }

    float triOut = 0, sqOut = 1, sawOut = 0, stairOut = 0, randOut = 0;

private:
    double fs = 96000.0, tri = 0.0;
    int dir = 1, count = 0;
    DcBlock hp, hpR;
};

//==============================================================================================
// DCO: master clock → rate multiplier a 12 bit → divisore di registro → contatore della forma
class Dco
{
public:
    static constexpr int kPrescale = 16;

    void reset() { phase = 0.0; c = 0; pre = 0; wc = 0; level = 0.0f; blep.reset(); }

    static float levelFor (int wave, uint32_t w, int pw)
    {
        switch (wave)
        {
            case 0:  return (float) (w >> 1) / 15.0f;                                         // sega 16 gradini
            case 1:  return (float) ((w & 16u) ? (15u - (w & 15u)) : (w & 15u)) / 15.0f;       // triangolo
            case 2:  return (w & 16u) ? 1.0f : 0.0f;                                          // quadra
            default: return (int) w < pw ? 1.0f : 0.0f;                                       // impulso
        }
    }

    // clockPerSample = f_clk / fs, n = codice del rate multiplier (1…4095), total = divisione totale
    inline float tick (double clockPerSample, int n, int total, int wave, int pw)
    {
        // un cambio di forma o di larghezza d'impulso modifica subito il livello
        const float now = levelFor (wave, wc, pw);
        if (now != level) { blep.step (now - level, 1.0f); level = now; }

        phase += clockPerSample;
        while (phase >= 1.0)
        {
            phase -= 1.0;
            c = (c + 1u) & 4095u;
            const int j = __builtin_ctz (~c);              // n. di 1 finali → bit del moltiplicatore abilitato
            if (j < 12 && ((n >> (11 - j)) & 1))
            {
                if (++pre >= total)
                {
                    pre = 0;
                    wc = (wc + 1u) & 31u;
                    const float nl = levelFor (wave, wc, pw);
                    if (nl != level)
                    {
                        blep.step (nl - level, (float) (phase / clockPerSample));
                        level = nl;
                    }
                }
            }
        }
        return blep.out (level);
    }

private:
    double phase = 0.0;
    uint32_t c = 0, wc = 0;
    int pre = 0;
    float level = 0.0f;
    BlepLine blep;
};

//==============================================================================================
// Sezione POLY: MK50240 + divisori, una scala a 8 gradini per tasto
class PolySection
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rampInc = (float) (1.0 / (0.0004 * fs));      // chiusura del contatto ≈ 0,4 ms
        reset();
    }
    void reset()
    {
        for (auto& p : pcs) { p.phase = 0.0; p.h = 0; }
        for (auto& n : notes) { n.g = 0.0f; n.on = false; n.lvl = 0.0f; }
        blep.reset();
    }
    void setKey (int m, bool down) { if (m >= 0 && m < 128) notes[(size_t) m].on = down; }
    void allOff() { for (auto& n : notes) n.on = false; }

    // fin = frequenza d'ingresso del MK50240 (2,00024 MHz a 8')
    inline float tick (double fin)
    {
        static constexpr int kDiv[12] = { 478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253 };
        float sum = 0.0f;
        for (int pc = 0; pc < 12; ++pc)
        {
            auto& p = pcs[(size_t) pc];
            // mezzi periodi dell'uscita quadra del MK50240 (f_in / divisore)
            p.phase += 2.0 * fin / (double) kDiv[pc] / fs;
            const int first = pc + 12 * 1;       // nota più bassa gestita per questa classe (C#0…)
            bool any = false;
            for (int m = first; m < 128; m += 12) if (notes[(size_t) m].on || notes[(size_t) m].g > 0.0f) { any = true; break; }
            if (! any) { p.h += (uint64_t) p.phase; p.phase -= std::floor (p.phase); continue; }

            while (p.phase >= 1.0)
            {
                p.phase -= 1.0;
                ++p.h;
                const float d = (float) (p.phase / (2.0 * fin / (double) kDiv[pc] / fs));
                for (int m = first; m < 128; m += 12)
                {
                    auto& n = notes[(size_t) m];
                    if (n.g <= 0.0f && ! n.on) continue;
                    const float nl = levelOf (p.h, m, pc);
                    if (nl != n.lvl) { blep.step (n.g * (nl - n.lvl), d); n.lvl = nl; }
                }
            }
            for (int m = first; m < 128; m += 12)
            {
                auto& n = notes[(size_t) m];
                if (n.on && n.g < 1.0f)       { n.lvl = levelOf (p.h, m, pc); n.g = std::min (1.0f, n.g + rampInc); }
                else if (! n.on && n.g > 0.0f) n.g = std::max (0.0f, n.g - rampInc);
                sum += n.g * n.lvl;
            }
        }
        return blep.out (sum);
    }

private:
    // b = numero di divisioni dalla quadra del MK50240 (DO8 = MIDI 108 a 8')
    static inline float levelOf (uint64_t h, int m, int pc)
    {
        const int b = (108 + pc - m) / 12;
        constexpr float w0 = 1.0f, w1 = 47.0f / 100.0f, w2 = 47.0f / 220.0f, mean = 0.5f * (w0 + w1 + w2);
        auto bit = [h] (int k) { return k >= 0 ? (float) ((h >> k) & 1u) : 0.0f; };
        if (b < 0) return 0.0f;
        return w0 * bit (b) + w1 * bit (b - 1) + w2 * bit (b - 2) - mean;
    }

    struct Pc { double phase = 0.0; uint64_t h = 0; };
    struct Note { bool on = false; float g = 0.0f, lvl = 0.0f; };
    std::array<Pc, 12> pcs;
    std::array<Note, 128> notes;
    double fs = 96000.0;
    float rampInc = 0.05f;
    BlepLine blep;
};

//==============================================================================================
// VCF P-446: quattro stadi OTA con buffer JFET, risoluzione TPT con linearizzazione istantanea
class Ds2Filter
{
public:
    static constexpr float S = 7.85f;     // 2Vt / (220/33220): tensione di saturazione riportata all'uscita
    static constexpr float kRail = 10.5f; // buffer BF244 tra ±12 V
    static constexpr float kStage = 1.5f; // (330/33330) / (220/33220)
    static constexpr float kPole4 = 150.0f / 22.0f;

    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { s.fill (0.0f); e.fill (0.0f); y.fill (0.0f); }

    // controreazione del potenziometro di risonanza (reostato 100K + 3K3 + RV3 ≈ 0,9K)
    static float feedbackFor (float resKnob)
    {
        const float r = 100.0f * (1.0f - std::clamp (resKnob, 0.0f, 1.0f)) + 4.2f;
        return 33.2f / r;
    }

    inline float tick (float x, double fc, float k)
    {
        const double lim = 0.45 * fs;
        const float g = (float) std::tan (kPi * std::min (fc, lim) / fs);
        const float g4 = (float) std::tan (kPi * std::min (fc * kPole4, lim) / fs);
        const float u = kStage * x;
        // due passate: la seconda linearizza le tanh attorno alla soluzione della prima
        std::array<float, 4> est = e;
        for (int pass = 0; pass < 2; ++pass)
        {
            std::array<float, 4> a, b;
            for (int i = 0; i < 4; ++i)
            {
                const float gi = (i == 3 ? g4 : g) * tanhGain (est[(size_t) i] / S);
                a[(size_t) i] = gi / (1.0f + gi);
                b[(size_t) i] = s[(size_t) i] / (1.0f + gi);
            }
            // y4 = A·y1 + B
            const float A = kStage * kStage * kStage * a[1] * a[2] * a[3];
            const float B = a[3] * kStage * (a[2] * kStage * b[1] + b[2]) + b[3];
            y[0] = (a[0] * (u - k * B) + b[0]) / (1.0f + a[0] * k * A);
            y[1] = a[1] * kStage * y[0] + b[1];
            y[2] = a[2] * kStage * y[1] + b[2];
            y[3] = a[3] * kStage * y[2] + b[3];
            // ingressi differenziali (riportati) della soluzione trovata
            est[0] = u - k * y[3] - y[0];
            est[1] = kStage * y[0] - y[1];
            est[2] = kStage * y[1] - y[2];
            est[3] = kStage * y[2] - y[3];
        }
        e = est;
        for (int i = 0; i < 4; ++i)
        {
            s[(size_t) i] = 2.0f * y[(size_t) i] - s[(size_t) i];
            if (std::fabs (s[(size_t) i]) > kRail) s[(size_t) i] = std::copysign (kRail, s[(size_t) i]);
        }
        return std::clamp (y[3], -kRail, kRail);
    }

private:
    double fs = 96000.0;
    std::array<float, 4> s {}, e {}, y {};
};

//==============================================================================================
class Ds2Engine
{
public:
    static constexpr double kClock8 = 1071875.0;    // master clock nominale (DO6 a 8' = 1046,5 Hz)
    static constexpr double kMk8 = 2000240.0;       // ingresso del MK50240 a 8'
    static constexpr int kTopNote = 84;             // ultimo DO della tastiera (codice 4095)
    static constexpr int kLowNote = 41;             // primo FA

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        envF.prepare (fs); envA.prepare (fs);
        lfo1.prepare (fs); lfo2.prepare (fs);
        poly.prepare (fs);
        vcf.prepare (fs);
        for (auto* d : { &dc1, &dc2, &dcN, &dcP }) d->set (1.0, fs);    // 4,7 µF + 33K
        dcOut.set (3.0, fs);
        // rete "pink" del LM307: −Zf / 100K
        {
            const double R1 = 270e3, R2 = 47e3, C1 = 10e-9, C2 = 1e-9;
            pink.setAnalog (R1, R1 * R2 * C1, 0.0, 1.0, R2 * C1 + R1 * C1 + R1 * C2, R1 * R2 * C1 * C2, fs);
            pink.b0 *= -1e-5f; pink.b1 *= -1e-5f; pink.b2 *= -1e-5f;
        }
        whiteLp.setAnalog (1.0, 0.0, 0.0, 1.0, 1.0 / (2.0 * kPi * 18000.0), 0.0, fs);
        lastLow = lastHigh = 99.0f;
        reset();
    }

    void reset()
    {
        dco1.reset(); dco2.reset(); poly.reset(); vcf.reset();
        envF.reset(); envA.reset(); lfo1.reset(); lfo2.reset();
        for (auto* d : { &dc1, &dc2, &dcN, &dcP, &dcOut }) d->reset();
        pink.reset(); whiteLp.reset(); lowSh.reset(); highSh.reset();
        held.fill (false); sustained.fill (false);
        gate = false; delayT = 0.0;
        codeNow = codeTarget = codeFor (60);
    }

    void setParams (const EngineParams& np) { p = np; }

    // ------------------------------------------------------------------- tastiera
    void noteOn (int m)
    {
        if (m < 0 || m > 127 || ! inRange (m)) return;
        held[(size_t) m] = true;
        keysChanged();
    }
    void noteOff (int m)
    {
        if (m < 0 || m > 127) return;
        held[(size_t) m] = false;
        if (sustainPedal && inRange (m)) sustained[(size_t) m] = true;
        keysChanged();
    }
    void setSustainPedal (bool down)
    {
        sustainPedal = down;
        if (! down) { sustained.fill (false); keysChanged(); }
    }
    void allNotesOff() { held.fill (false); sustained.fill (false); keysChanged(); }

    // ------------------------------------------------------------------- audio
    // out: uscita principale; polyOut: uscita POLY separata (solo con polySplit)
    void process (float* out, float* polyOut, int n)
    {
        envF.set (p.vcfA, p.vcfD, p.vcfS, p.vcfR);
        envA.set (p.vcaA, p.vcaD, p.vcaS, p.vcaR);
        if (p.polyLow != lastLow || p.polyHigh != lastHigh)
        {
            lastLow = p.polyLow; lastHigh = p.polyHigh;
            lowSh.setShelf (false, 300.0, 15.0 * p.polyLow, fs);
            highSh.setShelf (true, 1500.0, 15.0 * p.polyHigh, fs);
        }
        const float smooth = (float) (1.0 - std::exp (-1.0 / (0.004 * fs)));
        const int d1 = 8 >> std::clamp (p.o1Range, 0, 3), d2 = 8 >> std::clamp (p.o2Range, 0, 3);
        const float k = Ds2Filter::feedbackFor (p.vcfRes);
        const double glideHz = glideClock();

        for (int i = 0; i < n; ++i)
        {
            // parametri continui con un po' di inerzia (le manopole non hanno scatti)
            sCut += (p.vcfCut - sCut) * smooth;
            sK   += (k - sK) * smooth;
            sV1  += (p.o1Vol - sV1) * smooth;
            sV2  += (p.o2Vol - sV2) * smooth;
            sVn  += (p.noiseVol - sVn) * smooth;
            sVp  += (p.polyVol - sVp) * smooth;
            sVol += (p.volume - sVol) * smooth;
            sP1  += (p.o1Pitch - sP1) * smooth;
            sP2  += (p.o2Pitch - sP2) * smooth;
            sPp  += (p.polyPitch - sPp) * smooth;
            sBend += (p.bend - sBend) * smooth;

            // --- LFO e ritardo
            lfo1.tick (p.lfo1Rate);
            lfo2.tick (p.lfo2Rate);
            if (gate) delayT += 1.0 / fs;
            const float dly = delayGain();
            const float l1 = lfo1Wave() * (p.lfo1Delay ? dly : 1.0f);
            const float l2 = lfo2Wave() * (p.lfo2Delay ? dly : 1.0f);

            // --- LFO's MIXER (P-445)
            auto src = [l1, l2] (int s) { return s == 1 ? l1 : s == 2 ? l2 : s == 3 ? 0.5f * (l1 + l2) : 0.0f; };
            const float vib1 = 1.21f * src (p.mxO1Src) * (float) logTaper (p.mxO1Amt);
            const float vib2 = 1.21f * src (p.mxO2Src) * (float) logTaper (p.mxO2Amt);
            const float vcfMod = src (p.mxVcfSrc) * p.mxVcfAmt;
            const float vcaMod = src (p.mxVcaSrc) * p.mxVcaAmt;
            float pwc = p.mxPwAmt;
            if (p.mxPwSrc == 1) pwc *= 0.5f * (l1 + 1.0f);
            else if (p.mxPwSrc == 2) pwc *= 0.5f * (l2 + 1.0f);
            const int pw = (int) std::lround (16.0f - 14.0f * std::clamp (pwc, 0.0f, 1.0f));
            const float polyMod = 1.21f * src (p.polyLfoSrc) * (float) logTaper (p.polyLfoAmt);

            // --- inviluppi
            const float eF = envF.tick (gate);
            const float eA = envA.tick (gate);

            // --- glide: contatore del codice N
            if (codeNow != codeTarget)
            {
                if (! p.glideOn) codeNow = codeTarget;
                else
                {
                    glideAcc += glideHz / fs;
                    const int64_t steps = (int64_t) glideAcc;
                    glideAcc -= (double) steps;
                    if (codeTarget > codeNow) codeNow = std::min (codeTarget, codeNow + steps);
                    else                      codeNow = std::max (codeTarget, codeNow - steps);
                }
            }

            // --- DCO
            const double bendMul = std::exp2 (sBend * p.bendRange / 12.0);
            const double c1 = kClock8 * std::exp2 (sP1 * 6.0 / 12.0) * bendMul * std::max (0.05, 1.0 + 0.6 * vib1);
            const double c2 = kClock8 * std::exp2 (sP2 * 9.0 / 12.0) * bendMul * std::max (0.05, 1.0 + 0.6 * vib2);
            int n1, t1, n2, t2;
            splitCode (codeNow, d1, n1, t1);
            splitCode (codeNow, d2, n2, t2);
            const float o1 = dco1.tick (c1 / fs, n1, t1, std::clamp (p.o1Wave, 0, 3), pw);
            const float o2 = dco2.tick (c2 / fs, n2, t2, std::clamp (p.o2Wave, 0, 2), pw);

            // --- POLY
            const double fin = kMk8 * (p.polyOct == 0 ? 0.5 : p.polyOct == 2 ? 2.0 : 1.0)
                             * std::exp2 (sPp * 4.0 / 12.0) * std::max (0.05, 1.0 + 0.6 * polyMod);
            float pl = poly.tick (fin);
            pl = highSh.tick (lowSh.tick (pl)) * 1.8f * loadedPot (sVp, 10.0f, 100.0f);
            pl = dcP.tick (pl);

            // --- NOISE
            const float w = whiteLp.tick (noise.bipolar() * 1.73f);
            const float nz = dcN.tick (p.noisePink ? pink.tick (w) * 0.9f : w) * 1.2f;

            // --- mixer d'ingresso del VCF (volt prima dei 33K)
            float x = dc1.tick (o1 * 5.0f) * loadedPot (sV1, 33.0f, 100.0f)
                    + dc2.tick (o2 * 5.0f) * loadedPot (sV2, 33.0f, 100.0f)
                    + nz * loadedPot (sVn, 33.0f, 100.0f);
            const bool polyToFilter = p.polyOn && ! p.polySplit;
            if (polyToFilter) x += pl;

            // --- VCF
            double oct = 10.55 * std::pow ((double) std::clamp (sCut, 0.0f, 1.0f), 0.48)
                       + 9.0 * (double) (p.vcfEnv * eF) + 1.5 * (double) vcfMod;
            double fc = 16.0 * std::exp2 (oct);
            if (p.kbdCtrl) fc *= 1.393 * (double) codeNow / 4095.0;
            fc = std::clamp (fc, 4.0, 0.45 * fs);
            const float yf = vcf.tick (x, fc, sK);

            // --- VCA
            float envSel;
            if (p.vcaSwMode == 0) envSel = p.vcaIn ? eF : eA;
            else { gateLevel += ((gate ? 1.0f : 0.0f) - gateLevel) * smooth; envSel = p.vcaIn ? eA : gateLevel; }
            const float cv = std::max (0.0f, p.vcaEnv * std::max (envSel, 0.0f) + 0.5f * vcaMod);
            constexpr float Sv = 24.5f;
            float y = cv * Sv * fastTanh (yf / Sv);

            // --- uscita (volume generale 50K lin., MAIN / HEAD)
            y = dcOut.tick (y) * 0.075f * loadedPot (sVol, 517.0f, 50.0f);
            if (p.headphones) y = 1.2f * fastTanh (y * 2.0f / 1.2f);
            out[i] = y;
            if (polyOut != nullptr)
                polyOut[i] = (p.polyOn && p.polySplit) ? 0.06f * pl * loadedPot (sVol, 517.0f, 50.0f) : 0.0f;
        }
    }

    float lfoLed (int which) const { return which == 1 ? lfo1.led() : lfo2.led(); }
    bool gateOn() const { return gate; }
    int  monoNote() const { return lastNote; }

    // codice del rate multiplier di una nota (DO6 = 4095); usato anche dai test
    static int64_t codeFor (int m) { return std::max<int64_t> (1, std::llround (4095.0 * std::exp2 ((m - kTopNote) / 12.0))); }

private:
    bool inRange (int m) const { return ! p.originalRange || (m >= kLowNote && m <= kTopNote); }

    void keysChanged()
    {
        int top = -1;
        bool any = false;
        for (int m = 127; m >= 0; --m)
        {
            const bool on = held[(size_t) m] || sustained[(size_t) m];
            poly.setKey (m, on);
            if (on) { any = true; if (top < 0) top = m; }
        }
        const bool wasGate = gate;
        gate = any;
        if (any)
        {
            const bool newNote = top != lastNote;
            lastNote = top;
            codeTarget = codeFor (top);
            if (! wasGate || (p.retrigger && newNote))
            {
                envF.trigger(); envA.trigger();
                if (! wasGate) delayT = 0.0;
            }
        }
        if (! any) delayT = 0.0;
        // senza tasti premuti la tastiera ricorda l'ultima nota (il glide continua)
    }

    // codice N e divisione totale: le note oltre il DO acuto (estensione MIDI) dimezzano la divisione
    static void splitCode (int64_t code, int footDiv, int& n, int& total)
    {
        total = Dco::kPrescale * footDiv;
        double c = (double) code;
        while (c > 4095.0 && total > 1) { c *= 0.5; total >>= 1; }
        n = (int) std::clamp<double> (std::round (c), 1.0, 4095.0);
    }

    // clock del contatore di glide: slider 100K lin., deviatore SPEED RANGE (IN = portamenti lunghi)
    double glideClock() const
    {
        const double fmax = p.glideLong ? 6500.0 : 40000.0;
        return fmax / (1.0 + 49.0 * (1.0 - std::clamp ((double) p.glideSpeed, 0.0, 1.0)));
    }

    float delayGain() const
    {
        if (p.lfoDelay <= 0.001f) return 1.0f;
        const double T = 0.05 + 9.95 * logTaper (p.lfoDelay);
        return (float) std::clamp ((delayT / T - 0.3) / 0.7, 0.0, 1.0);
    }

    float lfo1Wave() const
    {
        switch (p.lfo1Wave)
        {
            case 0:  return -lfo1.sawOut;
            case 1:  return lfo1.triOut;
            case 2:  return lfo1.sqOut;
            default: return lfo1.sawOut;
        }
    }
    float lfo2Wave() const
    {
        switch (p.lfo2Wave)
        {
            case 0:  return lfo2.randOut;
            case 1:  return lfo2.triOut;
            case 2:  return lfo2.sqOut;
            default: return lfo2.stairOut;
        }
    }

    EngineParams p;
    double fs = 96000.0;
    Envelope envF, envA;
    Lfo lfo1, lfo2;
    Dco dco1, dco2;
    PolySection poly;
    Ds2Filter vcf;
    DcBlock dc1, dc2, dcN, dcP, dcOut;
    Biquad pink, whiteLp, lowSh, highSh;
    Rng noise { 0x9e3779b9u };
    float lastLow = 99.0f, lastHigh = 99.0f;

    std::array<bool, 128> held {}, sustained {};
    bool sustainPedal = false, gate = false;
    int lastNote = 60;
    int64_t codeNow = 1, codeTarget = 1;
    double glideAcc = 0.0, delayT = 0.0;
    float gateLevel = 0.0f;

    float sCut = 0.6f, sK = 0.33f, sV1 = 0.7f, sV2 = 0.0f, sVn = 0.0f, sVp = 0.6f, sVol = 0.7f;
    float sP1 = 0.0f, sP2 = 0.0f, sPp = 0.0f, sBend = 0.0f;
};

} // namespace ds2
