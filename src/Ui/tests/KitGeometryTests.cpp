#include <Grid.hpp>
#include <PanelGeometry.hpp>
#include <TextGeometry.hpp>

#include <catch_amalgamated.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <regex>
#include <string>
#include <vector>

/* The two thinnest parts of the kit's arithmetic — the ink field's content inset and the rotated
   caption's anchor — plus the leg that keeps the whole light half light. */
namespace
{
constexpr double kReferenceK = 1190.0 / 315.0;
constexpr double kPi = std::numbers::pi;
}

TEST_CASE("ui: panel: the ink field's content inset is one gutter, applied once")
{
    const ui::Grid grid = ui::Grid::reference ();
    const ui::Rect panel = grid.span (2, 1, 2);
    const ui::Rect content = ui::inkContentRect (panel, grid.k ());

    INFO("panel " << panel.w << " × " << panel.h << ", inset " << ui::inkPanelInsetAt (grid.k ()));

    /* The mockup's display panels ride columns 3–4, and that is the one gutter at the reference width
       that rounds to 20 px rather than 19 (item 01 §10 measured 19/19/20). So a two-cell span there is
       548 px wide where the same span at columns 1–2 is 547 — the 1 px the design allows, named here
       because the ink panels are the widest two-cell objects on the sheet. */
    REQUIRE(panel.w == 548);
    REQUIRE(grid.gutterBetween (2) == 20);
    REQUIRE(grid.gutterBetween (0) == 19);
    REQUIRE(panel.h == 227);

    SECTION("the inset is the sheet's own content unit, which is what it was chosen to be")
    {
        CHECK(ui::inkPanelInsetAt (grid.k ()) == grid.gutterPx ());
        CHECK(ui::kInkPanelContentInsetMm == ui::kGutterMm);
    }

    SECTION("symmetric on both axes, applied once, and inside the panel")
    {
        CHECK(content.x - panel.x == ui::inkPanelInsetAt (grid.k ()));
        CHECK(panel.right () - content.right () == ui::inkPanelInsetAt (grid.k ()));
        CHECK(content.y - panel.y == ui::inkPanelInsetAt (grid.k ()));
        CHECK(panel.bottom () - content.bottom () == ui::inkPanelInsetAt (grid.k ()));
        CHECK(content.w == panel.w - 2 * ui::inkPanelInsetAt (grid.k ()));
        CHECK(content.h == panel.h - 2 * ui::inkPanelInsetAt (grid.k ()));
    }

    SECTION("at k = 1 and k = 2 as well, and a panel too thin to inset still has a rect")
    {
        for (const double k : {1.0, 2.0, kReferenceK})
        {
            const ui::Rect field {0, 0, ui::inkPanelInsetAt (k) * 4, ui::inkPanelInsetAt (k) * 4};
            const ui::Rect inside = ui::inkContentRect (field, k);

            INFO("k = " << k);
            CHECK(inside.w == ui::inkPanelInsetAt (k) * 2);
            CHECK(inside.h == ui::inkPanelInsetAt (k) * 2);
        }

        const ui::Rect nothing = ui::inkContentRect ({0, 0, 4, 4}, kReferenceK);
        CHECK(nothing.w == 0);
        CHECK(nothing.h == 0);
    }
}

TEST_CASE("ui: text: a rotated caption is −90°, anchored on the cell's bottom-left")
{
    SECTION("the rule is −90°, not the +91° one hand-drawn element uses")
    {
        CHECK(ui::kRotationRadians == Catch::Approx (-kPi / 2.0));
        CHECK(ui::kRotationRadians < 0.0);
        CHECK(ui::kRotationRadians != Catch::Approx (91.0 * kPi / 180.0));
    }

    const ui::Grid grid = ui::Grid::reference ();
    const ui::Rect cell = grid.box (9);
    const ui::RotatedPlacement placed = ui::rotatedPlacementFor (cell, grid.k ());

    SECTION("the pivot is the caption's bottom-left as the reader sees it, inset from both edges")
    {
        INFO("cell " << cell.w << " × " << cell.h << " at inset "
                     << ui::rotatedCaptionInsetAt (grid.k ()));
        CHECK(ui::rotatedCaptionInsetAt (grid.k ()) == 8);
        CHECK(placed.pivot.x == cell.x + ui::rotatedCaptionInsetAt (grid.k ()));
        CHECK(placed.pivot.y == cell.bottom () - ui::rotatedCaptionInsetAt (grid.k ()));
    }

    SECTION("and the line has the cell's height to grow upward into, minus the same inset twice")
    {
        CHECK(placed.extentPx == cell.h - 2 * ui::rotatedCaptionInsetAt (grid.k ()));
        CHECK(placed.extentPx > 0);
    }

    SECTION("at k = 1 the inset is 2 mm and nothing about the rule changes")
    {
        const ui::Grid sheet (315, 210);
        const ui::RotatedPlacement atOne = ui::rotatedPlacementFor (sheet.box (5), sheet.k ());

        CHECK(ui::rotatedCaptionInsetAt (1.0) == 2);
        CHECK(atOne.pivot.x == sheet.box (5).x + 2);
        CHECK(atOne.pivot.y == sheet.box (5).bottom () - 2);
        CHECK(atOne.extentPx == sheet.box (5).h - 4);
    }
}

TEST_CASE("ui: kit: the light half still includes no JUCE")
{
    /* Spec A5, restated as a leg now that the half has five headers of kit arithmetic in it. The link
       line of UiTests is the proof this repo cites; this is the version that reddens before the link,
       and it looks at every header in the directory rather than the four item 01 shipped. */
    const std::filesystem::path directory = std::filesystem::path{ENTANGLE_SRC_DIR} / "Ui";
    REQUIRE(std::filesystem::is_directory (directory));

    const std::regex includeJuce (R"(#\s*include\s*[<"][^>"]*juce)", std::regex::icase);
    std::vector<std::string> offenders;
    int headers = 0;

    for (const auto& entry : std::filesystem::directory_iterator (directory))
    {
        if (! entry.is_regular_file () || entry.path ().extension () != ".hpp")
            continue;

        ++headers;
        std::ifstream in (entry.path ());
        std::string line;
        int number = 0;

        while (std::getline (in, line))
        {
            ++number;

            if (std::regex_search (line, includeJuce))
                offenders.push_back (entry.path ().filename ().string () + ":"
                                     + std::to_string (number) + "  " + line);
        }
    }

    INFO("headers scanned: " << headers);
    CHECK(headers >= 7);
    CHECK(offenders.empty ());
}
