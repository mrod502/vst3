#include "GrainEngine.hpp"

#include <cassert>
#include <stdexcept>

namespace grain
{
namespace
{
int nextPowerOfTwoAtLeast(int value) noexcept
{
    int size = 1;
    while (size < value)
        size <<= 1;
    return size;
}
}

GrainConvolutionEngine::GrainConvolutionEngine(const TransformFactory& factory) noexcept
    : transformFactory(&factory)
{
}

void GrainConvolutionEngine::prepare(const engine::EngineConfig& config)
{
    const int length = config.grainLength;
    const int hop = config.hop;
    if (length <= 0 || hop <= 0 || length % hop != 0)
        throw std::invalid_argument("grain length must be a positive multiple of the hop");

    /* R is a declaration the host acts on, so an out-of-range value would have the host
       compensate for a delay this engine does not have. The engine can honour any R in [0, L]
       without changing its arithmetic; outside it there is nothing to report honestly. */
    if (config.latencySamples < 0 || config.latencySamples > length)
        throw std::invalid_argument("declared latency must lie within [0, grain length]");

    if (config.maxChannels <= 0)
        throw std::invalid_argument("channel capacity must be positive");

    grainLengthValue = length;
    hopValue = hop;
    sizeFValue = nextPowerOfTwoAtLeast(2 * length - 1);
    reportedLatency = config.latencySamples;
    wetGain = static_cast<float>(1.0 / idealColaSum(length, hop));
    window = GrainWindow(length, GrainWindow::Kind::PeriodicHann);
    transform = transformFactory->create(sizeFValue);
    assert(transform != nullptr && transform->size() == sizeFValue);

    channels.resize(static_cast<std::size_t>(config.maxChannels));
    for (Channel& channel : channels)
    {
        channel.a.prepare(length, hop, sizeFValue);
        channel.b.prepare(length, hop, sizeFValue);
        channel.ola.prepare(length, hop, sizeFValue);
        channel.a.reset();
        channel.b.reset();
        channel.ola.reset();
    }

    grainA.assign(static_cast<std::size_t>(sizeFValue), 0.0f);
    grainB.assign(static_cast<std::size_t>(sizeFValue), 0.0f);
    spectrumA.assign(static_cast<std::size_t>(sizeFValue) + 2, 0.0f);
    spectrumB.assign(static_cast<std::size_t>(sizeFValue) + 2, 0.0f);
    spectrumProduct.assign(static_cast<std::size_t>(sizeFValue) + 2, 0.0f);
    impulse.assign(static_cast<std::size_t>(sizeFValue), 0.0f);
}

void GrainConvolutionEngine::reset()
{
    for (Channel& channel : channels)
    {
        channel.a.reset();
        channel.b.reset();
        channel.ola.reset();
    }
}

void GrainConvolutionEngine::processHop(const float* const* a, const float* const* b,
                                        float* const* out, int numChannels)
{
    assert(numChannels > 0 && numChannels <= static_cast<int>(channels.size()));
    assert(grainLengthValue > 0);

    for (int c = 0; c < numChannels; ++c)
    {
        Channel& channel = channels[static_cast<std::size_t>(c)];

        channel.a.push(a[c], hopValue);
        channel.b.push(b[c], hopValue);
        channel.a.extractWindowed(window.data(), grainA.data());
        channel.b.extractWindowed(window.data(), grainB.data());

        transform->forward(grainA.data(), spectrumA.data());
        transform->forward(grainB.data(), spectrumB.data());
        fft::multiplyHalfSpectrum(spectrumA.data(), spectrumB.data(), spectrumProduct.data(),
                                  sizeFValue);
        transform->inverse(spectrumProduct.data(), impulse.data());

        channel.ola.emitAndAdd(impulse.data(), wetGain, out[c]);
    }
}
}
