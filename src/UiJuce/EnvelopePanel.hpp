#pragma once

#include "Panel.hpp"
#include "Text.hpp"

#include <PanelGeometry.hpp>

#include <array>
#include <cstddef>

namespace ui
{
/** Boxes 11–12: the three buses the level stage follows, in the mockup's own tint order.

    A on `track`, B on `fill`, the convolved stream on `trace` — spec §1's assignment, and the mockup draws
    exactly these three tints in exactly this panel. The panel is the explanation of the one in boxes 7–8:
    the correction there is a function of these three readings and the gate below them.

    The y axis is a dBFS span handed in — the shell gives it the silence-floor parameter's own lower bound
    and 0 dBFS — and the panel's one rule sits at the *live* value of that parameter. The two are separate
    inputs on purpose: an axis tied to the gate would pin the rule to the field's bottom edge, and a rule
    that cannot move is a photograph. The gate's figure is on the dial in box 6, so the panel adds no fourth
    label for it.

    Drawn for channel 0, and named in the readout whenever there is more than one channel to choose from —
    a panel that silently picks a channel is the small lie `03` §0 A refuses ghost controls for. */
class EnvelopePanel final : public Panel
{
public:
    explicit EnvelopePanel (juce::String componentName = "envelopePanel");

    /** The plotted dBFS span. Both figures come from the shell — the floor from the gate parameter's bound,
        never from a level this component would have to invent — and a change re-plots the held history so
        the traces and their labels always describe one scale. */
    void setAxisDbRange (double floorDb, double ceilingDb);

    /** The gate the rule sits on, in dBFS: the live `silenceFloor` value. This moves the rule alone; the
        traces stay on the axis, because the signal did not change when the dial did. */
    void setGateDb (double gateDb);

    /** The processed channel count: above one, the readout names which channel is drawn. */
    void setChannelCount (int channels);

    /** The three buses' linear readings for channel 0, oldest first, one every `secondsPerEnvelope`.

        Linear rather than dB because the conversion is the axis's business: `amplitudeToDb` turns these into
        the figures the rule and the ribbons are drawn from, and silence reaches the axis's bottom edge
        through that clamp rather than through a floor invented here.

        `advancing` is false when the transport published nothing since the last call: the traces hold, the
        readout says `idle`, and nothing is decimated, rebuilt or repainted. */
    void setEnvelopes (const float* mainA, const float* sideB, const float* convolved, std::size_t count,
                       double secondsPerEnvelope, bool advancing);

    void resized () override;
    void paint (juce::Graphics&) override;

    /** The plotted range as set — the read path for the leg that asks whether the axis is derived or
        invented. */
    [[nodiscard]] double axisFloorDb () const noexcept { return axis.floorDb; }
    [[nodiscard]] double ceilingDb () const noexcept { return axis.ceilingDb; }

    /** The gate as the panel holds it — the read path for the leg that moves the parameter and expects the
        rule to move with it. */
    [[nodiscard]] double gateDb () const noexcept { return gate; }

    /** y of the gate rule, placed on the axis. */
    [[nodiscard]] int gateRuleY () const;

    [[nodiscard]] int rebuildCount () const noexcept { return rebuilds; }
    [[nodiscard]] int paintCount () const noexcept { return paints; }
    [[nodiscard]] juce::String readoutWords () const;

private:
    /** Decimates the held history into the current rect and rebuilds the three ribbons. */
    void refresh();

    EnvelopeAxis axis{};
    double gate = 0.0;
    double hopSeconds = 0.0;
    std::array<float, kMaxTraceHistory> historyA{};
    std::array<float, kMaxTraceHistory> historyB{};
    std::array<float, kMaxTraceHistory> historyConv{};
    std::array<float, kMaxTraceColumns> columnsA{};
    std::array<float, kMaxTraceColumns> columnsB{};
    std::array<float, kMaxTraceColumns> columnsConv{};
    std::array<int, kMaxTraceColumns> ysA{};
    std::array<int, kMaxTraceColumns> ysB{};
    std::array<int, kMaxTraceColumns> ysConv{};
    std::size_t historyCount = 0;
    std::size_t columnCount = 0;
    std::size_t firstA = kMaxTraceColumns;
    std::size_t firstB = kMaxTraceColumns;
    std::size_t firstConv = kMaxTraceColumns;
    juce::Path traceA;
    juce::Path traceB;
    juce::Path traceConv;
    Text ceilingLabel { "envelopeCeiling", {}, TypeRole::figure, TextAlignment::right,
                        TextSurface::inkField };
    Text floorLabel { "envelopeFloor", {}, TypeRole::figure, TextAlignment::right,
                      TextSurface::inkField };
    Text readout { "envelopeReadout", "idle", TypeRole::figure, TextAlignment::right,
                   TextSurface::inkField };
    int processedChannels = 0;
    int rebuilds = 0;
    int paints = 0;
};
}
