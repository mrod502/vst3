#include "ResponsePanel.hpp"

#include "Theme.hpp"

#include <algorithm>

namespace ui
{
namespace
{
/** The current correction as one figure: one value when the channels agree, the pair when they do not,
    because a single number would hide the spread the panel exists to show. */
[[nodiscard]] juce::String describeCorrection (float minimum, float maximum)
{
    if (minimum == maximum)
        return juce::String (minimum, 1) + " dB";

    return juce::String (minimum, 1) + " / " + juce::String (maximum, 1) + " dB";
}
}

ResponsePanel::ResponsePanel (juce::String componentName)
    : Panel (std::move (componentName), Mode::ink)
{
    addAndMakeVisible (rangeTop);
    addAndMakeVisible (rangeBottom);
    addAndMakeVisible (readout);
}

void ResponsePanel::setCorrectionRange (double rangeDb)
{
    if (axis.rangeDb == rangeDb)
        return;

    axis = CorrectionAxis {rangeDb};

    const auto top = "+" + juce::String (rangeDb, 0);
    const auto bottom = "-" + juce::String (rangeDb, 0);

    if (rangeTop.getText() != top)
        rangeTop.setText (top, juce::dontSendNotification);
    if (rangeBottom.getText() != bottom)
        rangeBottom.setText (bottom, juce::dontSendNotification);

    /* Re-plot the held history against the new axis, so the rule and the two traces always describe the
       same scale — see the same note on the envelope panel's floor. */
    refresh();
    repaint();
}

void ResponsePanel::setCorrections (const float* minimum, const float* maximum, std::size_t count,
                                   double secondsPerHop, bool advancing)
{
    hopSeconds = secondsPerHop;

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
        minimumHistory[i] = minimum[skip + i];
        maximumHistory[i] = maximum[skip + i];
    }
    historyCount = keep;

    refresh();

    const auto newest = historyCount - 1;
    readout.setText (describeCorrection (minimumHistory[newest], maximumHistory[newest]),
                     juce::dontSendNotification);
    repaint();
}

void ResponsePanel::refresh()
{
    const auto bounds = contentArea();
    const Rect area {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};
    columnCount = static_cast<std::size_t> (std::max (0, area.w));

    if (columnCount == 0 || historyCount == 0 || hopSeconds <= 0.0)
        return;

    const double pixelsPerSecond = tracePixelsPerSecond (area.w, kTraceWindowSeconds);

    maximumFirst = decimateToColumns (maximumHistory.data(), historyCount, hopSeconds, pixelsPerSecond,
                                      HoldPolicy::maximum, maximumColumns.data(), columnCount);
    minimumFirst = decimateToColumns (minimumHistory.data(), historyCount, hopSeconds, pixelsPerSecond,
                                      HoldPolicy::minimum, minimumColumns.data(), columnCount);

    for (std::size_t column = minimumFirst; column < columnCount; ++column)
        minimumYs[column] = correctionYAt (static_cast<double> (minimumColumns[column]), axis, area);
    for (std::size_t column = maximumFirst; column < columnCount; ++column)
        maximumYs[column] = correctionYAt (static_cast<double> (maximumColumns[column]), axis, area);

    const int thickness = traceThicknessAt (kitScaleFor (*this));
    appendTraceRibbon (minimumTrace, minimumYs.data(), minimumFirst,
                       minimumFirst < columnCount ? columnCount - minimumFirst : 0, thickness,
                       bounds.getX());
    appendTraceRibbon (maximumTrace, maximumYs.data(), maximumFirst,
                       maximumFirst < columnCount ? columnCount - maximumFirst : 0, thickness,
                       bounds.getX());

    ++rebuilds;
}

int ResponsePanel::unityRuleY() const
{
    const auto bounds = contentArea();
    const Rect area {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};
    return correctionYAt (0.0, axis, area);
}

juce::String ResponsePanel::readoutWords() const
{
    return readout.getText();
}

void ResponsePanel::resized()
{
    const auto bounds = contentArea();
    const Rect area {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};
    const int line = lineHeightFor (*this, TypeRole::figure);
    const int labelColumn = std::max (0, area.w / 4);

    rangeTop.setBounds (area.x, area.y, labelColumn, line);
    rangeBottom.setBounds (area.x, area.bottom() - line, labelColumn, line);
    readout.setBounds (area.x + area.w / 2, area.y, std::max (0, area.w - area.w / 2), line);

    /* Decimation is a function of the column count, so a resize re-decimates from the history rather than
       stretching columns measured against the old width. The transport is not involved: this is the same
       data re-asked. */
    refresh();
}

void ResponsePanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    ++paints;

    const auto bounds = contentArea();

    g.setColour (toColour (kInkFieldRuleToken));
    g.fillRect (bounds.getX(), unityRuleY(), std::max (0, bounds.getWidth()), 1);

    g.setColour (toColour (kTrack));
    g.fillPath (minimumTrace);

    g.setColour (toColour (kFill));
    g.fillPath (maximumTrace);
}
}
