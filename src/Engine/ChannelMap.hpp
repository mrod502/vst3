#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace engine
{
/** Ceiling on the processed channel count `N`, and the capacity of a `ChannelMap`'s index arrays.

    Set at 8 rather than 16 (D-017). Every per-channel figure below the seam is linear in `N` —
    `MixEngine`'s scratch is `hop × N` floats and the level stage owns three window-length detectors per
    channel — so a ceiling of 16 doubles the footprint of every configuration, including the ones no host
    ever asks for. `resolve`'s `capacity` argument lets a caller ask for less and a test drive the boundary
    without recompiling.

    A resolved map is at most `sizeof(int) + 2 · kMaxChannels · sizeof(uint16_t)` = 36 bytes at this
    ceiling, which is the bound the `static_assert` at the bottom of this file pins. */
inline constexpr int kMaxChannels = 8;

/** The source index for "this side has no channel here" — every B slot when the sidechain bus is absent
    (D-002).

    Never 0, and deliberately outside every real index range. A mono sidechain broadcasts its channel 0 to
    all outputs; a missing sidechain contributes silence to all of them. Those are different states, and an
    unhandled sentinel must fail as an out-of-range index rather than alias the dry bus into the wet path.
*/
inline constexpr std::uint16_t kSilentSource = 0xFFFF;

/** The source marker for "this side is the arithmetic mean of all its channels" — every B slot of a
    consolidated configuration, `chA == 1 && chB > 1` (D-018).

    A fold is not a channel selection, so it cannot be spelled as an index. Item 02's resolver reads the
    `chB` sidechain channels and writes `(1 / chB) · Σ_c B[c]` into one pre-allocated mono hop above the
    seam, so nothing in `Engine`, `Dsp`, `Fft` or `Grain` ever reads across channels.

    It sits in the same field as the real indices rather than beside them in a flag, because the field is a
    source selector and "the mean of this side" is one of its values on the same footing as "no channel
    here" — two sources of truth would let a caller read the index and the flag in either order. A caller
    that treats the marker as an index reads out of range rather than silently convolving against channel 0,
    which is why the value is outside every real index range and distinct from `kSilentSource`. */
inline constexpr std::uint16_t kConsolidatedSource = 0xFFFE;

/** Why a triple was resolved the way it was — or rejected. The reasons exist so a layout gate can name
    the rule it tripped on rather than hand a host a bare `false`. */
enum class ChannelMapStatus
{
    ok,             ///< the map holds `channelsOut` resolved index pairs
    empty,          ///< nothing to render: `chA <= 0`, `channelsOut <= 0`, or a negative count
    asymmetric,     ///< `chA > 1`, `chB > 1` and `chA != chB` — no rule is defined for it
    countMismatch,  ///< `channelsOut` above what this rule can produce for these counts — `max(chA, chB)`,
                    ///  or 1 for a consolidated mono main (D-018)
    exceedsCapacity ///< `max(chA, chB) > capacity`
};

struct ChannelMap;
struct ResolvedChannelMap;

/** Which source — or which fold of the whole side — each output channel reads, on each side: the one
    place the M4 channel rule lives (D-017, amended for the mono-main family by D-018).
    `isBusesLayoutSupported` will gate on it and the seam will turn it into pointers; neither is allowed to
    re-derive it.

    Resolved at prepare time and copied by value onto the audio path. It holds no pointer, reads no sample
    and allocates nothing, so it is trivially copyable by contract and layout-checked below. */
struct ChannelMap
{
    /** Resolves the main-input count `chA`, the sidechain count `chB` (0 = the bus is absent) and the
        main-output count `channelsOut` into `channelsOut` index pairs, `c` running over
        `[0, channelsOut)`. A `capacity` above `kMaxChannels` is clamped to it — the arrays are that size
        whatever a caller asks for.

        | chA    | chB            | channelsOut         | status            | `a[c]` | `b[c]` |
        |--------|----------------|---------------------|-------------------|--------|--------|
        | `0`    | any            | any                 | `empty`           | —      | —      |
        | `>= 1` | any            | `<= 0`              | `empty`           | —      | —      |
        | `> 1`  | `> 1`, unequal | any                 | `asymmetric`      | —      | —      |
        | any    | any            | any, `N > capacity` | `exceedsCapacity` | —      | —      |
        | `>= 1` | `0`            | `1..chA`            | `ok`              | `c`    | `kSilentSource` |
        | `1`    | `>= 2`         | `1`                 | `ok`              | `0`    | `kConsolidatedSource` |
        | `1`    | `>= 2`         | `>= 2`              | `countMismatch`   | —      | —      |
        | `>= 2` | `1`            | `1..chA`            | `ok`              | `c`    | `0`      |
        | `>= 2` | `== chA`       | `1..chA`            | `ok`              | `c`    | `c`      |
        | `>= 1` | any            | `> max(chA, chB)`   | `countMismatch`   | —      | —      |

        Read as four rules: a side broadcasts its channel 0 exactly when it has one channel; the two sides
        must agree whenever both are multi-channel; a mono main with a wider sidechain consolidates that
        side to one channel; and the output count is at most `max(chA, chB)`, or exactly 1 once the third
        rule applies.

        Fewer outputs than sources selects the leading channels — index `c` of each side, or that side's
        channel 0 when it is mono — and drops the rest, which is selection rather than the downmix M4 §2
        puts out of scope. Consolidation is the other way a side stops contributing `chB` sources, and it is
        the one cross-channel read in the design (D-018): the fold happens above the seam, in the shell's
        resolver, and the map only states that it is due.

        The rule keys on the **main** bus, so it is not symmetric in (`chA`, `chB`): a mono main with a
        stereo sidechain is one consolidated channel and a stereo main with a mono sidechain is two
        broadcast channels. Nothing here commutes, and F-013 records why the property M4 §1 once asked for
        has no domain left to be tested over.

        `asymmetric` is checked before `exceedsCapacity` and both before the count, so a triple that
        breaks several rules reports the most specific one and never fills what it is about to reject. The
        ceiling binds the *bus* widths, not the processed count: a 9-channel sidechain feeding a mono main
        is `exceedsCapacity` even though consolidation processes one channel.
    */
    static constexpr ResolvedChannelMap resolve(int chA, int chB, int channelsOut,
                                                int capacity = kMaxChannels) noexcept;

    int channels = 0;
    std::array<std::uint16_t, kMaxChannels> a{};
    std::array<std::uint16_t, kMaxChannels> b{};
};

/** The map beside the reason it was resolved that way.

    On every non-`ok` path `map` is value-initialised — `channels == 0` and every index 0 — so a rejected
    call provably leaves no half-written indices behind. Read it only when the status is `ok`. */
struct ResolvedChannelMap
{
    ChannelMapStatus status = ChannelMapStatus::empty;
    ChannelMap map{};
};

constexpr ResolvedChannelMap ChannelMap::resolve(int chA, int chB, int channelsOut,
                                                 int capacity) noexcept
{
    if (chA <= 0 || chB < 0 || channelsOut <= 0)
        return {};

    const int ceiling = capacity < kMaxChannels ? capacity : kMaxChannels;
    const int sources = chA > chB ? chA : chB;
    const bool consolidates = chA == 1 && chB > 1;

    if (chA > 1 && chB > 1 && chA != chB)
        return { ChannelMapStatus::asymmetric, {} };
    if (sources > ceiling)
        return { ChannelMapStatus::exceedsCapacity, {} };
    if (channelsOut > (consolidates ? 1 : sources))
        return { ChannelMapStatus::countMismatch, {} };

    ResolvedChannelMap resolved{ ChannelMapStatus::ok, ChannelMap{} };
    resolved.map.channels = channelsOut;

    /* The guards above leave channelsOut in [1, ceiling] and ceiling at most kMaxChannels. Restating the
       bound here is what keeps the fill provably in range for the optimiser — written without it, GCC
       vectorises the loop as a memset whose trip count it takes from the signed argument alone and warns
       -Wstringop-overflow on both arrays. */
    const auto slots = static_cast<std::size_t>(channelsOut < kMaxChannels ? channelsOut
                                                                           : kMaxChannels);
    for (std::size_t slot = 0; slot < slots; ++slot)
    {
        const auto index = static_cast<std::uint16_t>(slot);
        resolved.map.a[slot] = chA == 1 ? 0 : index;
        resolved.map.b[slot] = consolidates ? kConsolidatedSource
                                            : (chB == 0 ? kSilentSource : (chB == 1 ? 0 : index));
    }
    return resolved;
}

static_assert(sizeof(ChannelMap) <= sizeof(int) + 2 * kMaxChannels * sizeof(std::uint16_t),
              "ChannelMap outgrew the bound documented beside kMaxChannels");
static_assert(std::is_trivially_copyable_v<ChannelMap> && std::is_trivially_copyable_v<ResolvedChannelMap>,
              "A resolved map must stay copyable by value onto the audio path");
}
