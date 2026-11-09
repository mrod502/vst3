#pragma once

#include <DcBlocker.hpp>
#include <GrainEngine.hpp>
#include <HopScheduler.hpp>
#include <LevelNormalizer.hpp>
#include <MixEngine.hpp>
#include <SoftClipper.hpp>

#include <memory>

namespace entangle
{
/** Everything the shipped chain needs to be sized, decided off the audio thread. */
struct ChainConfig
{
    double sampleRate = 0.0;
    int hop = 0;
    int grainLength = 0;
    int channels = 0;          ///< chA, the main input width.
    int sidechainChannels = 0; ///< 0 means the sidechain bus is absent (D-002)
    int outputChannels = 0;    ///< the main output width; equal to chA for every layout the gate takes
    int maxHostBlock = 0;
};

/** Which of the chains the builder can compose.

    Shipped is what the plugin runs and what every leg that speaks about the product measures. RawWet
    leaves the safety stages out, which is the only way to assert engine physics — the delta anchor's
    shape, gain and arrival — against a stream that has not been reshaped on purpose. M3 item 2
    criterion C1: a leg names the variant it runs on, or the anchor stops pinning the engine's gain
    the day a filter lands in front of it.

    ClipperOnly exists for criterion B6: the DC blocker reshapes the anchor's envelope by design, so
    an arrival index compared across Shipped and RawWet measures the filter. With only the clipper in
    front of the engine, any movement of the arrival index can only be a delay in that stage.

    NoNormalizer is the shipped chain minus the level stage. M3 item 4 criterion C2 compares the delta
    anchor's onset against a render with no correction, and C3 prints the L-spread table with the stage
    beside the same table without it. Both need a named variant for the same reason as RawWet: without
    one, the baseline is whatever the builder happened to install that day. */
enum class ChainVariant
{
    Shipped,
    RawWet,
    ClipperOnly,
    NoNormalizer
};

/** The composition that ships: the M3 mix law over the mode 1 grain engine, with the DC blocker and
    the level normalizer on the convolved stream and the soft clipper on the output (decision C's
    order, D-016 for the stage between them).

    One builder, two callers — EntangleAudioProcessor and OfflineRender's §5.1 reference render.
    Where the chain is assembled in two places, the harness silently stops comparing what ships and
    the shell-identity and wet-path legs become self-fulfilling. */
inline std::unique_ptr<engine::MixEngine> makeChain(
    const grain::TransformFactory& factory, ChainVariant variant)
{
    auto wet = std::make_unique<grain::GrainConvolutionEngine>(factory);
    if (variant == ChainVariant::RawWet)
        return std::make_unique<engine::MixEngine>(std::move(wet));
    if (variant == ChainVariant::ClipperOnly)
        return std::make_unique<engine::MixEngine>(std::move(wet), nullptr,
                                                   std::make_unique<dsp::SoftClipper>());

    auto chain = std::make_unique<engine::MixEngine>(
        std::move(wet), std::make_unique<dsp::DcBlocker>(),
        std::make_unique<dsp::SoftClipper>());
    if (variant != ChainVariant::NoNormalizer)
        chain->installLevelNormalizer(std::make_unique<dsp::LevelNormalizer>());
    return chain;
}

inline std::unique_ptr<engine::MixEngine> makeShippedChain(
    const grain::TransformFactory& factory)
{
    return makeChain(factory, ChainVariant::Shipped);
}

/** The ring configuration for one prepare, R included.

    R is the M1 placeholder policy of D-004 — one hop — and this is the only place it is written, so
    the shell and the harness can never be measuring two different latency models.

    This is where the M4 map is turned into ring sizes (D-017 as amended by D-018): the map it resolves
    is what the scheduler turns into pointers, and the engine is sized by the processed count rather than
    by either bus width. It consumes the map and does not decide the layout — `engine::gateLayout`
    (LayoutGate.hpp) owns "may this layout exist at all" (D-021), and `configureRings` asks it before
    trusting what comes back here. A `map.channels` of 0 means the rule serves no channel set for these
    widths — prepare nothing and let the host hear silence; the status a host should be given is the
    verdict `gateLayout` returns for the same triple. */
inline engine::SchedulerConfig makeSchedulerConfig(const ChainConfig& chain)
{
    const engine::ResolvedChannelMap resolved =
        engine::ChannelMap::resolve(chain.channels, chain.sidechainChannels, chain.outputChannels);

    engine::EngineConfig engineConfig;
    engineConfig.sampleRate = chain.sampleRate;
    engineConfig.hop = chain.hop;
    engineConfig.maxChannels = resolved.map.channels;
    engineConfig.latencySamples = chain.hop;
    engineConfig.grainLength = chain.grainLength;

    engine::SchedulerConfig schedulerConfig;
    schedulerConfig.engine = engineConfig;
    schedulerConfig.channels = chain.channels;
    schedulerConfig.sidechainChannels = chain.sidechainChannels;
    schedulerConfig.maxHostBlock = chain.maxHostBlock;
    schedulerConfig.map = resolved.map;
    return schedulerConfig;
}

/** The dial position that actually runs (D-011).

    With no sidechain bus this is pure A whatever the dial says. Under the law p = 0 is pure
    convolution and convolution with a silent B is silence (D-002), so a host that cannot offer a
    second bus would otherwise get silence from its own default setting — the property D-009 was
    created to keep. The dial keeps its value, and the next prepare that sees a sidechain restores
    it; that means a preset saved at p = 0 with a sidechain renders A in a host without one. The
    editor reports this figure rather than the dial's, on the same grounds as the grain-length
    readout: a control that lies about what is running is worse than no control. */
inline float effectiveBlend(float dialPosition, bool sidechainActive) noexcept
{
    return sidechainActive ? dialPosition : -1.0f;
}
}
