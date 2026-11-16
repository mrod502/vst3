#pragma once

#include <cmath>

namespace ui
{
/** The sheet's millimetres, named here and nowhere else in the product.

    The grid is exactly closed at these figures — 2·10 + 4·70 + 3·5 = 315 and 2·10 + 3·60 + 2·5 =
    210 — which is what makes the sheet's own black display panels land on columns 3–4 and rows 2–3
    within hand placement (spec §3). */
inline constexpr double kSheetWidthMm = 315.0;
inline constexpr double kSheetHeightMm = 210.0;
inline constexpr double kMarginMm = 10.0;
inline constexpr double kGutterMm = 5.0;
inline constexpr double kCellWidthMm = 70.0;
inline constexpr double kCellHeightMm = 60.0;
inline constexpr int kColumns = 4;
inline constexpr int kRows = 3;
inline constexpr int kBoxCount = kColumns * kRows;

/** The reference window: 1190 × 794 px for the 315 × 210 mm sheet at 96 dpi. */
inline constexpr int kReferenceWindowWidthPx = 1190;
inline constexpr int kReferenceWindowHeightPx = 794;

/** The window range the editor offers: resizable, with every element fixed in relative size to every
    other, which is what one `k` gives.

    The floor is the half-scale sheet, where the `micro` em is 11 px — the point at which the readouts
    stop being readable, which makes it the floor rather than a value to
    cross. The ceiling is twice the reference — nothing in the design breaks above it, and a window wider than
    this is wider than any desktop it would be opened on — so the number exists to bound the drag, not to
    express a design limit.

    Both are exact halves and doubles of the reference, so the sheet's aspect holds at every size in the range
    and there is one ratio to lock rather than three. */
inline constexpr int kMinimumWindowWidthPx = kReferenceWindowWidthPx / 2;
inline constexpr int kMinimumWindowHeightPx = kReferenceWindowHeightPx / 2;
inline constexpr int kMaximumWindowWidthPx = kReferenceWindowWidthPx * 2;
inline constexpr int kMaximumWindowHeightPx = kReferenceWindowHeightPx * 2;

/** 1190 / 794 — the mockup's sheet ratio, and the one the window is locked to at every size in the range. */
inline constexpr double kSheetAspectRatio
    = static_cast<double> (kReferenceWindowWidthPx) / static_cast<double> (kReferenceWindowHeightPx);

/** k for a window of widthPx — the single place a size enters the product. There is no second
    scaling path, which is what keeps either answer to the resize question a small change (spec §3). */
[[nodiscard]] constexpr double kOf(int widthPx) noexcept
{
    return static_cast<double>(widthPx) / kSheetWidthMm;
}

/** The reference k, 3.778 — the value every px figure in the design is quoted at. */
inline constexpr double kReferenceScale = kOf(kReferenceWindowWidthPx);

/** A box in device pixels. */
struct Rect
{
    int x{};
    int y{};
    int w{};
    int h{};

    [[nodiscard]] constexpr int right() const noexcept { return x + w; }
    [[nodiscard]] constexpr int bottom() const noexcept { return y + h; }

    constexpr bool operator==(const Rect&) const noexcept = default;
};

/** A size in device pixels. */
struct Size
{
    int w{};
    int h{};

    constexpr bool operator==(const Size&) const noexcept = default;
};

/** A point in device pixels, on the floating side because the dial's rim and a rotated caption's
    pivot are not integers at any k. Added by item 02 beside Rect and Size rather than minted twice. */
struct PointF
{
    double x{};
    double y{};

    constexpr bool operator==(const PointF&) const noexcept = default;
};

/** The 4 × 3 grid, computed for one window size.

    Rounding is one-time and absolute: every origin is round(mm × k) taken straight from the
    millimetres, never a rounded origin plus a rounded offset. Accumulating instead leaves the last
    column 1 px short of where the millimetres put it at the reference width, and the drift grows
    with k — the one thing the design cannot absorb, since almost nothing is an integer at 3.778.

    Two consequences of the same rule, recorded rather than tuned away (measured at 1190 px): the
    three interior gutters land at 19, 19 and 20 px, and 2·margin + 4·cell + 3·gutter closes to 1189
    rather than 1190. Both are inside the 1 px the design allows; closing them exactly would mean
    widening a cell, which the millimetres forbid. */
class Grid
{
public:
    /** The grid for a window of widthPx × heightPx. k comes from the width alone; heightPx is
        carried so a caller can ask whether the window is the sheet's aspect, and nothing else reads
        it — the vertical geometry is defined by k, exactly like the horizontal. */
    constexpr Grid(int widthPx, int heightPx) noexcept
        : width (widthPx), height (heightPx), scale (kOf (widthPx))
    {
    }

    /** The grid at the reference window. */
    [[nodiscard]] static constexpr Grid reference() noexcept
    {
        return Grid (kReferenceWindowWidthPx, kReferenceWindowHeightPx);
    }

    [[nodiscard]] constexpr double k() const noexcept { return scale; }
    [[nodiscard]] constexpr int widthPx() const noexcept { return width; }
    [[nodiscard]] constexpr int heightPx() const noexcept { return height; }

    /** round(210 mm × k): what a window of this width would have to be as tall as the sheet. */
    [[nodiscard]] int sheetHeightPx() const noexcept { return px(kSheetHeightMm); }

    [[nodiscard]] int marginPx() const noexcept { return px(kMarginMm); }
    [[nodiscard]] int gutterPx() const noexcept { return px(kGutterMm); }
    [[nodiscard]] Size cellPx() const noexcept { return {px(kCellWidthMm), px(kCellHeightMm)}; }

    /** The mockup's own numbering, row-major: 1 2 3 4 / 5 6 7 8 / 9 10 11 12. An index outside
        1…12 is clamped to it rather than trusted — a box is never a silent zero-size rect. */
    [[nodiscard]] Rect box(int oneBasedIndex) const noexcept
    {
        const int first = oneBasedIndex < 1 ? 1 : (oneBasedIndex > kBoxCount ? kBoxCount : oneBasedIndex);
        return rectAt((first - 1) % kColumns, (first - 1) / kColumns);
    }

    /** cols cells from colStart on one row, plus the gutters between them — a first-class grid
        object, not a cell with a wide child. Its left edge is its first box's and its right edge is
        its last box's, so a span and the boxes it covers cannot disagree about where the sheet is.
        Both arguments are clamped into the grid. */
    [[nodiscard]] Rect span(int colStart, int row, int cols) const noexcept
    {
        const int first = colStart < 0 ? 0 : (colStart > kColumns - 1 ? kColumns - 1 : colStart);
        const int count = cols < 1 ? 1 : (cols > kColumns - first ? kColumns - first : cols);
        const int last = first + count - 1;
        return {originX(first), originY(row), originX(last) + cellPx().w - originX(first), cellPx().h};
    }

    /** The gap actually drawn between two adjacent columns after rounding — gutterPx() for two of
        the three gaps at the reference width and 1 px more for the third. */
    [[nodiscard]] int gutterBetween(int leftColumn) const noexcept
    {
        return originX(leftColumn + 1) - (originX(leftColumn) + cellPx().w);
    }

    /** The gap actually drawn below one row, for the same reason in the vertical axis. */
    [[nodiscard]] int rowGapBelow(int row) const noexcept
    {
        return originY(row + 1) - (originY(row) + cellPx().h);
    }

private:
    [[nodiscard]] int px(double millimetres) const noexcept
    {
        return static_cast<int>(std::lround(millimetres * scale));
    }

    /** The column's left edge, straight from the millimetres — never the previous column's rounded
        origin plus a rounded step. */
    [[nodiscard]] int originX(int column) const noexcept
    {
        return px(kMarginMm + column * (kCellWidthMm + kGutterMm));
    }

    [[nodiscard]] int originY(int row) const noexcept
    {
        const int clamped = row < 0 ? 0 : (row > kRows - 1 ? kRows - 1 : row);
        return px(kMarginMm + clamped * (kCellHeightMm + kGutterMm));
    }

    [[nodiscard]] Rect rectAt(int column, int row) const noexcept
    {
        const Size cell = cellPx();
        return {originX(column), originY(row), cell.w, cell.h};
    }

    int width{};
    int height{};
    double scale{};
};
}
