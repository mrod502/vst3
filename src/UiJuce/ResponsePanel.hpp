#pragma once

#include "Panel.hpp"
#include "Text.hpp"

#include <PanelGeometry.hpp>

#include <array>
#include <cstddef>

namespace ui
{
/** Boxes 7–8: the correction the level stage is applying, drawn as its two extremes.

    Two traces rather than one per channel because `processed` can be 8 and the mockup draws one curve: the
    pair is the minimum and the maximum correction of the hop, and with one channel they coincide, which is
    the honest rendering of a single channel rather than a special case. The gap between them is the quantity
    M9 row 1 is still asking about, which is why it is drawn instead of summarised.

    The y axis is ± the stage's own bound, handed in by the shell. The panel names no dB figure of its own, so
    moving the stage's `gainRangeDb` moves the axis, both labels and every plotted point together.

    Flat fills only, as on the rest of the sheet: each trace is one closed ribbon filled with one token, and
    the panel's single rule — 0 dB, the line whose crossing means the correction changed sign — is the only
    other thing drawn. Inside an ink field that rule is a tint (`kInkFieldRuleToken`), because ink on ink
    draws nothing. */
class ResponsePanel final : public Panel
{
public:
    explicit ResponsePanel (juce::String componentName = "responsePanel");

    /** The axis's symmetric bound, from `LevelNormalizer::gainRangeDb`. Retitles the two axis labels, which
        are derived from it rather than written beside it. */
    void setCorrectionRange (double rangeDb);

    /** The hop-by-hop extremes, oldest first, `count` of them, one every `secondsPerHop`.

        `advancing` is false when the transport published nothing since the last call: the traces hold, the
        readout says `idle`, and nothing is decimated, rebuilt or repainted. A stopped plugin shows where its
        traces stopped rather than a line marching toward a plausible zero. */
    void setCorrections (const float* minimum, const float* maximum, std::size_t count,
                         double secondsPerHop, bool advancing);

    void resized () override;
    void paint (juce::Graphics&) override;

    /** The axis as set — the read path for the leg that asks whether the range is the stage's or invented. */
    [[nodiscard]] double axisRangeDb () const noexcept { return axis.rangeDb; }

    /** y of the 0 dB rule, derived from the axis and the panel's own content rect. */
    [[nodiscard]] int unityRuleY () const;

    /** The leg counters: criterion 9's "no drain, no path rebuild, no repaint" is these three numbers. */
    [[nodiscard]] int rebuildCount () const noexcept { return rebuilds; }
    [[nodiscard]] int paintCount () const noexcept { return paints; }
    [[nodiscard]] juce::String readoutWords () const;

private:
    /** Decimates the held history into the current rect and rebuilds both ribbons — the only place a path
        is built, called by the tick and by the layout pass. */
    void refresh();

    CorrectionAxis axis{};
    double hopSeconds = 0.0;
    std::array<float, kMaxTraceHistory> minimumHistory{};
    std::array<float, kMaxTraceHistory> maximumHistory{};
    std::array<int, kMaxTraceColumns> maximumYs{};
    std::array<int, kMaxTraceColumns> minimumYs{};
    std::array<float, kMaxTraceColumns> maximumColumns{};
    std::array<float, kMaxTraceColumns> minimumColumns{};
    std::size_t historyCount = 0;
    std::size_t columnCount = 0;
    std::size_t maximumFirst = kMaxTraceColumns;
    std::size_t minimumFirst = kMaxTraceColumns;
    juce::Path maximumTrace;
    juce::Path minimumTrace;
    Text rangeTop { "responseRangeTop", {}, TypeRole::figure, TextAlignment::right,
                    TextSurface::inkField };
    Text rangeBottom { "responseRangeBottom", {}, TypeRole::figure, TextAlignment::right,
                       TextSurface::inkField };
    Text readout { "responseReadout", "idle", TypeRole::figure, TextAlignment::right,
                   TextSurface::inkField };
    int rebuilds = 0;
    int paints = 0;
};
}
