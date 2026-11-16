#include <Grid.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <cmath>

namespace
{
struct Expected
{
    int index;
    int x;
    int y;
    int w;
    int h;
};

/* Hand-computed from the millimetres at the shipped window, independent of Grid: origin is
   round((10 + col·75) × 1190/315) and round((10 + row·65) × 1190/315), size is round(70k) ×
   round(60k). Pinned as literals rather than recomputed from the same expression the class uses, so
   a change in the class cannot carry the expectation with it. */
constexpr Expected kReferenceBoxes[] = {
    {1, 38, 38, 264, 227},   {2, 321, 38, 264, 227},   {3, 604, 38, 264, 227},
    {4, 888, 38, 264, 227},  {5, 38, 283, 264, 227},   {6, 321, 283, 264, 227},
    {7, 604, 283, 264, 227}, {8, 888, 283, 264, 227},  {9, 38, 529, 264, 227},
    {10, 321, 529, 264, 227}, {11, 604, 529, 264, 227}, {12, 888, 529, 264, 227},
};

int originFromMillimetres(double millimetrePosition, double scale) noexcept
{
    return static_cast<int>(std::lround(millimetrePosition * scale));
}
}

TEST_CASE("ui: k is the only size input")
{
    CHECK(ui::kOf(315) == Catch::Approx(1.0));
    CHECK(ui::kOf(1190) == Catch::Approx(ui::kReferenceScale));
    CHECK(ui::kOf(630) == Catch::Approx(2.0));
    CHECK(ui::Grid::reference().k() == Catch::Approx(1190.0 / 315.0));
    CHECK(ui::kReferenceScale > 3.777);
    CHECK(ui::kReferenceScale < 3.778);
}

TEST_CASE("ui: at k = 1 the grid is the sheet, and the arithmetic closes exactly")
{
    const ui::Grid sheet(315, 210);

    CHECK(sheet.marginPx() == 10);
    CHECK(sheet.gutterPx() == 5);
    CHECK(sheet.cellPx().w == 70);
    CHECK(sheet.cellPx().h == 60);
    CHECK(sheet.sheetHeightPx() == 210);

    CHECK(sheet.box(1) == ui::Rect{10, 10, 70, 60});
    CHECK(sheet.box(4) == ui::Rect{235, 10, 70, 60});
    CHECK(sheet.box(9) == ui::Rect{10, 140, 70, 60});
    CHECK(sheet.box(12) == ui::Rect{235, 140, 70, 60});

    SECTION("the millimetres close on the sheet with no rounding involved")
    {
        CHECK(2 * 10 + 4 * 70 + 3 * 5 == 315);
        CHECK(2 * 10 + 3 * 60 + 2 * 5 == 210);
        CHECK(2 * sheet.marginPx() + 4 * sheet.cellPx().w + 3 * sheet.gutterPx() == 315);
        CHECK(2 * sheet.marginPx() + 3 * sheet.cellPx().h + 2 * sheet.gutterPx() == 210);
    }

    SECTION("a gutter of 6 mm would not close, which is what the 5 mm is for")
    {
        CHECK(2 * 10 + 4 * 70 + 3 * 6 != 315);
    }
}

TEST_CASE("ui: the twelve boxes at 1190 × 794 are the hand-computed millimetres")
{
    const ui::Grid grid = ui::Grid::reference();

    for (const Expected& want : kReferenceBoxes)
    {
        const ui::Rect got = grid.box(want.index);
        INFO("box " << want.index << " got {" << got.x << ", " << got.y << ", " << got.w << ", "
                    << got.h << "}");
        CHECK(got.x == want.x);
        CHECK(got.y == want.y);
        CHECK(got.w == want.w);
        CHECK(got.h == want.h);
    }

    SECTION("and the same holds against the millimetre formula at three other widths")
    {
        for (const int width : {315, 595, 1190, 2380})
        {
            const ui::Grid g(width, width);
            const double scale = ui::kOf(width);
            for (int index = 1; index <= ui::kBoxCount; ++index)
            {
                const int column = (index - 1) % ui::kColumns;
                const int row = (index - 1) / ui::kColumns;
                const ui::Rect got = g.box(index);
                INFO("width " << width << " box " << index);
                CHECK(got.x == originFromMillimetres(ui::kMarginMm + column * (ui::kCellWidthMm +
                                                                                ui::kGutterMm),
                                                     scale));
                CHECK(got.y == originFromMillimetres(ui::kMarginMm + row * (ui::kCellHeightMm +
                                                                             ui::kGutterMm),
                                                     scale));
            }
        }
    }
}

TEST_CASE("ui: the twelve boxes do not overlap")
{
    const ui::Grid grid = ui::Grid::reference();

    for (int a = 1; a <= ui::kBoxCount; ++a)
    {
        const ui::Rect ra = grid.box(a);
        CHECK(ra.w > 0);
        CHECK(ra.h > 0);

        for (int b = a + 1; b <= ui::kBoxCount; ++b)
        {
            const ui::Rect rb = grid.box(b);
            const bool disjoint = ra.right() <= rb.x || rb.right() <= ra.x || ra.bottom() <= rb.y ||
                                  rb.bottom() <= ra.y;
            INFO("boxes " << a << " and " << b);
            CHECK(disjoint);
        }
    }
}

TEST_CASE("ui: gutters are equal to one another within 1 px, on each axis")
{
    const ui::Grid grid = ui::Grid::reference();
    CHECK(grid.gutterPx() == 19);

    /* Measured at the reference width: 19, 19, 20 across and 18, 19 down. Pooling the two axes gives
       a 2 px spread, which is what round(18.889 px) does when it is applied to sums rather than to
       one length; per axis — the only comparison the design makes — the spread is 1 px, which is the
       bound criterion 6 states. Recorded in item §11, not tuned. */
    int narrowest = grid.gutterBetween(0);
    int widest = narrowest;
    for (int column = 0; column + 1 < ui::kColumns; ++column)
    {
        const int gap = grid.gutterBetween(column);
        CHECK(gap == grid.box(column + 2).x - grid.box(column + 1).right());
        narrowest = gap < narrowest ? gap : narrowest;
        widest = gap > widest ? gap : widest;
    }
    CHECK(widest - narrowest <= 1);

    narrowest = grid.rowGapBelow(0);
    widest = narrowest;
    for (int row = 0; row + 1 < ui::kRows; ++row)
    {
        const int gap = grid.rowGapBelow(row);
        CHECK(gap == grid.box(1 + ui::kColumns * (row + 1)).y -
                         grid.box(1 + ui::kColumns * row).bottom());
        narrowest = gap < narrowest ? gap : narrowest;
        widest = gap > widest ? gap : widest;
    }
    CHECK(widest - narrowest <= 1);
}

TEST_CASE("ui: the sheet closes on the window within rounding")
{
    const ui::Grid grid = ui::Grid::reference();
    const int cell = grid.cellPx().w;
    const int margin = grid.marginPx();
    const int gutter = grid.gutterPx();

    /* 2·38 + 4·264 + 3·19 = 1189 against a 1190 px window; the vertical closes to 795 against 794.
       Both are 1 px, and the alternative — a grid whose last edge lands exactly on the window — needs
       a cell that is not 70 mm. */
    CHECK(std::abs(2 * margin + 4 * cell + 3 * gutter - grid.widthPx()) <= 2);
    CHECK(std::abs(2 * margin + 3 * grid.cellPx().h + 2 * gutter - grid.heightPx()) <= 2);

    SECTION("the outer margin is the same on all four edges, within rounding")
    {
        CHECK(grid.box(1).x == margin);
        CHECK(ui::kReferenceWindowWidthPx - grid.box(4).right() == margin);
        CHECK(grid.box(1).y == margin);
        CHECK(ui::kReferenceWindowHeightPx - grid.box(12).bottom() == margin);
    }
}

TEST_CASE("ui: a span is its cells plus the gutters between them")
{
    const ui::Grid grid = ui::Grid::reference();
    const ui::Size cell = grid.cellPx();

    for (int column = 0; column + 1 < ui::kColumns; ++column)
    {
        const ui::Rect two = grid.span(column, 0, 2);
        const ui::Rect first = grid.box(1 + column);
        const ui::Rect second = grid.box(2 + column);

        CHECK(two.x == first.x);
        CHECK(two.right() == second.right());
        CHECK(two.w == 2 * cell.w + grid.gutterBetween(column));
        CHECK(two.w - cell.w == cell.w + grid.gutterBetween(column));
        CHECK(two.h == cell.h);
    }

    SECTION("a full row span reaches the sheet edges, and one column is one box")
    {
        const ui::Rect wide = grid.span(0, 1, 4);
        CHECK(wide.x == grid.box(5).x);
        CHECK(wide.right() == grid.box(8).right());
        CHECK(wide.y == grid.box(5).y);

        CHECK(grid.span(2, 2, 1) == grid.box(11));
    }

    SECTION("arguments outside the grid are clamped, not wrapped or zeroed")
    {
        CHECK(grid.span(6, 0, 1) == grid.box(4));
        CHECK(grid.span(0, 0, 9) == grid.span(0, 0, 4));
        CHECK(grid.span(-3, -2, 0) == grid.box(1));
        CHECK(grid.box(0) == grid.box(1));
        CHECK(grid.box(99) == grid.box(12));
    }
}
