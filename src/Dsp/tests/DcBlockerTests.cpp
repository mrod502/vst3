#include <DcBlocker.hpp>

#include "AllocationWatch.hpp"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <vector>

namespace
{
/** processChannel must not allocate. One definition per program, which is why DspTests is one binary
    for both stages and shares allocwatch's state through the header. */
bool armNow()
{
    return allocwatch::armed;
}
}

void* operator new(std::size_t size)
{
    if (armNow())
        ++allocwatch::allocations;
    void* memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

void* operator new[](std::size_t size)
{
    if (armNow())
        ++allocwatch::allocations;
    void* memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
using Samples = std::vector<float>;

constexpr double kPi = 3.14159265358979323846;
constexpr double kCorner = dsp::DcBlocker::cornerHz;

/** The rates criterion A2 names, i.e. everything the shell is asked to run at in anger. */
constexpr double kRates[] = {44100.0, 48000.0, 96000.0, 192000.0};

/** A sine the analysis window resolves exactly, so the correlation below has no leakage term to
    argue about at the 0.05 dB these criteria are set at.

    Two ways to get there, because each stops working at one end of the band: counting whole periods
    is exact down to a fraction of a hertz but cannot express a period under two samples; taking a
    whole bin of a fixed window is fine at the top and resolves nothing at the bottom, where one bin
    of a short window already sits above the corner being looked for. */
struct Tone
{
    double frequency;
    std::size_t window;
};

Tone chooseTone(double target, double sampleRate)
{
    if (target * 512.0 < sampleRate)
    {
        const double period = std::max(2.0, std::round(sampleRate / target));
        return {sampleRate / period, static_cast<std::size_t>(period) * 4};
    }

    const std::size_t window = 8192;
    const double bin = std::max(1.0, std::round(target * static_cast<double>(window) / sampleRate));
    return {bin * sampleRate / static_cast<double>(window), window};
}

Samples noiseOf(std::size_t count, unsigned seed)
{
    std::mt19937 generator(seed);
    std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
    Samples out(count);
    for (auto& sample : out)
        sample = spread(generator);
    return out;
}

/** The amplitude of one sine component of `stream`, by correlation over the whole window, which the
    caller has filled at an exact bin. `phaseFrom` continues a sine across calls. */
double amplitudeOf(const Samples& stream, double step, double phaseFrom)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t i = 0; i < stream.size(); ++i)
    {
        const double angle = step * (phaseFrom + static_cast<double>(i));
        real += static_cast<double>(stream[i]) * std::cos(angle);
        imaginary += static_cast<double>(stream[i]) * std::sin(angle);
    }
    return 2.0 * std::sqrt(real * real + imaginary * imaginary)
           / static_cast<double>(stream.size());
}

double meanOf(const Samples& stream)
{
    double sum = 0.0;
    for (const float sample : stream)
        sum += static_cast<double>(sample);
    return sum / static_cast<double>(stream.size());
}

double decibels(double gain)
{
    return 20.0 * std::log10(gain);
}

/** Settles the stage, then runs one sine through it and reports the gain against the same sine
    measured before the stage. The stage is reset first, so each measurement is independent of the
    one before it. */
double measureGain(dsp::DcBlocker& blocker, double target, double sampleRate)
{
    blocker.reset();
    const Tone tone = chooseTone(target, sampleRate);
    const std::size_t window = tone.window;
    const double step = 2.0 * kPi * tone.frequency / sampleRate;

    Samples chunk(256, 0.0f);
    std::size_t index = 0;
    const std::size_t warmUp = static_cast<std::size_t>(blocker.settlingSamples()) + window;
    while (index < warmUp)
    {
        const std::size_t count = std::min<std::size_t>(chunk.size(), warmUp - index);
        for (std::size_t i = 0; i < count; ++i, ++index)
            chunk[i] = static_cast<float>(std::sin(step * static_cast<double>(index)));
        blocker.processChannel(0, chunk.data(), count);
    }

    Samples analysis(window);
    for (std::size_t i = 0; i < window; ++i, ++index)
        analysis[i] = static_cast<float>(std::sin(step * static_cast<double>(index)));

    const double in = amplitudeOf(analysis, step, static_cast<double>(index - window));
    blocker.processChannel(0, analysis.data(), window);
    const double out = amplitudeOf(analysis, step, static_cast<double>(index - window));
    return out / in;
}

/** Where the stage is 3 dB down, found by geometric bisection on the gain measured above. */
double measureCornerHz(double sampleRate)
{
    dsp::DcBlocker blocker;
    blocker.prepare(sampleRate, 1);

    constexpr double kHalfPower = 0.70710678118654752440;
    double low = kCorner * 0.25;
    double high = kCorner * 4.0;
    for (int step = 0; step < 24; ++step)
    {
        const double middle = std::sqrt(low * high);
        if (measureGain(blocker, middle, sampleRate) < kHalfPower)
            low = middle;
        else
            high = middle;
    }
    return std::sqrt(low * high);
}
}

TEST_CASE("the blocker's coefficients reject DC by construction and its corner is where it says")
{
    /* b0 + b1 + b2 == 0 is what makes DC rejection a property of the coefficients rather than a
       limit the recursion approaches, and it is only exact because b1 and b2 are derived from b0.
       The -3 dB point is the design corner: measured by bisection at four rates, and A2's bound is
       1 % of design. */
    for (const double rate : kRates)
    {
        const double measured = measureCornerHz(rate);
        const double error = std::abs(measured / kCorner - 1.0);
        INFO("sampleRate=" << rate << " measured corner " << measured << " Hz");
        CHECK(error <= 0.01);
        /* The relative bound above cannot see the corner itself move — a stage raised a hundredfold
           is still exactly at its own design point — so the "a few Hz" of M3 §4.3 is pinned here in
           absolute terms as well, with room around D-013's figure. Without these two checks the
       discriminator that raises the corner by 100x turns nothing red. */
        CHECK(measured >= 1.0);
        CHECK(measured <= 10.0);
        /* Reported because D-013 quotes it: the corner is a design choice, and this is the figure it
           actually lands on at every rate the shell accepts. */
        std::cout << "  corner  " << rate << " Hz rate -> " << measured
                  << " Hz (" << (100.0 * error) << " % off design)\n";
    }
}

TEST_CASE("a constant input falls to 1e-6 of itself inside the documented settling bound")
{
    /* Criterion A1. The bound is the class's own settlingSamples(), derived from the realised poles,
       so this is the documented figure being held rather than a length chosen to pass. */
    for (const double rate : kRates)
    {
        for (const double offset : {0.001, 0.01, 1.0})
        {
            dsp::DcBlocker blocker;
            blocker.prepare(rate, 1);
            const int bound = blocker.settlingSamples();
            INFO("rate=" << rate << " offset=" << offset << " settling bound=" << bound);
            REQUIRE(bound > 0);

            Samples stream(static_cast<std::size_t>(bound), static_cast<float>(offset));
            blocker.processChannel(0, stream.data(), stream.size());

            const double limit = 1e-6 * std::abs(offset);
            CHECK(std::abs(static_cast<double>(stream.back())) <= limit);
            std::cout << "  DC      " << rate << " Hz rate, offset " << offset << ": bound "
                      << bound << " samples, residue " << std::abs(stream.back()) << "\n";

            /* A re-prepare at the same rate clears the state rather than carrying a residue, so the
               second run is bit-identical to the first — which is also the determinism half of A8. */
            Samples restarted(static_cast<std::size_t>(bound), static_cast<float>(offset));
            blocker.prepare(rate, 1);
            blocker.processChannel(0, restarted.data(), restarted.size());
            CHECK(restarted == stream);
            CHECK(std::abs(static_cast<double>(restarted.back())) <= limit);
        }
    }
}

TEST_CASE("the passband is flat to 0.05 dB from eight times the corner, and never peaks")
{
    /* Criteria A2 and A3. Eight times the corner is where a first-order stage would sit 0.067 dB
       down, which is the reason this stage is second order. */
    for (const double rate : kRates)
    {
        for (const double multiple : {8.0, 16.0, 32.0, 256.0})
        {
            dsp::DcBlocker blocker;
            blocker.prepare(rate, 1);
            const double gain = measureGain(blocker, kCorner * multiple, rate);
            INFO("rate=" << rate << " f=" << kCorner * multiple << " Hz gain " << decibels(gain)
                         << " dB");
            CHECK(std::abs(decibels(gain)) <= 0.05);
            if (multiple == 8.0)
            {
                /* The binding point of A2, reported for D-013: a first-order stage would read
                   -0.067 dB here. */
                std::cout << "  flat    " << rate << " Hz rate at " << kCorner * multiple
                          << " Hz: " << decibels(gain) << " dB\n";
            }
        }

        /*  The same bound at absolute frequencies, which is what makes this criterion bite. Stated
           only in multiples of the corner it is true of any corner at all — raising the corner a
           hundredfold leaves it eight-multiples-flat by construction — so the part of the band the
           stage must not touch is named in hertz: from the bottom of the musical range to the top of
           the audible one. The two highest points are skipped below 48 kHz rates only because the
           analysis window has to stay inside the band being measured. */
        for (const double absolute : {40.0, 100.0, 1000.0, 10000.0})
        {
            if (absolute > 0.25 * rate)
                continue;
            dsp::DcBlocker blocker;
            blocker.prepare(rate, 1);
            const double gain = measureGain(blocker, absolute, rate);
            INFO("rate=" << rate << " f=" << absolute << " Hz gain " << decibels(gain) << " dB");
            CHECK(std::abs(decibels(gain)) <= 0.05);
        }

        /* And nothing above the corner exceeds unity by more than 0.01 dB. The sweep is geometric
           from the corner and stops below Nyquist, because the analysis has to stay in the band. */
        const double topFrequency = 0.49 * rate;
        const double ratio = topFrequency / kCorner;
        dsp::DcBlocker blocker;
        blocker.prepare(rate, 1);
        double worst = 0.0;
        double worstFrequency = 0.0;
        for (int step = 0; step <= 48; ++step)
        {
            const double frequency =
                kCorner * std::pow(ratio, static_cast<double>(step) / 48.0);
            const double gain = measureGain(blocker, frequency, rate);
            if (gain > worst)
            {
                worst = gain;
                worstFrequency = frequency;
            }
        }
        INFO("rate=" << rate << " worst gain " << decibels(worst) << " dB at " << worstFrequency
                     << " Hz");
        CHECK(decibels(worst) <= 0.01);
        std::cout << "  peak    " << rate << " Hz rate: worst gain " << decibels(worst)
                  << " dB at " << worstFrequency << " Hz\n";
    }
}

TEST_CASE("a DC offset on a carrier is removed and the carrier is not")
{
    /* Criterion A4: 100 Hz at 0.5 with +0.25 DC on top, 48 kHz. Both the DC and the carrier are
       exact bins of the analysis window, so the two readings do not leak into each other. */
    const double rate = 48000.0;
    const std::size_t window = 30720; ///< 100 Hz is bin 64 of it, and DC is bin 0
    const double step = 2.0 * kPi * 100.0 / rate;

    dsp::DcBlocker blocker;
    blocker.prepare(rate, 1);

    Samples chunk(256, 0.0f);
    std::size_t index = 0;
    const std::size_t warmUp = static_cast<std::size_t>(blocker.settlingSamples()) + window;
    auto feed = [&](std::size_t count) {
        for (std::size_t i = 0; i < count; ++i, ++index)
            chunk[i] = static_cast<float>(0.5 * std::sin(step * static_cast<double>(index)) + 0.25);
        blocker.processChannel(0, chunk.data(), count);
    };
    while (index < warmUp)
        feed(std::min<std::size_t>(chunk.size(), warmUp - index));

    Samples analysis(window);
    for (std::size_t i = 0; i < window; ++i, ++index)
        analysis[i] = static_cast<float>(0.5 * std::sin(step * static_cast<double>(index)) + 0.25);

    const double carrierIn = amplitudeOf(analysis, step, static_cast<double>(index - window));
    const double directIn = meanOf(analysis);
    blocker.processChannel(0, analysis.data(), window);
    const double carrierOut = amplitudeOf(analysis, step, static_cast<double>(index - window));
    const double directOut = meanOf(analysis);

    const double rejectionDb = decibels(std::abs(directOut) / std::abs(directIn));
    INFO("DC in " << directIn << " out " << directOut << " -> " << rejectionDb << " dB;"
                  << " carrier in " << carrierIn << " out " << carrierOut << " -> "
                  << decibels(carrierOut / carrierIn) << " dB");
    CHECK(rejectionDb <= -60.0);
    CHECK(std::abs(decibels(carrierOut / carrierIn)) <= 0.05);
}

TEST_CASE("the step settles inside the bound with a decaying transient, not a sustained one")
{
    /* Criterion A5. "No sustained ringing" is read as the envelope falling in every window: the
       max |y| of each successive quarter of the bound has to be below the one before it, which a
       marginally-stable stage would fail and a slow-but-decaying one passes. */
    for (const double rate : kRates)
    {
        dsp::DcBlocker blocker;
        blocker.prepare(rate, 1);
        const std::size_t bound = static_cast<std::size_t>(blocker.settlingSamples());
        INFO("rate=" << rate << " bound=" << bound);

        Samples stream(bound * 2, 0.0f);
        for (std::size_t i = 16; i < stream.size(); ++i)
            stream[i] = 1.0f;
        blocker.processChannel(0, stream.data(), stream.size());

        const std::size_t slice = std::max<std::size_t>(1, bound / 4);
        double previous = std::numeric_limits<double>::infinity();
        bool decaying = true;
        for (std::size_t start = 16; start + slice <= bound * 2; start += slice)
        {
            double worst = 0.0;
            for (std::size_t i = start; i < start + slice; ++i)
                worst = std::max(worst, std::abs(static_cast<double>(stream[i])));
            if (worst >= previous)
                decaying = false;
            previous = worst;
        }
        CHECK(decaying);

        CHECK(std::abs(static_cast<double>(stream[bound + 15])) <= 1e-6);

        Samples tail(stream.begin() + static_cast<std::ptrdiff_t>(bound), stream.end());
        CHECK(std::abs(meanOf(tail)) <= 1e-6);
    }
}

TEST_CASE("the same stream is bit-identical in chunks of 1, 7, 128, 4096 and a seeded sequence")
{
    /* Criterion A6: the state advances one sample at a time, so hostBlock cannot reach the output. */
    const std::size_t total = 20000;
    const Samples source = noiseOf(total, 20261029U);

    dsp::DcBlocker reference;
    reference.prepare(48000.0, 1);
    Samples whole = source;
    reference.processChannel(0, whole.data(), whole.size());

    dsp::DcBlocker chunked;
    chunked.prepare(48000.0, 1);
    Samples stream = source;
    for (const std::size_t size : {std::size_t{1}, std::size_t{7}, std::size_t{128},
                                   std::size_t{4096}})
    {
        for (std::size_t start = 0; start < total; start += size)
            chunked.processChannel(0, stream.data() + start, std::min(size, total - start));
        CHECK(stream == whole);
        chunked.reset();
        stream = source;
    }

    /* A seeded chunk sequence: one draw per chunk, and the same length advances the cursor as the
       one processed, or the pass would be measuring a re-cut of the stream rather than the stream. */
    std::mt19937 generator(60613U);
    std::uniform_int_distribution<int> spread(1, 5000);
    for (std::size_t start = 0; start < total;)
    {
        const std::size_t count =
            std::min(static_cast<std::size_t>(spread(generator)), total - start);
        chunked.processChannel(0, stream.data() + start, count);
        start += count;
    }
    CHECK(stream == whole);
}

TEST_CASE("a silent channel stays exactly zero beside a driven one, and so does silence")
{
    /* Criterion A7: per-channel state, and 0 in gives 0 out exactly — no noise floor, no subnormal
       tail inside the documented bound. */
    const std::size_t total = 40000;
    dsp::DcBlocker blocker;
    blocker.prepare(48000.0, 2);

    Samples driven = noiseOf(total, 90210U);
    Samples silent(total, 0.0f);
    blocker.processChannel(0, driven.data(), driven.size());
    blocker.processChannel(1, silent.data(), silent.size());

    bool silentKept = true;
    for (const float sample : silent)
        silentKept = silentKept && sample == 0.0f;
    CHECK(silentKept);
    CHECK(driven != silent);

    const Samples quiet(total, 0.0f);
    dsp::DcBlocker fresh;
    fresh.prepare(96000.0, 1);
    Samples silence = quiet;
    fresh.processChannel(0, silence.data(), silence.size());
    bool exact = true;
    for (const float sample : silence)
        exact = exact && sample == 0.0f;
    CHECK(exact);

    /* A driven signal that then goes silent: inside the documented bound the decay is still in the
       normal range, so no subnormal reaches the audio thread's own flush. */
    dsp::DcBlocker decay;
    decay.prepare(48000.0, 1);
    const std::size_t bound = static_cast<std::size_t>(decay.settlingSamples());
    Samples ramp(bound * 2, 1.0f);
    for (std::size_t i = bound; i < ramp.size(); ++i)
        ramp[i] = 0.0f;
    decay.processChannel(0, ramp.data(), ramp.size());
    bool noSubnormals = true;
    for (const float sample : ramp)
        noSubnormals = noSubnormals && std::fpclassify(sample) != FP_SUBNORMAL;
    CHECK(noSubnormals);
}

TEST_CASE("processChannel allocates nothing and repeats bit-identically")
{
    /* Criterion A8. prepare is allowed to size the state; the process path is not. */
    const std::size_t hop = 128;
    dsp::DcBlocker blocker;
    blocker.prepare(48000.0, 2);
    Samples first = noiseOf(hop * 64, 4242U);
    Samples second = first;

    long allocations = 0;
    {
        allocwatch::Watch watch;
        for (std::size_t call = 0; call < 64; ++call)
        {
            blocker.processChannel(0, first.data() + call * hop, hop);
            blocker.processChannel(1, second.data() + call * hop, hop);
        }
        allocations = allocwatch::allocations;
    }
    CHECK(allocations == 0);

    dsp::DcBlocker again;
    again.prepare(48000.0, 2);
    Samples repeat = noiseOf(hop * 64, 4242U);
    Samples other = repeat;
    for (std::size_t call = 0; call < 64; ++call)
    {
        again.processChannel(0, repeat.data() + call * hop, hop);
        again.processChannel(1, other.data() + call * hop, hop);
    }
    CHECK(repeat == first);
    CHECK(other == second);
}
