#include "Window.hpp"

#include <cmath>

namespace grain
{
namespace
{
constexpr double kTwoPi = 6.28318530717958647692528676655900577;
}

GrainWindow::GrainWindow(int length, Kind kind)
    : lengthValue(length), samples(static_cast<std::size_t>(length < 0 ? 0 : length))
{
    assert(length > 0);

    for (int i = 0; i < lengthValue; ++i)
    {
        const double value = kind == Kind::PeriodicHann
                                 ? 0.5 * (1.0 - std::cos(kTwoPi * static_cast<double>(i)
                                                        / static_cast<double>(lengthValue)))
                                 : 1.0;
        samples[static_cast<std::size_t>(i)] = static_cast<float>(value);
    }
}

double colaSumAtPhase(const GrainWindow& window, int hop, int phase) noexcept
{
    assert(hop > 0 && phase >= 0 && phase < hop);

    double sum = 0.0;
    for (int i = phase; i < window.length(); i += hop)
        sum += static_cast<double>(window[i]);
    return sum;
}

double idealColaSum(int grainLength, int hop) noexcept
{
    assert(hop > 0 && grainLength > 0);
    return static_cast<double>(grainLength) / (2.0 * static_cast<double>(hop));
}
}
