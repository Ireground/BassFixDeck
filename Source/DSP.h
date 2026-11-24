#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace bfd
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// Topology-preserving state variable filter (Andrew Simper). Stays stable and
// click-free while its frequency is being moved around every few samples,
// which is exactly what a pitch-tracking EQ needs.
struct SVF
{
    double ic1 = 0, ic2 = 0;
    double a1 = 1, a2 = 0, a3 = 0, k = 1, bellMul = 0;

    void reset() { ic1 = ic2 = 0; }

    void setBand (double sr, double fc, double q)
    {
        fc = std::clamp (fc, 10.0, sr * 0.45);
        const double g = std::tan (kPi * fc / sr);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        bellMul = 0;
    }

    void setBell (double sr, double fc, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        fc = std::clamp (fc, 10.0, sr * 0.45);
        const double g = std::tan (kPi * fc / sr);
        k = 1.0 / (q * A);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        bellMul = k * (A * A - 1.0);
    }

    // returns {low, band, high}
    inline void tick (double x, double& lp, double& bp, double& hp)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }

    inline double bell (double x)
    {
        double lp, bp, hp;
        tick (x, lp, bp, hp);
        return x + bellMul * bp;
    }

    inline double band (double x)  { double lp, bp, hp; tick (x, lp, bp, hp); return bp; }
    inline double high (double x)  { double lp, bp, hp; tick (x, lp, bp, hp); return hp; }
};

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setLowpass (double sr, double fc, double q)
    {
        const double w = 2.0 * kPi * fc / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (1.0 - c) * 0.5 / a0; b1 = (1.0 - c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
    }
    void reset() { z1 = z2 = 0; }
    inline double tick (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

//==============================================================================
// Monophonic bass pitch tracker (YIN) running on a decimated (~6 kHz) copy of
// the signal, so it is cheap enough to run on every instance.
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 6000.0));
        decRate = sr / decim;
        lp1.setLowpass (sr, std::min (900.0, decRate * 0.4), 0.5412);
        lp2.setLowpass (sr, std::min (900.0, decRate * 0.4), 1.3066);
        win    = (int) std::lround (decRate * 0.040);
        tauMax = (int) std::ceil  (decRate / 27.0);
        tauMin = std::max (2, (int) std::floor (decRate / 650.0));
        hop    = std::max (8, (int) std::lround (decRate * 0.006));
        bufLen = 1;
        while (bufLen < (win + tauMax + 4) * 2) bufLen <<= 1;
        ring.assign ((size_t) bufLen, 0.0);
        frame.assign ((size_t) (win + tauMax + 2), 0.0);
        diff.assign ((size_t) (tauMax + 2), 0.0);
        reset();
    }

    void reset()
    {
        lp1.reset(); lp2.reset();
        std::fill (ring.begin(), ring.end(), 0.0);
        writePos = 0; decCount = 0; hopCount = 0;
        pitch = 0; confidence = 0;
        hist[0] = hist[1] = hist[2] = 0;
        newResult = false;
    }

    // Feed one full-rate mono sample.
    inline void push (double x)
    {
        const double y = lp2.tick (lp1.tick (x));
        if (++decCount < decim) return;
        decCount = 0;
        ring[(size_t) writePos] = y;
        writePos = (writePos + 1) & (bufLen - 1);
        if (++hopCount >= hop) { hopCount = 0; analyse(); }
    }

    bool  takeNewResult()  { const bool r = newResult; newResult = false; return r; }
    double getPitch() const       { return pitch; }       // Hz, 0 = no note
    double getConfidence() const  { return confidence; }  // 0..1
    double getLevelDb() const     { return levelDb; }

private:
    void analyse()
    {
        const int n = win + tauMax;
        int idx = (writePos - n + bufLen) & (bufLen - 1);
        double energy = 0;
        for (int i = 0; i < n; ++i)
        {
            frame[(size_t) i] = ring[(size_t) idx];
            idx = (idx + 1) & (bufLen - 1);
        }
        for (int i = n - win; i < n; ++i) energy += frame[(size_t) i] * frame[(size_t) i];
        levelDb = 10.0 * std::log10 (energy / win + 1e-20);

        double found = 0, conf = 0;
        if (levelDb > -62.0)
        {
            // YIN difference function + cumulative mean normalisation
            diff[0] = 1.0;
            double running = 0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                double s = 0;
                const double* a = frame.data();
                const double* b = frame.data() + tau;
                for (int j = 0; j < win; ++j) { const double d = a[j] - b[j]; s += d * d; }
                running += s;
                diff[(size_t) tau] = running > 0 ? s * tau / running : 1.0;
            }

            int best = -1;
            for (int tau = tauMin; tau < tauMax; ++tau)
            {
                if (diff[(size_t) tau] < 0.18)
                {
                    while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                    best = tau; break;
                }
            }
            if (best < 0)
            {
                double mn = 1e9;
                for (int tau = tauMin; tau < tauMax; ++tau)
                    if (diff[(size_t) tau] < mn) { mn = diff[(size_t) tau]; best = tau; }
                if (mn > 0.4) best = -1;
            }

            if (best > 0)
            {
                double t = best;
                if (best > 1 && best < tauMax)
                {
                    const double s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
                    const double den = s0 + s2 - 2.0 * s1;
                    if (std::abs (den) > 1e-12) t += 0.5 * (s0 - s2) / den;
                }
                found = decRate / t;
                conf = std::clamp (1.0 - diff[(size_t) best], 0.0, 1.0);
            }
        }

        // 3-point median removes single-frame octave glitches
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = found;
        double a = hist[0], b = hist[1], c = hist[2];
        if (a > b) std::swap (a, b);
        if (b > c) std::swap (b, c);
        if (a > b) std::swap (a, b);
        pitch = (found > 0 && b > 0) ? b : (found > 0 ? found : 0.0);
        confidence = found > 0 ? conf : 0.0;
        newResult = true;
    }

    double sr = 44100, decRate = 6000;
    int decim = 8, win = 240, tauMax = 220, tauMin = 9, hop = 36, bufLen = 1024;
    int writePos = 0, decCount = 0, hopCount = 0;
    Biquad lp1, lp2;
    std::vector<double> ring, frame, diff;
    double pitch = 0, confidence = 0, levelDb = -120;
    double hist[3] {};
    bool newResult = false;
};

//==============================================================================
// The correction curve: how many dB to apply at the note's fundamental, given
// the detected pitch. Shared by the DSP and the GUI so the display is exact.
struct CurveParams
{
    double boomNote = 55, boomDepth = 6, bodyNote = 110, bodyDepth = 6, rangeSt = 12;
};

inline double smooth01 (double x) { x = std::clamp (x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }

inline double boomAmount (double f0, const CurveParams& p)   // 0..1
{
    if (f0 <= 0) return 0;
    const double st = 12.0 * std::log2 (p.boomNote / f0);   // semitones below threshold
    return smooth01 (st / std::max (0.5, p.rangeSt));
}

inline double bodyAmount (double f0, const CurveParams& p)   // 0..1
{
    if (f0 <= 0) return 0;
    const double st = 12.0 * std::log2 (f0 / p.bodyNote);   // semitones above threshold
    return smooth01 (st / std::max (0.5, p.rangeSt));
}

inline double correctionDb (double f0, const CurveParams& p)
{
    return -p.boomDepth * boomAmount (f0, p) + p.bodyDepth * bodyAmount (f0, p);
}

inline const char* noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((midi % 12) + 12) % 12];
}
} // namespace bfd
