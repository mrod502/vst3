#pragma once

#include "Fft.hpp"

#include <memory>

namespace fft
{
/** IFft over juce::dsp::FFT — the one adapter that knows a JUCE transform exists.

    The layout and scaling in Fft.hpp are imposed on the engine, not inherited from it. JUCE's
    real transforms are in-place over a 2F-float buffer, so this owns that scratch and copies
    in and out; the per-engine differences in forward gain are measured once in the constructor
    and divided out of both directions. Measured, the shipped engines all land on 1.0 — vDSP's
    raw zrip forward gain of 2.0 is cancelled by JUCE's own 0.5 — so the calibration is a
    tripwire for the next engine, not a per-platform fudge.

    The gain is measured rather than keyed to the compile target because which engine wins is
    decided at runtime: JUCE takes the highest-priority engine whose create() succeeds, and the
    FFTW engine only exists if it can dlopen the library. A hardcoded per-platform constant would
    be wrong the moment a Mac has IPP or MKL installed, or a Linux host loses FFTW.

    forwardGain() and inverseGain() report the installed engine's raw gains, so an engine swap
    shows up as a number rather than as a level change downstream.

    One instance is driven by one thread: the scratch is per-instance and unsynchronised. */
class JuceFft final : public IFft
{
public:
    struct Impl;

    /** Allocates, builds the JUCE transform and calibrates: belongs in prepareToPlay, never on
        the audio path. @param sizeF transform size F, a power of two >= 2. */
    explicit JuceFft(int sizeF);
    ~JuceFft() override;

    JuceFft(const JuceFft&) = delete;
    JuceFft& operator=(const JuceFft&) = delete;

    int size() const noexcept override { return sizeValue; }

    void forward(const float* in, float* bins) const noexcept override;

    void inverse(const float* bins, float* out) const noexcept override;

    /** The installed engine's un-normalised forward gain as measured at construction. */
    float forwardGain() const noexcept;

    /** The installed engine's inverse gain as measured at construction. */
    float inverseGain() const noexcept;

private:
    int sizeValue;
    std::unique_ptr<Impl> impl;
};
}
