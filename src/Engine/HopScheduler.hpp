#pragma once

#include "Engine.hpp"

#include <Container.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine
{
/** Ring sizing for the scheduler. Decided off the audio thread; nothing here is a literal
    the DSP has to trust — the capacities are derived from these numbers. */
struct SchedulerConfig
{
    EngineConfig engine;
    int channels = 0;          ///< A and output channel count.
    int sidechainChannels = 0; ///< B channels; 0 means the sidechain bus is absent (D-002).
    int maxHostBlock = 0;      ///< Largest numSamples any host callback may deliver.
};

/** Turns arbitrary host callbacks into whole hops for one Engine.

    Invariant: every callback delivers exactly as many samples as it consumed, including a
    zero-length callback, which returns without touching state. A hop fires only when every
    input ring holds a full H, so a partial hop never reaches the Engine (T1) and the Engine
    never sees hostBlock.

    A missing sidechain is silence, filled at ingest and at hop read, so it cannot stall the
    hop clock (T3). Channels beyond sidechainChannels read silence; upmix policy is M4's.

    prepare() allocates; process() does not.
*/
class HopScheduler
{
public:
    explicit HopScheduler(Engine& engineToUse) noexcept;

    void prepare(const SchedulerConfig&);

    /** Drops all buffered audio and re-applies the latency pre-fill. Audio-thread safe. */
    void reset() noexcept;

    /** @param aIn @p numChannels pointers of @p numSamples samples.
        @param bIn @p sidechainChannels pointers, or nullptr when the sidechain is inactive.
        @param out @p numChannels pointers; exactly @p numSamples written per channel.
    */
    void process(const float* const* aIn, const float* const* bIn,
                 float* const* out, int numChannels, int numSamples) noexcept;

    std::uint64_t hopCount() const noexcept { return hops; }

private:
    bool holdsFullHop() const noexcept;
    void prefillOutput() noexcept;

    Engine* engine; ///< non-owning borrow; the shell owns the Engine's lifetime

    SchedulerConfig config;
    bool prepared = false;

    std::vector<container::CircularBuffer<float>> inputA;
    std::vector<container::CircularBuffer<float>> inputB;
    std::vector<container::CircularBuffer<float>> output;

    /** One hop of contiguous scratch per channel: the rings wrap, the Engine contract does
        not. Sized in prepare() only, which is what keeps the pointer arrays stable. */
    std::vector<float> hopBufferA;
    std::vector<float> hopBufferB;
    std::vector<float> hopBufferOut;
    std::vector<const float*> hopA;
    std::vector<const float*> hopB;
    std::vector<float*> hopOut;

    std::vector<float> silence; ///< zeros: missing sidechain, and the output pre-fill

    std::uint64_t hops = 0;
};
}
