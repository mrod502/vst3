#pragma once

#include <type_traits>

namespace engine
{
/** The configuration a prepare has already realised, so the next one can be skipped (D-022 B).

    The shell's `configureRings` is reached from `prepareToPlay`, from `processorLayoutsChanged` and from the
    hop and grain-length dials through `AsyncUpdater`. Before this key every arrival rebuilt every ring,
    re-prepared the engine — which rebuilds the transform and zeroes the grain history — and re-applied the
    latency pre-fill, so a host that re-applies an unchanged layout on session load dropped the tail of the
    previous one. Equality of this key is what makes "nothing changed" decidable instead of assumed.

    The seven fields are the *inputs* to `configureRings`, not its outputs, and the last two are the dial
    values **after** `EntangleAudioProcessor::clampGrainLength`: a dial move the clamp swallows changes nothing
    the engine can see, so it must not re-prepare. `sidechainChannels` carries the sidechain-enabled bit,
    because `configureRings` writes 0 when the bus is disabled and D-002 keeps that distinct from mono.
    `maxHostBlock` is not a field — it is `maxHostBlockFor(hostBlock)`, and holding both would be a second
    place for the two to disagree.

    Plain data and equality, nothing else. The decision to prepare or return stays in `configureRings`, beside
    the gate call that decides whether the triple may exist, so the layout rule and the reconfigure rule keep
    one home each (M4 `INDEX.md` §3 rule 2). A value-initialised key describes a 0 Hz, 0-block, 0-channel
    configuration no prepare can realise; the shell keeps the "never prepared" state in a `std::optional`
    rather than depending on that, so a first prepare can never be skipped.
*/
struct ReconfigureKey
{
    double sampleRate = 0.0;
    int hostBlock = 0;            ///< the host's callback size as prepared, before the floor of D-022 A
    int channels = 0;             ///< chA — the main input width
    int sidechainChannels = 0;    ///< chB — 0 means the sidechain bus is absent (D-002)
    int outputChannels = 0;       ///< chOut — equal to chA on every layout the gate accepts (D-018)
    int hop = 0;                  ///< H, straight off the dial
    int grainLength = 0;          ///< L, after the clamp — the length that actually runs

    friend constexpr bool operator==(const ReconfigureKey&, const ReconfigureKey&) noexcept = default;
};

static_assert(sizeof(ReconfigureKey) <= sizeof(double) + 6 * sizeof(int),
              "ReconfigureKey outgrew the bound documented beside it");
static_assert(std::is_trivially_copyable_v<ReconfigureKey>,
              "The key is copied by value on the message thread and compared against the running one");
static_assert(ReconfigureKey{} != ReconfigureKey{48000.0, 4096, 2, 2, 2, 128, 8192},
              "A value-initialised key must never match a configuration a host can be given — it is the "
              "fallback a default-constructed sentinel would compare against");
}
