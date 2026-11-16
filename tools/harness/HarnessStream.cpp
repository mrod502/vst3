#include "HarnessStream.hpp"

#include <SoftClipper.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace harness
{

Stream noiseStream(int channels, int samples, unsigned seed, float amplitude)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> spread(-amplitude, amplitude);
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] = spread(rng);
    return data;
}

Stream constantStream(int channels, int samples, float value)
{
    return Stream(static_cast<std::size_t>(channels),
                  std::vector<float>(static_cast<std::size_t>(samples), value));
}

Stream impulseStream(int channels, int samples, int at)
{
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples), 0.0f));
    for (int channel = 0; channel < channels; ++channel)
        data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(at)] = 1.0f;
    return data;
}

/** The same material with one channel lit and the rest exactly zero.

    `impulseStream` writes an impulse into every channel at once, which is why no leg above it can
    localise a response to one source channel — the whole content of M4 §7's independence row. §5.20's
    M4 and M5 legs feed one channel at a time from here. */
Stream singleImpulse(int channels, int samples, int channel, int at)
{
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples), 0.0f));
    data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(at)] = 1.0f;
    return data;
}

namespace
{
constexpr double kTwoPi = 6.283185307179586476925286766559;

float phaseFromSeed(std::mt19937 &rng)
{
    return std::uniform_real_distribution<float>(0.0f, static_cast<float>(kTwoPi))(rng);
}
}

Stream silenceStream(int channels, int samples)
{
    return constantStream(channels, samples, 0.0f);
}

Stream sineStream(int channels, int samples, unsigned seed, float frequencyHz, float amplitude)
{
    std::mt19937 rng(seed);
    const double step = kTwoPi * frequencyHz / kSampleRate;
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
    {
        const double phase = phaseFromSeed(rng);
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] = static_cast<float>(
                amplitude * std::sin(phase + step * static_cast<double>(i)));
    }
    return data;
}

Stream chirpStream(int channels, int samples, unsigned seed, float startHz, float endHz,
                   float amplitude)
{
    std::mt19937 rng(seed);
    const double delta = static_cast<double>(endHz) - static_cast<double>(startHz);
    const double sweepSeconds = static_cast<double>(samples) / kSampleRate;
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
    {
        const double phase = phaseFromSeed(rng);
        for (int i = 0; i < samples; ++i)
        {
            const double seconds = static_cast<double>(i) / kSampleRate;
            const double phaseTurns = static_cast<double>(startHz) * seconds
                                      + delta * seconds * seconds
                                            / (2.0 * sweepSeconds);
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] = static_cast<float>(
                amplitude * std::sin(phase + kTwoPi * phaseTurns));
        }
    }
    return data;
}

Stream squareStream(int channels, int samples, unsigned seed, float frequencyHz, float amplitude)
{
    std::mt19937 rng(seed);
    const double step = kTwoPi * frequencyHz / kSampleRate;
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
    {
        const double phase = phaseFromSeed(rng);
        for (int i = 0; i < samples; ++i)
        {
            const bool high = std::sin(phase + step * static_cast<double>(i)) >= 0.0;
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] =
                high ? amplitude : -amplitude;
        }
    }
    return data;
}

Stream dcOffsetStream(int channels, int samples, unsigned seed, float offset, float noiseAmplitude)
{
    const Stream noise = noiseStream(channels, samples, seed, noiseAmplitude);
    Stream out = noise;
    for (auto &channel : out)
        for (auto &sample : channel)
            sample += offset;
    return out;
}

Stream poisonStream(int channels, int samples, Poison kind)
{
    const float value = kind == Poison::NaN
                            ? std::numeric_limits<float>::quiet_NaN()
                            : kind == Poison::Infinity
                                  ? std::numeric_limits<float>::infinity()
                                  : std::numeric_limits<float>::denorm_min();
    return Stream(static_cast<std::size_t>(channels),
                  std::vector<float>(static_cast<std::size_t>(samples), value));
}

Stream speechLikeStream(int channels, int samples, unsigned seed, float amplitude)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples)));

    for (int channel = 0; channel < channels; ++channel)
    {
        const double fundamental = 90.0 + 90.0 * static_cast<double>(unit(rng));
        const double drift = 0.25 + 0.75 * static_cast<double>(unit(rng));
        const double syllableHz = 2.5 + 4.0 * static_cast<double>(unit(rng));
        const double phase = static_cast<double>(phaseFromSeed(rng));
        const double first = 1.0;
        const double second = 0.3 + 0.6 * static_cast<double>(unit(rng));
        const double third = 0.15 + 0.45 * static_cast<double>(unit(rng));
        const double fourth = 0.05 + 0.25 * static_cast<double>(unit(rng));
        const double weightSum = first + second + third + fourth;

        for (int i = 0; i < samples; ++i)
        {
            const double seconds = static_cast<double>(i) / kSampleRate;
            const double f0 = fundamental * (1.0 + 0.08 * drift * std::sin(kTwoPi * 0.25 * seconds));
            const double partials = first * std::sin(kTwoPi * f0 * seconds + phase)
                                    + second * std::sin(2.0 * kTwoPi * f0 * seconds + 2.0 * phase)
                                    + third * std::sin(3.0 * kTwoPi * f0 * seconds + 3.0 * phase)
                                    + fourth * std::sin(4.0 * kTwoPi * f0 * seconds + 4.0 * phase);
            const double shaped = 0.5 * (1.0 + std::sin(kTwoPi * syllableHz * seconds + phase));
            const double envelope = 0.35 + 0.65 * shaped * shaped;
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] = static_cast<float>(
                amplitude * envelope * partials / weightSum);
        }
    }
    return data;
}

/** Block sequences summing to exactly `total`, so two renders of one stream line up sample for
    sample instead of by luck — the old harness rendered whatever its block list happened to sum
    to, which silently capped how large an L the delta test could address.

    Zero-length callbacks stay in: they are what exercise the drain accounting. Pattern 0 is the
    M1 list plus a seeded random run, pattern 1 is {1, 4, 4096}. */
std::vector<int> blocksSummingTo(int total, int pattern)
{
    std::vector<int> blocks;
    int remaining = total;
    auto push = [&blocks, &remaining](int wanted) {
        if (remaining <= 0)
            return;
        const int taken = std::min(wanted, remaining);
        blocks.push_back(taken);
        remaining -= taken;
    };

    if (pattern == 0)
    {
        for (const int size : {1, 7, 32, 64, 128, 512, 1000, 2048, 4096})
        {
            blocks.push_back(0);
            push(size);
        }
        std::mt19937 rng(20261022U);
        std::uniform_int_distribution<int> random(kMaxBlock / 4, kMaxBlock);
        while (remaining > 0)
        {
            blocks.push_back(0);
            push(random(rng));
        }
    }
    else
    {
        while (remaining > 0)
            for (const int size : {1, 4, 4096})
            {
                blocks.push_back(0);
                push(size);
            }
    }

    blocks.push_back(0);
    blocks.push_back(0);
    return blocks;
}

/** A uniform callback list: `total` in `size`-sample callbacks, remainder last, no zero-length entries.

    §5.19's local `blocksOf` is `blocksSummingTo(total, 0)` under another name, so the file had no uniform
    plan until §5.20's third block plan asked for one: a steady 512-sample callback is the case the two
    ragged lists cannot separate from the DSP's own hop, and it is the shape a host actually runs. */
std::vector<int> constantBlocks(int total, int size)
{
    std::vector<int> blocks;
    for (int remaining = total; remaining > 0; remaining -= size)
        blocks.push_back(std::min(size, remaining));
    return blocks;
}

int totalOf(const std::vector<int> &blocks)
{
    int total = 0;
    for (int block : blocks)
        total += block;
    return total;
}

StreamDelta compareChannel(const Stream &got, std::size_t gotChannel, const Stream &want,
                           std::size_t wantChannel)
{
    StreamDelta delta;
    if (gotChannel >= got.size() || wantChannel >= want.size())
    {
        delta.shapeMismatch = true;
        delta.bitIdentical = false;
        return delta;
    }
    const auto &from = got[gotChannel];
    const auto &to = want[wantChannel];
    if (from.size() != to.size())
        delta.shapeMismatch = true;
    for (std::size_t i = 0; i < std::min(from.size(), to.size()); ++i)
    {
        const double difference = std::abs(static_cast<double>(from[i]) - static_cast<double>(to[i]));
        if (difference > delta.worst)
        {
            delta.worst = difference;
            delta.channel = gotChannel;
            delta.sample = i;
        }
        if (from[i] != to[i])
            delta.bitIdentical = false;
    }
    return delta;
}

StreamDelta compareStreams(const Stream &got, const Stream &want)
{
    StreamDelta delta;
    if (got.size() != want.size())
    {
        delta.shapeMismatch = true;
        delta.bitIdentical = false;
        return delta;
    }
    for (std::size_t channel = 0; channel < got.size(); ++channel)
    {
        const auto one = compareChannel(got, channel, want, channel);
        if (one.worst > delta.worst)
        {
            delta.worst = one.worst;
            delta.channel = channel;
            delta.sample = one.sample;
        }
        delta.bitIdentical = delta.bitIdentical && one.bitIdentical;
    }
    return delta;
}

double channelPeak(const Stream &stream, std::size_t channel)
{
    if (channel >= stream.size())
        return 0.0;
    double peak = 0.0;
    for (const float sample : stream[channel])
        peak = std::max(peak, static_cast<double>(std::abs(sample)));
    return peak;
}

bool channelIsExactSilence(const Stream &stream, std::size_t channel)
{
    if (channel >= stream.size())
        return false;
    for (const float sample : stream[channel])
        if (sample != 0.0f)
            return false;
    return true;
}

Stream delayedBy(const Stream &in, int latency)
{
    Stream want(in.size());
    for (std::size_t channel = 0; channel < in.size(); ++channel)
    {
        want[channel].assign(in[channel].size(), 0.0f);
        for (std::size_t i = static_cast<std::size_t>(latency); i < in[channel].size(); ++i)
            want[channel][i] = in[channel][i - static_cast<std::size_t>(latency)];
    }
    return want;
}

Stream scaledCopy(const Stream &in, float scale)
{
    Stream out = in;
    for (auto &channel : out)
        for (auto &sample : channel)
            sample = sample * scale;
    return out;
}

Stream meanWithOneReciprocal(const Stream &side, float scale)
{
    Stream mean(1, std::vector<float>(side.empty() ? 0 : side[0].size(), 0.0f));
    for (std::size_t i = 0; i < mean[0].size(); ++i)
    {
        float sum = 0.0f;
        for (const auto &channel : side)
            sum += channel[i];
        mean[0][i] = sum * scale;
    }
    return mean;
}

/** The DC component of a render: the largest |mean| over the last `window` samples of any channel,
    which is the spectral DC bin the wet path would carry. Reported over a settled tail rather than
    the whole render, because the head of every pass here is silence. */
double dcComponentOf(const Stream &stream, std::size_t window)
{
    double worst = 0.0;
    for (const auto &channel : stream)
    {
        if (channel.empty())
            continue;
        const std::size_t from = window < channel.size() ? channel.size() - window : 0;
        double sum = 0.0;
        for (std::size_t i = from; i < channel.size(); ++i)
            sum += static_cast<double>(channel[i]);
        worst = std::max(worst, std::abs(sum / static_cast<double>(channel.size() - from)));
    }
    return worst;
}

/** The level a settled stream is actually held at: RMS over the last `window` samples of each
    channel, in the same units as the samples. The head of every pass here is silence and the level
    stage spends its first 200 ms travelling, which is why this is a tail window and not the render. */
std::vector<double> settledRms(const Stream &stream, std::size_t window)
{
    std::vector<double> rms;
    rms.reserve(stream.size());
    for (const auto &channel : stream)
    {
        const std::size_t from = window < channel.size() ? channel.size() - window : 0;
        double sum = 0.0;
        for (std::size_t i = from; i < channel.size(); ++i)
            sum += static_cast<double>(channel[i]) * static_cast<double>(channel[i]);
        rms.push_back(channel.size() > from
                          ? std::sqrt(sum / static_cast<double>(channel.size() - from))
                          : 0.0);
    }
    return rms;
}

/** How much of a render the clipper is actually shaping — whether it is engaged at default settings
    is either a measured fact or a guess. */
double fractionAboveKnee(const Stream &stream)
{
    std::size_t above = 0;
    std::size_t total = 0;
    for (const auto &channel : stream)
        for (const float sample : channel)
        {
            above += std::abs(sample) > dsp::SoftClipper::knee ? 1U : 0U;
            ++total;
        }
    return total != 0 ? static_cast<double>(above) / static_cast<double>(total) : 0.0;
}

/** The click prototype: max |x[n] - 2x[n-1] + x[n-2]| over every channel. */
double secondDifferencePeak(const Stream &stream)
{
    double worst = 0.0;
    for (const auto &channel : stream)
        for (std::size_t i = 2; i < channel.size(); ++i)
        {
            const double d2 = std::abs(static_cast<double>(channel[i])
                                       - 2.0 * static_cast<double>(channel[i - 1])
                                       + static_cast<double>(channel[i - 2]));
            worst = std::max(worst, d2);
        }
    return worst;
}

/** Where a render first clears `threshold` — an index that must not move when a stage is installed,
    since a stage that delays would move it by exactly its delay. */
std::size_t arrivalIndex(const Stream &stream, double threshold)
{
    const std::size_t length = stream.empty() ? 0 : stream[0].size();
    for (std::size_t i = 0; i < length; ++i)
        for (const auto &channel : stream)
            if (std::abs(static_cast<double>(channel[i])) > threshold)
                return i;
    return std::numeric_limits<std::size_t>::max();
}

double peakOf(const Stream &stream)
{
    double peak = 0.0;
    for (const auto &channel : stream)
        for (const float sample : channel)
            peak = std::max(peak, static_cast<double>(std::abs(sample)));
    return peak;
}

/** Level of a ratio, for the legs that have to quote a reshaping in the unit the decision log uses. */
double decibels(double ratio)
{
    return 20.0 * std::log10(ratio);
}

double hannAt(int i, int length) noexcept
{
    return 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i)
                                 / static_cast<double>(length)));
}

}
