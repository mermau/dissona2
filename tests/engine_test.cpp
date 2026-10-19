// Test del motore DS-2 (senza JUCE).  c++ -O2 -std=c++17 tests/engine_test.cpp -Isrc -o engine_test
#include "dsp/DS2Engine.h"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace ds2;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FALLITO: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static void writeWav (const std::string& path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    auto w32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t bytes = (uint32_t) x.size() * 2;
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (1); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 2); w16 (2); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (float v : x) { const int s = (int) std::lround (std::clamp (v, -1.0f, 1.0f) * 32767.0f); w16 ((uint16_t) (int16_t) s); }
    std::fclose (f);
}

static std::vector<float> render (Ds2Engine& e, int n)
{
    std::vector<float> y ((size_t) n), pp ((size_t) n);
    e.process (y.data(), pp.data(), n);
    return y;
}

// frequenza fondamentale stimata dai passaggi per lo zero in salita (dopo un passa-basso)
static double pitchOf (const std::vector<float>& x, double fs, size_t from)
{
    std::vector<double> lp (x.size());
    double a = 0.0, b = 0.0;
    for (size_t i = 0; i < x.size(); ++i) { a += 0.05 * (x[i] - a); b += 0.05 * (a - b); lp[i] = b; }
    double first = -1.0, last = -1.0; int count = 0;
    for (size_t i = from + 1; i < lp.size(); ++i)
        if (lp[i - 1] < 0.0 && lp[i] >= 0.0)
        {
            const double t = (double) (i - 1) + lp[i - 1] / (lp[i - 1] - lp[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? fs * count / (last - first) : 0.0;
}

static double rms (const std::vector<float>& x, size_t from = 0)
{
    double s = 0.0; for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) std::max<size_t> (1, x.size() - from));
}

static EngineParams base()
{
    EngineParams p;
    p.o1Vol = 1.0f; p.o1Wave = 0; p.o1Range = 2;
    p.o2Vol = 0.0f;
    p.vcfCut = 1.0f; p.vcfEnv = 0.0f; p.vcfRes = 0.0f;
    p.vcaEnv = 1.0f; p.vcaA = 0.0f; p.vcaD = 0.0f; p.vcaS = 1.0f; p.vcaR = 0.0f;
    p.volume = 1.0f;
    p.tolerances = false;
    return p;
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const double fs = 96000.0;

    std::printf ("1) intonazione dei DCO\n");
    for (int range = 0; range < 4; ++range)
        for (int m : { 41, 53, 60, 69, 84 })
        {
            Ds2Engine e; e.prepare (fs);
            auto p = base(); p.o1Range = range; p.o1Wave = 2;
            e.setParams (p);
            e.noteOn (m);
            auto y = render (e, (int) fs);
            const double expect = 440.0 * std::exp2 ((m - 69) / 12.0) * std::exp2 (range - 2);
            const double got = pitchOf (y, fs, (size_t) (0.2 * fs));
            const double cents = 1200.0 * std::log2 (got / expect);
            if (m == 60 || std::fabs (cents) > 4.0) std::printf ("   reg %d nota %d: %.2f Hz (attesi %.2f, %+.1f cent)\n", range, m, got, expect, cents);
            CHECK (std::fabs (cents) < 4.0, "nota %d registro %d: %+.1f cent", m, range, cents);
        }

    std::printf ("2) codice del rate multiplier\n");
    CHECK (Ds2Engine::codeFor (84) == 4095, "DO acuto = %lld", (long long) Ds2Engine::codeFor (84));
    std::printf ("   FA grave = %lld, DO acuto = %lld\n", (long long) Ds2Engine::codeFor (41), (long long) Ds2Engine::codeFor (84));

    std::printf ("3) soglia di autooscillazione del filtro\n");
    {
        double onset = -1.0;
        for (int step = 60; step <= 100; ++step)
        {
            Ds2Engine e; e.prepare (fs);
            auto p = base(); p.o1Vol = 0.0f; p.vcfCut = 0.6f; p.vcfRes = (float) step / 100.0f;
            e.setParams (p);
            e.noteOn (60);
            auto y = render (e, (int) (0.6 * fs));
            if (rms (y, (size_t) (0.4 * fs)) > 0.01) { onset = step / 10.0; break; }
        }
        std::printf ("   autooscillazione da RESONANCE = %.1f (manuale: ≈ 8½)\n", onset);
        CHECK (onset >= 8.0 && onset <= 9.2, "soglia %.1f", onset);
    }

    std::printf ("4) stabilità con tutto al massimo\n");
    {
        Ds2Engine e; e.prepare (fs);
        auto p = base(); p.o2Vol = 1.0f; p.noiseVol = 1.0f; p.polyOn = true; p.polyVol = 1.0f;
        p.vcfRes = 1.0f; p.vcfEnv = 1.0f; p.mxVcfSrc = 3; p.mxVcfAmt = 1.0f; p.mxO1Src = 2; p.mxO1Amt = 1.0f;
        p.lfo1Rate = 1.0f; p.lfo2Rate = 1.0f; p.lfo2Wave = 0;
        e.setParams (p);
        for (int m : { 41, 45, 48, 52, 55, 60, 64, 67, 72, 84 }) e.noteOn (m);
        auto y = render (e, (int) fs);
        bool finite = true; float peak = 0.0f;
        for (float v : y) { finite = finite && std::isfinite (v); peak = std::max (peak, std::fabs (v)); }
        std::printf ("   picco %.3f\n", peak);
        CHECK (finite && peak < 2.0f, "uscita instabile (picco %.3f)", peak);
    }

    std::printf ("5) livelli\n");
    {
        Ds2Engine e; e.prepare (fs);
        auto p = base(); p.vcfCut = 0.75f;
        e.setParams (p); e.noteOn (48);
        auto y = render (e, (int) fs);
        float peak = 0.0f; for (size_t i = (size_t) (0.2 * fs); i < y.size(); ++i) peak = std::max (peak, std::fabs (y[i]));
        std::printf ("   sega 8' DO3, filtro a 7,5: picco %.3f, rms %.3f\n", peak, rms (y, (size_t) (0.2 * fs)));
    }

    std::printf ("6) CPU\n");
    {
        Ds2Engine e; e.prepare (fs);
        auto p = base(); p.o2Vol = 0.8f; p.noiseVol = 0.3f; p.polyOn = true; p.mxO1Src = 1; p.mxO1Amt = 0.3f;
        e.setParams (p);
        for (int m : { 48, 52, 55, 60, 64, 67, 72, 76 }) e.noteOn (m);
        std::vector<float> y (96000), pp (96000);
        const auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < 10; ++r) e.process (y.data(), pp.data(), (int) y.size());
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("   10 s di audio a 96 kHz (2 DCO + 8 note poly) in %.3f s → %.1f %% di un core\n", sec, sec * 10.0);
    }

    std::printf ("7) demo audio in %s\n", dir.c_str());
    {
        struct Demo { const char* name; void (*set) (EngineParams&); std::vector<int> notes; };
        const std::vector<Demo> demos {
            { "saw_8", [] (EngineParams& p) { p.o1Wave = 0; p.vcfCut = 0.85f; }, { 36, 48, 60, 72 } },
            { "tri_8", [] (EngineParams& p) { p.o1Wave = 1; p.vcfCut = 0.85f; }, { 36, 48, 60, 72 } },
            { "pulse_lfo", [] (EngineParams& p) { p.o1Wave = 3; p.mxPwSrc = 1; p.mxPwAmt = 1.0f; p.lfo1Rate = 0.7f; p.vcfCut = 0.85f; }, { 48 } },
            { "jazzbass", [] (EngineParams& p) { p.o1Wave = 0; p.o1Range = 1; p.vcfCut = 0.35f; p.vcfEnv = 0.7f; p.vcfRes = 0.85f;
                                                 p.vcfA = 0.0f; p.vcfD = 0.35f; p.vcfS = 0.0f; p.vcaD = 0.45f; p.vcaS = 0.5f; }, { 36, 43, 41, 36 } },
            { "poly_strings", [] (EngineParams& p) { p.o1Vol = 0.0f; p.polyOn = true; p.polyVol = 1.0f; p.polyLfoSrc = 1; p.polyLfoAmt = 0.25f;
                                                     p.lfo1Rate = 0.85f; p.vcfCut = 0.8f; p.vcaA = 0.5f; }, { 48, 55, 60, 64 } },
            { "selfosc", [] (EngineParams& p) { p.o1Vol = 0.0f; p.vcfRes = 0.95f; p.vcfCut = 0.6f; p.kbdCtrl = true; }, { 60, 72, 84 } },
        };
        for (const auto& d : demos)
        {
            Ds2Engine e; e.prepare (fs);
            auto p = base(); d.set (p); e.setParams (p);
            std::vector<float> all;
            for (int m : d.notes)
            {
                const bool chord = std::string (d.name) == "poly_strings";
                if (chord) { for (int c : d.notes) e.noteOn (c); }
                else e.noteOn (m);
                auto a = render (e, (int) (0.7 * fs));
                if (chord) { for (int c : d.notes) e.noteOff (c); } else e.noteOff (m);
                auto b = render (e, (int) (0.3 * fs));
                all.insert (all.end(), a.begin(), a.end()); all.insert (all.end(), b.begin(), b.end());
                if (chord) break;
            }
            // decimazione semplice a 48 kHz per l'ascolto
            std::vector<float> out (all.size() / 2);
            for (size_t i = 0; i < out.size(); ++i) out[i] = 0.5f * (all[2 * i] + all[2 * i + 1]);
            writeWav (dir + "/" + d.name + ".wav", out, 48000);
        }
    }

    std::printf ("8) priorità delle note MONO\n");
    for (int pr = 0; pr < 3; ++pr)
    {
        Ds2Engine e; e.prepare (fs);
        auto p = base(); p.o1Wave = 2; p.priority = pr; e.setParams (p);
        e.noteOn (60); render (e, 2000); e.noteOn (72); render (e, 2000); e.noteOn (48);
        auto y = render (e, (int) (0.6 * fs));
        const double got = pitchOf (y, fs, (size_t) (0.2 * fs));
        const int expect = pr == 0 ? 72 : pr == 1 ? 48 : 48;   // ultima suonata = 48
        const double cents = 1200.0 * std::log2 (got / (440.0 * std::exp2 ((expect - 69) / 12.0)));
        std::printf ("   priorità %d: suona %.1f Hz (%+.1f cent dalla nota %d)\n", pr, got, cents, expect);
        CHECK (std::fabs (cents) < 5.0, "priorità %d", pr);
    }
    {   // ultima suonata: rilasciando l'ultima torna alla precedente ancora tenuta
        Ds2Engine e; e.prepare (fs);
        auto p = base(); p.o1Wave = 2; p.priority = 2; e.setParams (p);
        e.noteOn (60); render (e, 2000); e.noteOn (55); render (e, 2000); e.noteOn (64); render (e, 2000); e.noteOff (64);
        auto y = render (e, (int) (0.6 * fs));
        const double got = pitchOf (y, fs, (size_t) (0.2 * fs));
        CHECK (std::fabs (1200.0 * std::log2 (got / (440.0 * std::exp2 ((55 - 69) / 12.0)))) < 5.0, "ritorno alla nota precedente");
    }

    std::printf ("9) accordatura generale\n");
    {
        Ds2Engine e; e.prepare (fs);
        auto p = base(); p.o1Wave = 2; p.tuneA4 = 432.0f; e.setParams (p);
        e.noteOn (69);
        auto y = render (e, (int) fs);
        const double got = pitchOf (y, fs, (size_t) (0.2 * fs));
        std::printf ("   La4 a 432 Hz: %.2f Hz\n", got);
        CHECK (std::fabs (got - 432.0) < 1.0, "accordatura %.2f", got);
    }

    std::printf ("10) tolleranze analogiche\n");
    {
        double maxc = 0.0;
        for (uint32_t seed = 1; seed < 40; ++seed)
        {
            Ds2Engine e; e.prepare (fs); e.setSeed (seed * 2654435761u);
            auto p = base(); p.o1Wave = 2; p.tolerances = true; e.setParams (p);
            e.noteOn (69);
            auto y = render (e, (int) (0.6 * fs));
            maxc = std::max (maxc, std::fabs (1200.0 * std::log2 (pitchOf (y, fs, (size_t) (0.2 * fs)) / 440.0)));
        }
        std::printf ("   scostamento massimo d'intonazione su 39 esemplari: %.1f cent\n", maxc);
        CHECK (maxc < 4.0 && maxc > 0.2, "scostamento %.1f cent", maxc);
    }

    std::printf ("11) ingresso esterno\n");
    {
        for (int open = 0; open < 2; ++open)
        {
            Ds2Engine e; e.prepare (fs);
            auto p = base(); p.o1Vol = 0.0f; p.extVol = 1.0f; p.extOpen = open == 1; p.vcfCut = 0.8f; e.setParams (p);
            const int n = (int) (0.5 * fs);
            std::vector<float> in ((size_t) n), y ((size_t) n), pp ((size_t) n);
            for (int i = 0; i < n; ++i) in[(size_t) i] = 0.5f * std::sin (2.0 * kPi * 220.0 * i / fs);
            e.process (y.data(), pp.data(), n, in.data());
            const double r = rms (y, (size_t) (0.2 * fs));
            std::printf ("   %s, nessun tasto: rms %.4f\n", open ? "VCA OPEN" : "VCA GATE", r);
            CHECK (open ? r > 0.02 : r < 1.0e-4, "ingresso esterno %s", open ? "open" : "gate");
        }
    }

    std::printf ("12) riposo\n");
    {
        Ds2Engine e; e.prepare (fs);
        auto p = base(); e.setParams (p);
        e.noteOn (60); render (e, (int) (0.2 * fs)); e.noteOff (60);
        for (int b = 0; b < 200; ++b) render (e, 480);   // blocchi come in una DAW
        CHECK (e.isSleeping(), "il motore non va a riposo dopo il rilascio");
        e.noteOn (60);
        auto y = render (e, (int) (0.2 * fs));
        CHECK (rms (y, (size_t) (0.1 * fs)) > 0.05, "il motore non si risveglia");
        std::printf ("   riposo dopo il rilascio e risveglio alla nota: %s\n", e.isSleeping() ? "NO" : "ok");
        std::vector<float> a ((size_t) fs), b ((size_t) fs);
        Ds2Engine idle; idle.prepare (fs); idle.setParams (p);
        idle.process (a.data(), b.data(), 9600);
        const auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < 100; ++r) idle.process (a.data(), b.data(), (int) fs);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("   CPU a riposo: %.2f %% di un core\n", sec);
    }

    std::printf (failures ? "\n%d TEST FALLITI\n" : "\nTutti i test superati\n", failures);
    return failures ? 1 : 0;
}
