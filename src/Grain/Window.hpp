#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

namespace grain
{
/** The analysis window applied to both inputs.

    Periodic Hann is `0.5·(1 − cos(2 pi i / L))`. The symmetric form (denominator `L − 1`) is
    not COLA, so event gain would depend on which side of the hop grid an event falls; that is
    the window choice M2 corrected, not a tuning knob. Rectangular is not a musically offered
    window — it exists as the known-bad reference the hop-rate ripple test calibrates against. */
class GrainWindow
{
public:
    enum class Kind
    {
        PeriodicHann,
        Rectangular,
    };

    /** Builds the table in double and casts once; belongs in prepare. */
    GrainWindow(int length, Kind kind);

    int length() const noexcept { return lengthValue; }

    const float* data() const noexcept { return samples.data(); }

    float operator[](int index) const noexcept
    {
        assert(index >= 0 && index < lengthValue);
        return samples[static_cast<std::size_t>(index)];
    }

private:
    int lengthValue;
    std::vector<float> samples;
};

/** Sum of `window[phase + j·hop]` over every `j` keeping the index inside the window,
    accumulated in double. The accumulation width is the point: summing the same table in
    float produces failures a correct window cannot explain. */
[[nodiscard]] double colaSumAtPhase(const GrainWindow& window, int hop, int phase) noexcept;

/** The COLA sum a correct window holds at every phase, and the constant the provisional wet
    gain is the reciprocal of. */
[[nodiscard]] double idealColaSum(int grainLength, int hop) noexcept;
}
