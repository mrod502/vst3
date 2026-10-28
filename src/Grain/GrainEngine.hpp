#pragma once

#include "GrainHistory.hpp"
#include "OlaAccumulator.hpp"
#include "Window.hpp"

#include <Engine.hpp>
#include <Fft.hpp>
#include <Spectral.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace grain
{
/** Builds the transform of a size the engine only knows at prepare time.

    The engine cannot construct its own backend: the seam lives in Fft and the JUCE-backed
    implementation is a separate library, so the caller supplies one. Owning the choice here is
    also what lets the tests run the identical engine on the Dft oracle and on the real backend. */
class TransformFactory
{
public:
    virtual ~TransformFactory() = default;

    [[nodiscard]] virtual std::unique_ptr<fft::IFft> create(int sizeF) const = 0;
};

/** Mode 1: windowed grains of A and B, linearly cross-convolved by FFT, overlap-added.

    Per hop, per channel:

    push both signals; extract the windowed, zero-padded grain of each; forward both; multiply
    the half-spectra out-of-place; inverse; overlap-add and emit.

    Time origin: a grain starting at s is complete at real time s+L and its result sample j is
    emitted at output position s+L+j, so the physical wet delay is exactly L and the engine adds
    nothing beyond it. R, the declared latency, is therefore an independent choice in [0, L]:
    it changes what the host compensates for and what M3 delays the dry path by, never this
    arithmetic. F is derived from L as nextPow2(2L-1), the linear-convolution length; the
    accumulator's live span is 2L-1, which is what makes F a sufficient ring.

    The provisional wet gain is 1/C with C = L/(2H), the Hann COLA sum computed once here. That
    is the whole gain table: the transform owns 1/F and every backend factor, so a second factor
    here would cost 20·log10(F) dB (F-004).

    prepare allocates and throws on an unusable config; processHop allocates nothing, takes no
    lock and is audio-thread safe. One instance is driven by one thread. */
class GrainConvolutionEngine final : public engine::Engine
{
public:
    /** @param factory borrows nothing; must outlive the engine. */
    explicit GrainConvolutionEngine(const TransformFactory& factory) noexcept;

    /** @throws std::invalid_argument when L is not a positive multiple of H, or R falls outside
        [0, L]. Both are prepare-time mistakes: reporting them here is what keeps processHop
        total (§8), and R outside that range would declare a latency the engine cannot honour. */
    void prepare(const engine::EngineConfig&) override;

    void reset() override;

    int latencySamples() const override { return reportedLatency; }

    int wetDelaySamples() const override { return grainLengthValue; }

    void processHop(const float* const* a, const float* const* b, float* const* out,
                    int numChannels) override;

    /** F, the transform size derived from L. */
    int transformSize() const noexcept { return sizeFValue; }

private:
    const TransformFactory* transformFactory; ///< non-owning borrow
    std::unique_ptr<fft::IFft> transform;

    int grainLengthValue = 0;
    int hopValue = 0;
    int sizeFValue = 0;
    int reportedLatency = 0;
    float wetGain = 1.0f;

    GrainWindow window{1, GrainWindow::Kind::PeriodicHann};

    struct Channel
    {
        GrainHistory a;
        GrainHistory b;
        OlaAccumulator ola;
    };
    std::vector<Channel> channels;

    /** One scratch set, reused across channels in sequence: nothing from one channel's
        pipeline survives into the next, so per-channel scratch would only multiply the
        footprint. Sized in prepare to F and F + 2. */
    std::vector<float> grainA;
    std::vector<float> grainB;
    std::vector<float> spectrumA;
    std::vector<float> spectrumB;
    std::vector<float> spectrumProduct;
    std::vector<float> impulse;
};
}
