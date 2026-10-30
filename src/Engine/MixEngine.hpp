#pragma once

#include "Engine.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

namespace dsp
{
class DcBlocker;
class SoftClipper;
}

namespace engine
{
/** The mix law's weights at dial position p in [-1, 1], with t = |p|: equal power through the
    travel, exact at p = 0 and |p| = 1.

    The three exact positions are values rather than a tolerance because `sin`/`cos` do not produce
    them: measured at -O0 x86-64 float, cosf(pi/2f) == -4.371138829e-08, which at the wet peaks
    recorded in D-009 leaves 8.8e-08 to 7.3e-07 of the unwanted signal in a sum that M3 §7 requires
    to be bit-exact at p = -1 and p = +1. */
struct MixWeights
{
    float dry;
    float wet;
};

inline MixWeights mixLawWeights(float p) noexcept
{
    constexpr float kHalfPi = 1.57079632679489661923f;
    const float t = p < 0.0f ? -p : p;
    if (t == 0.0f)
        return {0.0f, 1.0f};
    if (t >= 1.0f)
        return {1.0f, 0.0f};
    return {sinf(t * kHalfPi), cosf(t * kHalfPi)};
}

/** The M3 mix law over any Engine's wet stream.

    Per output sample:

        out = g * [ beta * ( w_dry * dry + w_wet * conv ) + (1 - beta) * A ]

    with `w_dry`/`w_wet` from mixLawWeights, `dry` the dry A bus for p < 0 and the dry B bus for
    p >= 0, `g` the output gain (0 dB is exactly 1.0f) and `beta` the effect coefficient, 1 in the
    effect and 0 bypassed onto dry A. All three coefficients are ramped in the output-sample domain,
    so their trajectories depend on absolute sample position and never on a host callback.

    Both dry signals are the hop the scheduler is draining, so they reach the host at exactly R like
    everything else in the drained stream: this class adds no delay line, which is why prepare,
    reset, latencySamples and wetDelaySamples forward to the inner engine untouched. The wet stream
    is never additionally delayed, and the law contributes no gain of its own — the inner engine's
    own wet scale stays the whole wet gain table.

    Two optional safety stages wrap the law, in the order M3 item 2 decision C fixes: the DC blocker
    shapes `conv` before the law sees it, and the soft clipper bounds the summed output after the
    gain, so driving the gain harder is limited rather than unbounded. Both are absent unless they
    are handed in, and a stage that is not installed is not applied at all — that is how the harness
    renders the same chain as engine physics and as what ships. Neither one touches a dry bus, moves
    R or W, or allocates on the process path.

    The three targets are written on the thread that calls processHop, by the shell reading its
    parameter state; nothing here is shared across threads, so no lock is taken. prepare and reset
    snap the coefficients onto their targets, because both drop the buffered stream (the scheduler
    re-applies its latency pre-fill) and a ramp would otherwise spend its time constant replaying a
    gesture the transport has already discarded.

    prepare allocates; processHop allocates nothing, takes no lock and throws nothing. */
class MixEngine final : public Engine
{
public:
    /** Ramp times, recorded in D-011 and realised in samples against the prepare-time sample
        rate. Each is a value to confirm by ear on the user's host, not a figure to defend here. */
    static constexpr double blendRampMs = 20.0;
    static constexpr double outputGainRampMs = 20.0;
    static constexpr double bypassRampMs = 10.0;

    /** The law with no safety stages: what the harness calls the raw wet chain. */
    explicit MixEngine(std::unique_ptr<Engine> innerToUse) noexcept;

    /** @param innerToUse    the wet source; the law owns it for the lifetime of the stage.
        @param wetBlocker    DC blocker for the convolved stream, applied before the law.
        @param outputClipper output safety stage, applied last, after the gain.

        Two constructors rather than defaulted unique_ptr arguments: a default of nullptr would make
        every one-argument caller construct and destroy a unique_ptr of a type they never see, which
        needs the complete type in MixEngine.hpp and would pull the stage headers through the seam. */
    MixEngine(std::unique_ptr<Engine> innerToUse,
              std::unique_ptr<dsp::DcBlocker> wetBlocker,
              std::unique_ptr<dsp::SoftClipper> outputClipper) noexcept;

    /** Defined where the two stage types are, because both members are unique_ptrs of them. */
    ~MixEngine() override;

    void prepare(const EngineConfig&) override;
    void reset() override;

    int latencySamples() const override;
    int wetDelaySamples() const override;

    void processHop(const float* const* a, const float* const* b, float* const* out,
                    int numChannels) override;

    /** @param dialPosition p in [-1, 1]: negative blends dry A, positive dry B, 0 is pure wet. */
    void setBlendTarget(float dialPosition) noexcept;

    /** @param decibels output gain; 0 dB is exactly unity. Converted once here, never per sample. */
    void setOutputGainTarget(float decibels) noexcept;

    /** Bypass is a crossfade onto the dry A path, so it switches no routing and cannot move R. */
    void setBypassTarget(bool bypassed) noexcept;

private:
    /** A straight line between two values over a fixed number of samples.

        The last step assigns the target rather than evaluating the line at progress 1.0: measured
        2026-10-29, start + (target - start) * 1.0f lands on 0.00099992752 for a 1.0 to 0.001 ramp,
        which is not the target, and an endpoint criterion cannot be met on a rounding of it. */
    struct Ramp
    {
        explicit Ramp(float initial = 0.0f) noexcept
            : value(initial), start(initial), target(initial)
        {
        }

        float value = 0.0f;
        float start = 0.0f;
        float target = 0.0f;
        int elapsed = 0;
        int length = 1;

        float step() noexcept
        {
            if (elapsed + 1 >= length)
            {
                elapsed = length;
                return value = target;
            }
            ++elapsed;
            return value = start + (target - start)
                                 * (static_cast<float>(elapsed) / static_cast<float>(length));
        }

        void retarget(float newTarget) noexcept
        {
            if (newTarget == target)
                return;
            start = value;
            target = newTarget;
            elapsed = 0;
        }

        void snap() noexcept
        {
            value = target;
            start = target;
            elapsed = length;
        }
    };

    void fillCoefficients() noexcept;
    void snapRamps() noexcept;

    std::unique_ptr<Engine> inner;
    std::unique_ptr<dsp::DcBlocker> wetBlocker;
    std::unique_ptr<dsp::SoftClipper> outputClipper;
    EngineConfig config;

    Ramp blendRamp{0.0f}; ///< dial position: the centre, which is the parameter's own default
    Ramp gainRamp{1.0f};  ///< linear output gain: unity, so an ungained stage is not silenced
    Ramp bypassRamp{1.0f}; ///< effect coefficient: 1 fully in the effect, 0 bypassed onto dry A

    /** Sign of the last dial position the ramp produced, before any zero-crossing snap: the dry bus
        swaps where its weight is exactly zero, and never anywhere else. */
    float previousBlend = 0.0f;

    /** One hop per channel of wet scratch, sized in prepare only: the same rule that keeps
        HopScheduler's pointer arrays stable for the whole audio thread. */
    std::vector<float> scratch;
    std::vector<float*> scratchPtrs;

    /** Per-output-sample coefficients, one hop deep each, rebuilt once per hop and applied to
        every channel identically: out = coefA * a + coefB * b + coefWet * conv. */
    std::vector<float> coefA;
    std::vector<float> coefB;
    std::vector<float> coefWet;
};
}
