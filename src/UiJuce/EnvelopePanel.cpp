#include "EnvelopePanel.hpp"

#include "Theme.hpp"

#include <algorithm>

namespace ui
{
namespace
{
/** One bus's linear history onto the panel's columns and then onto the dBFS axis.

    The hold happens in the linear domain, where the largest reading is the loudest moment in the column —
    the meter's rule, and the one an averaging decimator would quietly soften. The dB conversion is the
    axis's, applied to the held figure. */
[[nodiscard]] std::size_t plotEnvelope (const float* history, std::size_t historyCount,
                                        double secondsPerValue, double pixelsPerSecond, EnvelopeAxis axis,
                                        Rect area, float* columns, int* ys)
{
    const std::size_t first
        = decimateToColumns (history, historyCount, secondsPerValue, pixelsPerSecond, HoldPolicy::maximum,
                             columns, static_cast<std::size_t> (area.w));

    for (std::size_t column = first; column < static_cast<std::size_t> (area.w); ++column)
        ys[column] = envelopeYAt (amplitudeToDb (static_cast<double> (columns[column])), axis, area);

    return first;
}
}

EnvelopePanel::EnvelopePanel (juce::String componentName)
    : Panel (std::move (componentName), Mode::ink)
{
    addAndMakeVisible (ceilingLabel);
    addAndMakeVisible (floorLabel);
    addAndMakeVisible (readout);
}

void EnvelopePanel::setAxisDbRange (double floorDb, double ceilingDb)
{
    if (axis.floorDb == floorDb && axis.ceilingDb == ceilingDb)
        return;

    axis = EnvelopeAxis { floorDb, ceilingDb };

    const auto top = juce::String (ceilingDb, 0);
    const auto bottom = juce::String (floorDb, 0) + " dB";
    if (ceilingLabel.getText() != top)
        ceilingLabel.setText (top, juce::dontSendNotification);
    if (floorLabel.getText() != bottom)
        floorLabel.setText (bottom, juce::dontSendNotification);

    /* The held history is re-plotted rather than left where it was: the scale moved, and a trace drawn
       against the old axis under labels naming the new one is two claims about the same signal. */
    refresh();
    repaint();
}

void EnvelopePanel::setGateDb (double gateDb)
{
    if (gate == gateDb)
        return;

    /* The rule alone moves: the dial changed, not the signal, so the three traces stay where the axis puts
       them. Repainting without a rebuild is the whole cost, and it is what criterion 5's leg measures. */
    gate = gateDb;
    repaint();
}

void EnvelopePanel::setChannelCount (int channels)
{
    processedChannels = channels;

    /* One channel leaves the readout to the transport's state; with a choice, the panel names the one it
       drew, because an unlabelled trace of channel 0 out of eight reads as a trace of everything. */
    if (readout.getText() != "idle" && channels <= 1)
        readout.setText ({}, juce::dontSendNotification);
}

void EnvelopePanel::setEnvelopes (const float* mainA, const float* sideB, const float* convolved,
                                 std::size_t count, double secondsPerEnvelope, bool advancing)
{
    hopSeconds = secondsPerEnvelope;

    if (!advancing || count == 0)
    {
        if (readout.getText() != "idle")
            readout.setText ("idle", juce::dontSendNotification);
        return;
    }

    const auto keep = std::min (count, kMaxTraceHistory);
    const auto skip = count - keep;
    for (std::size_t i = 0; i < keep; ++i)
    {
        historyA[i] = mainA[skip + i];
        historyB[i] = sideB[skip + i];
        historyConv[i] = convolved[skip + i];
    }
    historyCount = keep;

    refresh();

    if (processedChannels > 1)
    {
        const auto words = "ch 1 / " + juce::String (processedChannels);
        if (readout.getText() != words)
            readout.setText (words, juce::dontSendNotification);
    }
    repaint();
}

void EnvelopePanel::refresh()
{
    const auto bounds = contentArea();
    const Rect area {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};
    columnCount = static_cast<std::size_t> (std::max (0, area.w));

    if (columnCount == 0 || historyCount == 0 || hopSeconds <= 0.0)
        return;

    const double pixelsPerSecond = tracePixelsPerSecond (area.w, kTraceWindowSeconds);
    const int thickness = traceThicknessAt (kitScaleFor (*this));

    firstA = plotEnvelope (historyA.data(), historyCount, hopSeconds, pixelsPerSecond, axis, area,
                           columnsA.data(), ysA.data());
    firstB = plotEnvelope (historyB.data(), historyCount, hopSeconds, pixelsPerSecond, axis, area,
                           columnsB.data(), ysB.data());
    firstConv = plotEnvelope (historyConv.data(), historyCount, hopSeconds, pixelsPerSecond, axis, area,
                              columnsConv.data(), ysConv.data());

    const auto span = [this] (std::size_t first)
                      {
                          return first < columnCount ? columnCount - first : std::size_t {};
                      };

    appendTraceRibbon (traceA, ysA.data(), firstA, span (firstA), thickness, bounds.getX());
    appendTraceRibbon (traceB, ysB.data(), firstB, span (firstB), thickness, bounds.getX());
    appendTraceRibbon (traceConv, ysConv.data(), firstConv, span (firstConv), thickness, bounds.getX());

    ++rebuilds;
}

int EnvelopePanel::gateRuleY() const
{
    const auto bounds = contentArea();
    const Rect area {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};
    return envelopeYAt (gate, axis, area);
}

juce::String EnvelopePanel::readoutWords() const
{
    return readout.getText();
}

void EnvelopePanel::resized()
{
    const auto bounds = contentArea();
    const Rect area {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};
    const int line = lineHeightFor (*this, TypeRole::figure);
    const int labelColumn = std::max (0, area.w / 4);

    ceilingLabel.setBounds (area.x, area.y, labelColumn, line);
    floorLabel.setBounds (area.x, area.bottom() - line, labelColumn, line);
    readout.setBounds (area.x + area.w / 2, area.y, std::max (0, area.w - area.w / 2), line);

    refresh();
}

void EnvelopePanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    ++paints;

    const auto bounds = contentArea();

    g.setColour (toColour (kInkFieldRuleToken));
    g.fillRect (bounds.getX(), gateRuleY(), std::max (0, bounds.getWidth()), 1);

    g.setColour (toColour (kTrack));
    g.fillPath (traceA);

    g.setColour (toColour (kFill));
    g.fillPath (traceB);

    g.setColour (toColour (kTrace));
    g.fillPath (traceConv);
}
}
