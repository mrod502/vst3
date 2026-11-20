#include <Grid.hpp>
#include <PanelGeometry.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

using namespace ui;

namespace
{
constexpr double kEps = 1e-9;

/** A ±24 dB correction axis and a −60…0 dBFS envelope axis — the shapes both panels hand in, written here
    as literals because the point of these cases is that the axis is an argument. */
constexpr CorrectionAxis kCorrection24 {24.0};
constexpr EnvelopeAxis kEnvelope {-60.0, 0.0};
constexpr Rect kArea {0, 0, 100, 200};
}

TEST_CASE("ui: trace: the column bound is the widest ink panel the sheet can make, and no more")
{
    /* Every trace buffer is kMaxTraceColumns, so the bound is only worth having if it actually holds the
       panel it claims to. The widest ink field is a two-cell span at the resize ceiling, inset once. */
    const Grid widest(kMaximumWindowWidthPx, kMaximumWindowHeightPx);
    const Rect field = inkContentRect(widest.span(2, 1, 2), widest.k());

    CHECK(static_cast<std::size_t>(field.w) <= kMaxTraceColumns);
    /* …and it is not a bound so loose it stops meaning anything: the spare columns are the ten the figure
       was rounded up to, not an order of magnitude. */
    CHECK(kMaxTraceColumns - static_cast<std::size_t>(field.w) <= 10);
}

TEST_CASE("ui: trace: the history bound holds the window at the fastest hop and rate")
{
    /* 2 s at H = 64 and 192 kHz is the most often-repeated publication the shell can be configured for. */
    constexpr double kFastestHopRate = 192000.0 / 64.0;

    CHECK(kTraceWindowSeconds == 2.0);
    CHECK(kMaxTraceHistory >= static_cast<std::size_t>(kTraceWindowSeconds * kFastestHopRate));
}

TEST_CASE("ui: trace: thickness is one column at the reference width, two at the ceiling")
{
    CHECK(traceThicknessAt(1.0) == 1);
    CHECK(traceThicknessAt(kReferenceScale) == 1);
    CHECK(traceThicknessAt(kReferenceScale * 2.0) == 2);
    /* Never zero: a trace thinner than a column is a trace that is not drawn, which is criterion 6's
       failure with a rounding error as its excuse. */
    CHECK(traceThicknessAt(0.05) == 1);
}

TEST_CASE("ui: trace: pixels per second is the panel width over the window")
{
    CHECK(tracePixelsPerSecond(540, 2.0) == 270.0);
    CHECK(tracePixelsPerSecond(0, 2.0) == 0.0);
    CHECK(tracePixelsPerSecond(540, 0.0) == 0.0);
}

TEST_CASE("ui: trace: the correction axis puts unity on the middle line and its bound on the edges")
{
    CHECK(correctionYAt(0.0, kCorrection24, kArea) == 100);
    CHECK(correctionYAt(24.0, kCorrection24, kArea) == 0);
    CHECK(correctionYAt(-24.0, kCorrection24, kArea) == 200);
    CHECK(correctionYAt(12.0, kCorrection24, kArea) == 50);
}

TEST_CASE("ui: trace: the correction axis is the one it was handed, not one it remembers")
{
    /* §5 arm 4: an axis hard-coded to ±24 keeps every assertion above and is wrong the moment the level
       stage's range is anything else. The same reading on two axes must give two positions. */
    constexpr CorrectionAxis narrow {12.0};

    CHECK(correctionYAt(6.0, narrow, kArea) == 50);
    CHECK(correctionYAt(6.0, kCorrection24, kArea) == 75);
    CHECK(correctionYAt(12.0, narrow, kArea) == correctionYAt(24.0, kCorrection24, kArea));
    /* Unity is unity on any axis. */
    CHECK(correctionYAt(0.0, narrow, kArea) == correctionYAt(0.0, kCorrection24, kArea));
}

TEST_CASE("ui: trace: a correction the axis cannot hold is clamped to its edge")
{
    CHECK(correctionYAt(96.0, kCorrection24, kArea) == 0);
    CHECK(correctionYAt(-96.0, kCorrection24, kArea) == 200);
    /* A degenerate axis is the middle line rather than a division by zero. */
    CHECK(correctionYAt(6.0, CorrectionAxis {}, kArea) == 100);
}

TEST_CASE("ui: trace: the envelope axis puts the floor on the bottom edge and the ceiling on the top")
{
    CHECK(envelopeYAt(0.0, kEnvelope, kArea) == 0);
    CHECK(envelopeYAt(-60.0, kEnvelope, kArea) == 200);
    CHECK(envelopeYAt(-30.0, kEnvelope, kArea) == 100);
    CHECK(envelopeYAt(6.0, kEnvelope, kArea) == 0);
    CHECK(envelopeYAt(-120.0, kEnvelope, kArea) == 200);
    CHECK(envelopeYAt(-30.0, EnvelopeAxis {}, kArea) == 200);
}

TEST_CASE("ui: trace: the envelope axis is the one it is handed, and a reading moves with it")
{
    /* Criterion 5's arithmetic: the shell hands the panel a dBFS span and a gate value, and the same reading
       lands in a different place on each. The rule itself is placed by the gate on that axis, so a moved
       parameter moves the line without the signal moving at all. */
    const Rect panel {0, 0, 100, 226};
    const double reading = -20.0;

    for (const double floorDb : {-90.0, -70.0, -60.0, -48.0})
    {
        const EnvelopeAxis axis {floorDb, 0.0};
        const int rule = envelopeYAt(floorDb, axis, panel);
        const int trace = envelopeYAt(reading, axis, panel);
        const double expected = (reading - floorDb) / (0.0 - floorDb);

        CHECK(rule == panel.bottom());
        INFO("floor " << floorDb << " dB");
        CHECK(std::abs((panel.bottom() - trace) / static_cast<double>(panel.h) - expected) < 0.01);
    }
}

TEST_CASE("ui: trace: amplitude to dB is the log, and silence is not a number above the floor")
{
    CHECK(amplitudeToDb(1.0) == 0.0);
    CHECK(std::abs(amplitudeToDb(0.5) + 6.020599913) < kEps);
    CHECK(std::abs(amplitudeToDb(2.0) - 6.020599913) < kEps);
    CHECK(amplitudeToDb(0.0) == std::numeric_limits<double>::lowest());
    CHECK(amplitudeToDb(-1.0) == std::numeric_limits<double>::lowest());

    /* Which is the difference between a silent trace pinned to the gate and one resting on an invented
       mid-scale floor. */
    CHECK(envelopeYAt(amplitudeToDb(0.0), kEnvelope, kArea) == kArea.bottom());
}

TEST_CASE("ui: trace: a column holds the loudest reading inside it, and the quietest under the other policy")
{
    /* 0.25 s per reading against 2 px/s is 0.5 px per reading, so exactly two readings land in a column and
       the hold is the whole question. Values rise, so maximum takes the later of each pair. */
    const std::array<float, 8> values {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    std::array<float, 8> columns {};

    const std::size_t maxFirst
        = decimateToColumns(values.data(), values.size(), 0.25, 2.0, HoldPolicy::maximum, columns.data(),
                            columns.size());
    REQUIRE(maxFirst == 4);
    CHECK(columns[7] == 8.0f);
    CHECK(columns[6] == 6.0f);
    CHECK(columns[5] == 4.0f);
    CHECK(columns[4] == 2.0f);

    const std::size_t minFirst
        = decimateToColumns(values.data(), values.size(), 0.25, 2.0, HoldPolicy::minimum, columns.data(),
                            columns.size());
    REQUIRE(minFirst == 4);
    CHECK(columns[7] == 7.0f);
    CHECK(columns[6] == 5.0f);
    CHECK(columns[5] == 3.0f);
    CHECK(columns[4] == 1.0f);
}

TEST_CASE("ui: trace: a spike survives decimation — peak-hold, not an average")
{
    /* §5 arm 2: an averaging decimator passes every test that checks a smooth series and quietly softens
       the transient the panel exists to show. One loud reading among quiet ones must still reach the top. */
    std::array<float, 64> values {};
    values[40] = 0.9f;
    std::array<float, 32> columns {};

    const std::size_t first = decimateToColumns(values.data(), values.size(), 0.02, 8.0,
                                               HoldPolicy::maximum, columns.data(), columns.size());
    REQUIRE(first < columns.size());

    bool kept = false;
    for (const float column : columns)
        if (column == 0.9f)
            kept = true;
    CHECK(kept);
}

TEST_CASE("ui: trace: the newest reading is pinned to the last column")
{
    /* The time direction, decided in the light half where an equality test can hold it: values are oldest
       first, so the right-hand edge of the panel is the newest hop and the trace runs uphill in this case. */
    const std::array<float, 4> values {1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, 8> columns {};

    const std::size_t first
        = decimateToColumns(values.data(), values.size(), 1.0, 1.0, HoldPolicy::maximum, columns.data(),
                            columns.size());

    REQUIRE(first == 4);
    CHECK(columns[4] == 1.0f);
    CHECK(columns[7] == 4.0f);
}

TEST_CASE("ui: trace: history older than the window is not drawn, and a gap steps rather than gapping")
{
    /* Two seconds between readings against 1 px/s means a column can fall between them. The unfilled one
       takes the earlier reading — a step, never a midpoint — and the leading empties stay left of the
       returned index so the caller draws nothing there. */
    const std::array<float, 3> values {1.0f, 2.0f, 3.0f};
    std::array<float, 8> columns {};

    const std::size_t first
        = decimateToColumns(values.data(), values.size(), 2.0, 1.0, HoldPolicy::maximum, columns.data(),
                            columns.size());

    /* The three readings sit 2 px apart, so they land on columns 3, 5 and 7 and the two columns between
       each pair step. Columns 0…2 received nothing and stay left of the returned index. */
    REQUIRE(first == 3);
    CHECK(std::isnan(columns[2]));
    CHECK(columns[3] == 1.0f);
    CHECK(columns[4] == 1.0f);
    CHECK(columns[5] == 2.0f);
    CHECK(columns[6] == 2.0f);
    CHECK(columns[7] == 3.0f);
}

TEST_CASE("ui: trace: no column at or past the returned index is ever NaN")
{
    /* The caller maps every column from `first` on onto the axis, so a NaN reaching it becomes a trace drawn
       at an undefined y. Both degenerate inputs return the end index instead. */
    std::array<float, 8> columns {};
    const std::array<float, 2> values {1.0f, 2.0f};

    for (const float* none : {static_cast<const float*>(nullptr), values.data()})
    {
        const std::size_t first
            = decimateToColumns(none, 0, 0.25, 2.0, HoldPolicy::maximum, columns.data(), columns.size());
        CHECK(first == columns.size());
    }

    for (const double rate : {0.0, -1.0})
    {
        const std::size_t first = decimateToColumns(values.data(), values.size(), rate, 2.0,
                                                   HoldPolicy::maximum, columns.data(), columns.size());
        CHECK(first == columns.size());
    }
}

TEST_CASE("ui: trace: the spread of one channel is two equal figures")
{
    /* §2.1's honest rendering of `processed == 1`, and criterion 7's coincident traces. */
    const std::array<float, 1> mono {-3.5f};
    const Spread one = spreadOf(mono.data(), mono.size());
    CHECK(one.minimum == -3.5f);
    CHECK(one.maximum == -3.5f);

    const std::array<float, 8> stereoPair {-4.0f, 2.5f, -1.0f, 0.0f, 6.25f, -12.0f, 3.0f, -0.5f};
    const Spread many = spreadOf(stereoPair.data(), stereoPair.size());
    CHECK(many.minimum == -12.0f);
    CHECK(many.maximum == 6.25f);

    const Spread empty = spreadOf(nullptr, 0);
    CHECK(empty.minimum == 0.0f);
    CHECK(empty.maximum == 0.0f);
}
