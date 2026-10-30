#include <SoftClipper.hpp>

#include "AllocationWatch.hpp"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace
{
using Samples = std::vector<float>;

constexpr float kKnee = dsp::SoftClipper::knee;
constexpr float kCeiling = dsp::SoftClipper::ceiling;

/** Values that cover every region of the transfer function: the endpoints of the transparent band
    and their neighbours, powers of two up to the largest finite float, and a seeded spread across
    the whole range. */
std::vector<float> probeValues()
{
    std::vector<float> out;
    const float exact[] = {0.0f,  -0.0f,  1.0f,   -1.0f,   std::nextafter(1.0f, 0.0f),
                           -std::nextafter(1.0f, 0.0f), std::nextafter(1.0f, 2.0f),
                           -std::nextafter(1.0f, 2.0f), std::numeric_limits<float>::denorm_min(),
                           -std::numeric_limits<float>::denorm_min(), 0.5f,  -0.5f,
                           1.0000001f,                  -1.0000001f,  2.0f,
                           10.0f,                       1000.0f,      std::numeric_limits<float>::max(),
                           -std::numeric_limits<float>::max()};
    for (const float value : exact)
        out.push_back(value);

    for (int exponent = -30; exponent <= 127; ++exponent)
        for (const float scale : {1.0f, 1.5f, 1.9f})
        {
            const double magnitude = std::ldexp(static_cast<double>(scale), exponent);
            if (magnitude <= std::numeric_limits<double>::max())
            {
                const float value = static_cast<float>(magnitude);
                if (std::isfinite(value))
                {
                    out.push_back(value);
                    out.push_back(-value);
                }
            }
        }

    std::mt19937 generator(20261029U);
    std::uniform_real_distribution<float> spread(0.0f, 20.0f);
    for (int i = 0; i < 20000; ++i)
    {
        const float value = spread(generator);
        out.push_back(value);
        out.push_back(-value);
    }
    return out;
}

bool strictlyNonDecreasing(const std::vector<std::pair<float, float>>& ordered)
{
    for (std::size_t i = 1; i < ordered.size(); ++i)
        if (ordered[i].second < ordered[i - 1].second)
            return false;
    return true;
}
}

TEST_CASE("the clipper is the identity, bit for bit, at and below 0 dBFS")
{
    /* Criterion B1, and the range that matters runs up to exactly full scale: D-010's dial endpoints
       render A or B with no headroom at the full-scale level, so |x| == knee has to be untouched
       rather than merely close. Nothing above the knee is touched by this criterion — item 02
       criterion C5 says a hot input is *meant* to be shaped. */
    dsp::SoftClipper clipper;

    for (const float value : probeValues())
    {
        if (std::abs(value) > kKnee)
            continue;
        INFO("x=" << value);
        CHECK(dsp::SoftClipper::shape(value) == value);
        CHECK(clipper.process(value) == value);
    }

    /* A dense linear walk of the transparent band, both signs. */
    for (int i = 0; i <= 100000; ++i)
    {
        const float value = static_cast<float>(2.0 * static_cast<double>(i) / 100000.0 - 1.0);
        if (std::abs(value) > kKnee)
            continue;
        REQUIRE(dsp::SoftClipper::shape(value) == value);
    }

    CHECK(dsp::SoftClipper::shape(-0.0f) == -0.0f);
    CHECK(clipper.nonFiniteSamples() == 0);

    /*  The absolute pin. Every loop above is parameterised on the class's own knee, so a knee moved
        anywhere at all leaves them green — the discriminator that moves it to 0.25 turns only the
        literal expectations in the other cases red, by accident. The criterion is that the transparent
        band reaches exactly 0 dBFS, so say that in literals: the knee is unity, and a sample well
        inside full scale is untouched whichever way the constant drifts. */
    CHECK(kKnee == 1.0f);
    CHECK(dsp::SoftClipper::shape(0.5f) == 0.5f);
    CHECK(dsp::SoftClipper::shape(-0.9f) == -0.9f);
}

TEST_CASE("the shape is monotone, odd, and joins the identity line with a continuous derivative")
{
    /* Criterion B2. Three separate claims, because each rules out a different shortcut: a hard clamp
       at the ceiling would be monotone and bounded but has no derivative at the knee, and an
       even-symmetric curve would be smooth but put DC into a balanced signal. */
    std::vector<std::pair<float, float>> ordered;
    for (const float value : probeValues())
        ordered.emplace_back(value, dsp::SoftClipper::shape(value));
    std::sort(ordered.begin(), ordered.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    CHECK(strictlyNonDecreasing(ordered));

    for (const float value : probeValues())
    {
        INFO("x=" << value);
        CHECK(dsp::SoftClipper::shape(-value) == -dsp::SoftClipper::shape(value));
    }

    /* Left of the knee the slope is exactly the identity's. Right of it the slope tends to the same
       1: tanh's derivative at the origin is 1, so the join has no corner.

       h stops at 1e-3 because below that the difference of two float samples is float resolution
       itself — at h = 1e-4 the quotient carries 1.2e-4 of rounding noise, which is larger than any
       tolerance worth stating and says nothing about the curve. A hard clamp gives slope 0 here, so
       1e-3 still discriminates it. */
    const float behind = std::nextafter(kKnee, 0.0f);
    CHECK((dsp::SoftClipper::shape(kKnee) - dsp::SoftClipper::shape(behind))
          == (kKnee - behind));

    for (const float h : {0.01f, 0.001f})
    {
        const float forward =
            (dsp::SoftClipper::shape(kKnee + h) - dsp::SoftClipper::shape(kKnee)) / h;
        INFO("h=" << h << " forward slope " << forward);
        CHECK(std::abs(forward - 1.0f) <= 1e-3f);
    }

    /* A C1 join whose second derivative does not blow up at the knee. The curvature is measured in
       double from float samples, so the bound sits above the float cancellation noise and below what
       a derivative step would produce: for a hard clamp this ratio is 1/h, i.e. 1e3 here. */
    for (const float h : {0.01f, 0.001f})
    {
        const double curvature = (static_cast<double>(dsp::SoftClipper::shape(kKnee + h))
                                  - 2.0 * static_cast<double>(dsp::SoftClipper::shape(kKnee))
                                  + static_cast<double>(dsp::SoftClipper::shape(kKnee - h)))
                                 / (static_cast<double>(h) * static_cast<double>(h));
        INFO("h=" << h << " curvature " << curvature);
        CHECK(std::abs(curvature) <= 1.0);
    }
}

TEST_CASE("the output never leaves the ceiling, however far above it the input is")
{
    /* Criterion B3, over every finite magnitude the type can hold. The tolerance is the criterion's:
       the bound is reached as a limit, so equality is expected and not a violation. */
    dsp::SoftClipper clipper;
    for (const float value : probeValues())
    {
        if (!std::isfinite(value))
            continue;
        const float shaped = clipper.process(value);
        INFO("x=" << value << " out=" << shaped);
        CHECK(std::abs(shaped) <= kCeiling + 1e-6f);
        CHECK(std::isfinite(shaped));
    }

    /* And the largest finite float, both signs, which is where an overflow in the ramp would show. */
    const float top = std::numeric_limits<float>::max();
    CHECK(dsp::SoftClipper::shape(top) == kCeiling);
    CHECK(dsp::SoftClipper::shape(-top) == -kCeiling);
    CHECK(clipper.nonFiniteSamples() == 0);
}

TEST_CASE("the stage holds no state, so what a sample becomes never depends on its neighbours")
{
    /* Criterion B4. The same value is presented in three contexts — after silence, after a
       full-scale blast, after a non-finite sample — and becomes the same thing every time. */
    dsp::SoftClipper clipper;

    const float probe = 1.75f;
    const float afterSilence = clipper.process(0.0f);
    clipper.process(1.0e6f);
    const float afterBlast = clipper.process(probe);
    clipper.process(std::numeric_limits<float>::quiet_NaN());
    const float afterNonFinite = clipper.process(probe);

    CHECK(afterBlast == dsp::SoftClipper::shape(probe));
    CHECK(afterNonFinite == afterBlast);
    CHECK(afterSilence == 0.0f);

    Samples stream = {0.5f, 3.0f, 0.5f, 1.0f, 1.0f, -0.25f, -3.0f, -0.25f};
    clipper.processChannel(stream.data(), stream.size());
    CHECK(stream[0] == 0.5f);
    CHECK(stream[2] == stream[0]);
    CHECK(stream[3] == 1.0f);
    CHECK(stream[4] == stream[3]);
    CHECK(stream[5] == -0.25f);
    CHECK(stream[7] == stream[5]);
    CHECK(stream[6] == dsp::SoftClipper::shape(-3.0f));

    /* Interleaving a second stream changes nothing about the first. */
    Samples same = {0.5f, 3.0f, 0.5f, 1.0f, 1.0f, -0.25f, -3.0f, -0.25f};
    Samples other(8, 0.9f);
    dsp::SoftClipper second;
    second.processChannel(other.data(), other.size());
    second.processChannel(same.data(), same.size());
    CHECK(same == stream);
}

TEST_CASE("a non-finite sample becomes zero, is counted, and leaves nothing behind")
{
    /* Criterion B5: never propagated, always counted, and the very next finite sample behaves
       normally — which is the whole argument for keeping the counter off the signal path. */
    dsp::SoftClipper clipper;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    CHECK(clipper.process(nan) == 0.0f);
    CHECK(clipper.nonFiniteSamples() == 1);
    CHECK(clipper.process(infinity) == 0.0f);
    CHECK(clipper.process(-infinity) == 0.0f);
    CHECK(clipper.nonFiniteSamples() == 3);

    CHECK(clipper.process(0.75f) == 0.75f);
    CHECK(clipper.process(4.0f) == dsp::SoftClipper::shape(4.0f));
    CHECK(clipper.nonFiniteSamples() == 3);

    clipper.resetNonFiniteCount();
    CHECK(clipper.nonFiniteSamples() == 0);

    Samples stream;
    std::size_t expected = 0;
    for (int i = 0; i < 500; ++i)
    {
        if (i % 7 == 0)
        {
            stream.push_back(i % 14 == 0 ? nan : infinity);
            ++expected;
        }
        else
        {
            stream.push_back(static_cast<float>((i % 61) - 30) * 0.4f);
        }
    }
    clipper.processChannel(stream.data(), stream.size());
    CHECK(clipper.nonFiniteSamples() == expected);
    for (const float sample : stream)
        CHECK(std::isfinite(sample));
}

TEST_CASE("the stage is channel-independent and allocates nothing")
{
    /* Criterion B8. Two channels driven with the same values through one instance must agree, and
       the process path must leave the allocator alone. */
    const std::size_t hop = 128;
    const std::size_t calls = 64;

    auto makeStream = [] {
        Samples out(hop * calls);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = static_cast<float>(std::sin(0.021 * static_cast<double>(i)) * 3.0
                                        + std::sin(0.4 * static_cast<double>(i)));
        return out;
    };

    Samples left = makeStream();
    Samples right = makeStream();
    const Samples original = left;

    dsp::SoftClipper clipper;
    long allocations = 0;
    {
        allocwatch::Watch watch;
        for (std::size_t call = 0; call < calls; ++call)
        {
            clipper.processChannel(left.data() + call * hop, hop);
            clipper.processChannel(right.data() + call * hop, hop);
        }
        allocations = allocwatch::allocations;
    }
    CHECK(allocations == 0);
    CHECK(left == right);

    bool sameShape = true;
    for (std::size_t i = 0; i < left.size(); ++i)
        sameShape = sameShape && left[i] == dsp::SoftClipper::shape(original[i]);
    CHECK(sameShape);
    CHECK(clipper.nonFiniteSamples() == 0);
}
