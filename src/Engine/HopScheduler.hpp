#pragma once

#include "ChannelMap.hpp"
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
    /** @c chA — the A *source* width, which is the number of A pointers a callback hands over and
        therefore what @c aIn holds. Not the processed count: see @c map. */
    int channels = 0;
    /** @c chB — the B source width, the number of pointers @c bIn holds; 0 means the sidechain bus
        is absent (D-002). */
    int sidechainChannels = 0;
    int maxHostBlock = 0; ///< Largest numSamples any host callback may deliver.

    /** The resolved map this scheduler turns into pointers: `resolve(channels, sidechainChannels,
        outputs)`, so `map.channels` is the processed count the Engine is driven with.

        Mandatory — a value-initialised map has `channels == 0` and `prepare` rejects it. The rule
        itself lives in `ChannelMap::resolve` and nowhere else (M4 INDEX §3 rule 2); this class only
        dereferences it, and `prepare` checks its indices against the widths passed above. */
    ChannelMap map{};
};

/** Turns arbitrary host callbacks into whole hops for one Engine.

    Invariant: every callback delivers exactly as many samples as it consumed, including a
    zero-length callback, which returns without touching state. A hop fires only when every
    input ring holds a full H, so a partial hop never reaches the Engine (T1) and the Engine
    never sees hostBlock.

    A missing sidechain is silence, filled at ingest and at hop read, so it cannot stall the
    hop clock (T3). The @c map decides what each processed channel reads on each side, and it is
    resolved before this class is constructed:

    * a side narrower than the processed count **broadcasts** — every slot whose index is @c 0 gets
      the same source pointer, so the Engine's per-channel loop needs no branch (M4 §5);
    * a consolidated side (`kConsolidatedSource`, D-018) is the arithmetic mean of its @c chB
      channels, summed once per hop into one pre-allocated mono scratch, and the Engine gets that
      single pointer;
    * `kSilentSource` reads zeros.

    Rings and hop scratch are per *source* channel (`chA` A rings, `chB` B rings, `chB × hop` floats
    of B scratch); the pointer arrays, the output rings and the output scratch are per *processed*
    channel. That is the difference the fold creates: a mono main with an 8-wide sidechain costs the
    Engine one channel set and the scheduler eight input rings.

    prepare() allocates; process() does not.
*/
class HopScheduler
{
public:
    explicit HopScheduler(Engine& engineToUse) noexcept;

    void prepare(const SchedulerConfig&);

    /** Drops all buffered audio and re-applies the latency pre-fill. Audio-thread safe. */
    void reset() noexcept;

    /** @param aIn @c config.channels (@c chA) pointers of @p numSamples samples.
        @param bIn @c config.sidechainChannels (@c chB) pointers, or nullptr when the sidechain is
        inactive.
        @param out @p numChannels pointers; exactly @p numSamples written per channel.
        @param numChannels the processed count, `config.map.channels` — not the width of either
        input side.
    */
    void process(const float* const* aIn, const float* const* bIn,
                 float* const* out, int numChannels, int numSamples) noexcept;

    std::uint64_t hopCount() const noexcept { return hops; }

private:
    bool holdsFullHop() const noexcept;
    void foldSidechain() noexcept;
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

    /** The consolidated side's mono hop (D-018): `hop` floats, written between the B ring reads and
        `processHop`, and never anywhere else. Separate from `hopBufferB` because the Engine reads
        the fold and the sources at the same time — folding in place would overwrite the inputs the
        mean is still being summed from. */
    std::vector<float> foldBuffer;
    bool consolidates = false;
    float foldScale = 0.0f; ///< `1 / chB`, applied once after the sum so the mean is one multiply.

    std::vector<float> silence; ///< zeros: missing sidechain, and the output pre-fill

    std::uint64_t hops = 0;
};
}
