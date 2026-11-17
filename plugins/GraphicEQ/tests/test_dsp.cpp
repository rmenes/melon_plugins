// DSP core test (does not need JUCE):  g++ -std=c++17 -O2 -I../Source test_dsp.cpp -o test_dsp
#include "EqDsp.h"
#include <cstdio>
#include <random>

static void evaluate (int n, double fs, const char* name, const double* targets, bool precise)
{
    geq::Design d;
    d.configure (n, fs);
    double g[31];
    d.filterGains (targets, precise, g);
    double maxCenter = 0;
    for (int k = 0; k < n; ++k)
        maxCenter = std::max (maxCenter, std::abs (d.responseDb (g, d.frequency (k)) - targets[k]));
    std::printf ("  %2d bands %-11s %-8s max error at centre frequencies: %5.2f dB\n", n, name,
                 precise ? "precise" : "simple", maxCenter);
}

int main()
{
    const double fs = 48000;
    std::mt19937 rng (1);
    std::uniform_real_distribution<double> u (-12, 12);

    for (int n : { 10, 31 })
    {
        double allUp[31], zig[31], rnd[31];
        for (int i = 0; i < 31; ++i) { allUp[i] = 12; zig[i] = (i % 2 ? 12 : -12); rnd[i] = u (rng); }
        for (bool p : { false, true })
        {
            evaluate (n, fs, "all +12", allUp, p);
            evaluate (n, fs, "zigzag", zig, p);
            evaluate (n, fs, "random", rnd, p);
        }
    }

    // Engine test: mode switching and stability
    geq::Engine e;
    e.prepare (fs, 2, 10);
    double t[31];
    for (int i = 0; i < 31; ++i) t[i] = u (rng);
    e.setTargets (10, t, true);
    e.snapToTargets();
    std::vector<float> l (4800), r (4800);
    float* ch[2] = { l.data(), r.data() };
    double peak = 0;
    bool finite = true;
    for (int blk = 0; blk < 200; ++blk)
    {
        for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = (float) std::sin (0.05 * (double) i);
        e.setTargets (blk < 50 ? 10 : 31, t, true);
        e.process (ch, 2, (int) l.size());
        for (float v : l) { finite &= std::isfinite (v); peak = std::max (peak, (double) std::abs (v)); }
    }
    std::printf ("\nEngine: active bands after switch = %d, finite output = %s, peak = %.2f\n",
                 e.activeBands(), finite ? "yes" : "NO", peak);
    return (finite && e.activeBands() == 31) ? 0 : 1;
}
