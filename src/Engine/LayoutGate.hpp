#pragma once

#include "ChannelMap.hpp"

#include <type_traits>

namespace engine
{
/** The widest main input bus the shell will accept.

    A width the *shell* chooses, not a property of the channel rule: `resolve` takes an 8-wide main today.
    D-021 B defers quad, 5.1 and 7.1 behind three figures this item does not own — item 04's prepare-time
    allocation arithmetic, M7's per-channel cost curve (F-007 is per-callback) and M4 §6's question about how
    JUCE reports surround and ambisonic buses. Read the line below as that deferral, not as a rule. */
inline constexpr int kMaxMainBusChannels = 2;

/** The host-facing half of a layout answer: whether the triple may exist at all.

    `LayoutDecision::rule` carries the map's verdict beside this one, so a refusal can name the rule it
    tripped on rather than hand a host a bare `false` (D-017). */
enum class LayoutVerdict
{
    accepted,          ///< the layout may exist; `processed` is the count the Engine is driven with
    mainWidth,         ///< `chA` above `kMaxMainBusChannels` — D-021 B's deferral, which the map cannot see
    mainCountMismatch, ///< `chOut != chA` — D-018's `mainOut == mainIn`, which the map cannot see either
    channelRule        ///< `ChannelMap::resolve` rejected it, and `rule` says which rule it tripped on
};

/** The layout decision: the verdict, the map's reason verbatim, and the processed count.

    No pointer, no JUCE type, trivially copyable, and at most `3 * sizeof(int)` = 12 bytes — the bound the
    `static_assert` at the bottom of this file pins, the way `ChannelMap` pins its 36. A value-initialised
    decision refuses and names the reason (`channelRule` + `empty`), so there is no uninitialised state that
    reads as permission. */
struct LayoutDecision
{
    LayoutVerdict verdict = LayoutVerdict::channelRule;
    ChannelMapStatus rule = ChannelMapStatus::empty;
    int processed = 0; ///< `map.channels` when accepted, 0 on every refusal
};

/** May this layout exist at all — the one place that answers, for `isBusesLayoutSupported` and for the
    shell's prepare (D-021).

    Exactly two rules are decided here: the main bus width and `chOut == chA`. Everything else is
    `ChannelMap::resolve`'s, which is called on every path and whose status is kept in `rule` whatever the
    verdict, so no refusal loses the reason the map produced.

    `chB <= chA` is a *necessary condition* of every accepted layout and is not this function's rule. Read
    as a decision procedure that sentence admits 22 triples on the domain a host can present — 21
    `asymmetric` ones (`(3, 2, 3)` … `(8, 7, 8)`) and `(1, 9, 1)` past the ceiling (D-018's own wording,
    measured 2026-11-04). This gate inherits the map's answer for that reason.

    | chA | chB | chOut | verdict | `rule` | processed |
    |---|---|---|---|---|---|
    | 1 | 0 / 1 | 1 | accepted | ok | 1 |
    | 1 | 2…8 | 1 | accepted | ok | 1 | consolidated (D-018) |
    | 1 | 9 | 1 | refused | exceedsCapacity | 0 | the ceiling binds bus widths (D-017) |
    | 2 | 0 / 1 / 2 | 2 | accepted | ok | 2 |
    | 2 | 3…9 | 2 | refused | asymmetric | 0 |
    | 3…8 | any | = chA | refused | `resolve`'s answer, kept | 0 | `mainWidth` — D-021 A/B |
    | any | any | ≠ chA | refused | `resolve`'s answer, kept | 0 | `mainCountMismatch` — D-018 |
    | 0 | any | any | refused | empty | 0 | A is the mandatory side (D-017) |
    | any | any | 0 | refused | empty | 0 | a disabled main output |

    Rows are the verdict ladder of the body, evaluated in that order: a triple that breaks several rules
    reports the first one it meets, so a 4-wide main with an unequal output is `mainWidth` (the width
    decision has to be made before any count can be argued about) and a 0-channel main is `channelRule` +
    `empty` because the map already names "no main input" — re-labelling that would lose the reason.
*/
constexpr LayoutDecision gateLayout(int chA, int chB, int chOut) noexcept
{
    const auto resolved = ChannelMap::resolve(chA, chB, chOut);
    const auto rule = resolved.status;

    if (rule == ChannelMapStatus::empty)
        return { LayoutVerdict::channelRule, rule, 0 };
    if (chA > kMaxMainBusChannels)
        return { LayoutVerdict::mainWidth, rule, 0 };
    if (chOut != chA)
        return { LayoutVerdict::mainCountMismatch, rule, 0 };
    if (rule == ChannelMapStatus::ok)
        return { LayoutVerdict::accepted, rule, resolved.map.channels };

    return { LayoutVerdict::channelRule, rule, 0 };
}

static_assert(sizeof(LayoutDecision) <= 3 * sizeof(int),
              "LayoutDecision outgrew the bound documented beside it");
static_assert(std::is_trivially_copyable_v<LayoutDecision>,
              "The layout decision is copied onto the audio path at prepare time");
static_assert(gateLayout(1, 0, 1).verdict == LayoutVerdict::accepted
                  && gateLayout(1, 0, 1).processed == 1,
              "(1, 0, 1) — mono main, no sidechain — is the layout every host can offer");
static_assert(gateLayout(2, 2, 2).verdict == LayoutVerdict::accepted
                  && gateLayout(2, 2, 2).processed == 2,
              "(2, 2, 2) — stereo main, stereo sidechain — is the layout the shipped default runs on");
}
