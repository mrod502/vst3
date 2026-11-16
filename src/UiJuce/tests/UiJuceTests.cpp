#include <FontFactory.hpp>
#include <Theme.hpp>

#include <catch_amalgamated.hpp>

#include <juce_graphics/juce_graphics.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

/* The skin, measured offscreen. There is no display in this container, so nothing here may create a
   peer: every leg renders into a juce::Image with the software rasteriser (INDEX §4).
   Case names carry the "ui: " prefix so ctest -R "ui:" selects both halves of item 01's suite and
   -R "ui: skin:" selects this one — a target name is not a filter in this repo. */
namespace
{
constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 240;
constexpr int kInkX = 24;
constexpr float kReference = static_cast<float> (1190.0 / 315.0);

juce::Image makeCanvas()
{
    /* clear = true: scanInk reads alpha, so an uninitialised canvas would find ink that was never
       drawn. */
    return {juce::Image::ARGB, kCanvasWidth, kCanvasHeight, true};
}

struct Ink
{
    bool found{};
    int top{};
    int left{};
    int right{};
    int bottom{};
};

/** The bounding box of every pixel with any coverage at all — the measurement criterion 4 and
    criterion 3's "narrower than" leg are decided by. */
Ink scanInk(const juce::Image& image)
{
    Ink ink;

    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            if (image.getPixelAt(x, y).getAlpha() == 0)
                continue;

            if (!ink.found)
            {
                ink = {true, y, x, x, y};
                continue;
            }

            ink.left = x < ink.left ? x : ink.left;
            ink.right = x > ink.right ? x : ink.right;
            ink.bottom = y;
        }
    }

    return ink;
}

void drawWithTheme(ui::Theme& theme, const juce::String& text, ui::TypeRole role, float k,
                   int capTopRow, juce::Image& image)
{
    juce::Graphics g(image);
    g.setColour(ui::toColour(ui::kInk));
    theme.drawCapAlignedText(g, text, role, k, {kInkX, capTopRow}, juce::Justification::left);
}

/** The mistake spec §2 names: placing by the em box — a baseline laid out from a metric of the box rather
    than from the cap. The caller names which metric, because the two candidates behave differently and the
    leg has to say which one it is refusing: `getAscent()` is what JUCE's own placement uses, and
    `getHeight()` is the em box itself. This is the control that gives criterion 4 its teeth. */
void drawByMetric(ui::Theme& theme, const juce::String& text, ui::TypeRole role, float k,
                  int capTopRow, float metric, juce::Image& image)
{
    const juce::Font font = theme.fontFor(role, k);
    juce::Graphics g(image);
    g.setColour(ui::toColour(ui::kInk));
    g.setFont(font);
    const int baseline = capTopRow + static_cast<int>(std::lround(metric));
    g.drawMultiLineText(text, kInkX, baseline, kCanvasWidth - kInkX, juce::Justification::left);
}

/** The inked width of a string drawn in a given font. */
int inkedWidth(const juce::String& text, const juce::Font& font)
{
    juce::Image image = makeCanvas();
    juce::Graphics g(image);
    g.setColour(ui::toColour(ui::kInk));
    g.setFont(font);
    g.drawMultiLineText(text, kInkX, 120, kCanvasWidth - kInkX, juce::Justification::left);
    const Ink ink = scanInk(image);
    return ink.found ? ink.right - ink.left : -1;
}

using Scheme = juce::LookAndFeel_V4::ColourScheme;

juce::Colour slot(const ui::Theme::ColourScheme& scheme, Scheme::UIColour which)
{
    return scheme.getUIColour(which);
}
}

TEST_CASE("ui: skin: the nine slots are the five tokens, with four collisions")
{
    const ui::Theme theme;
    const ui::Theme::ColourScheme& scheme = theme.colourScheme();

    CHECK(slot(scheme, Scheme::windowBackground) == ui::toColour(ui::kGround));
    CHECK(slot(scheme, Scheme::widgetBackground) == ui::toColour(ui::kTrack));
    CHECK(slot(scheme, Scheme::menuBackground) == ui::toColour(ui::kTrack));
    CHECK(slot(scheme, Scheme::outline) == ui::toColour(ui::kInk));
    CHECK(slot(scheme, Scheme::defaultText) == ui::toColour(ui::kInk));
    CHECK(slot(scheme, Scheme::defaultFill) == ui::toColour(ui::kFill));
    CHECK(slot(scheme, Scheme::highlightedText) == ui::toColour(ui::kInk));
    CHECK(slot(scheme, Scheme::highlightedFill) == ui::toColour(ui::kFill));
    CHECK(slot(scheme, Scheme::menuText) == ui::toColour(ui::kInk));

    SECTION("the collision counts are ground 1, track 2, fill 2, ink 4, trace 0")
    {
        int ground = 0;
        int fill = 0;
        int track = 0;
        int ink = 0;
        int trace = 0;

        for (int i = 0; i < Scheme::numColours; ++i)
        {
            const auto argb = scheme.getUIColour(static_cast<Scheme::UIColour>(i)).getARGB();
            ground += argb == ui::kGround;
            fill += argb == ui::kFill;
            track += argb == ui::kTrack;
            ink += argb == ui::kInk;
            trace += argb == ui::kTrace;
        }

        STATIC_CHECK(Scheme::numColours == 9);
        CHECK(ground == 1);
        CHECK(track == 2);
        CHECK(fill == 2);
        CHECK(ink == 4);
        CHECK(trace == 0);
    }

    SECTION("and the scheme is not the dark one the base class starts on")
    {
        const Scheme dark = ui::Theme::getDarkColourScheme();
        CHECK(slot(scheme, Scheme::windowBackground) != slot(dark, Scheme::windowBackground));
        CHECK(slot(scheme, Scheme::defaultText) != slot(dark, Scheme::defaultText));
    }
}

TEST_CASE("ui: skin: the scheme reaches the ids initialiseColours derives from it")
{
    /* colourScheme() is the theme's own copy, so reading it back proves nothing about what a widget
       will get. These ids are the ones LookAndFeel_V4 reassigns from the scheme in
       initialiseColours(), which makes each one a read path to the slot behind it — and the reason
       arm 7 (outline → widgetBackground) reddens something in this item rather than only in 02. */
    const ui::Theme theme;

    SECTION("widgetBackground and menuBackground are surfaces, so track")
    {
        CHECK(theme.findColour(juce::TextButton::buttonColourId) == ui::toColour(ui::kTrack));
        CHECK(theme.findColour(juce::ComboBox::backgroundColourId) == ui::toColour(ui::kTrack));
        CHECK(theme.findColour(juce::PopupMenu::backgroundColourId) == ui::toColour(ui::kTrack));
    }

    SECTION("outline is the only line, and ink")
    {
        CHECK(theme.findColour(juce::TextEditor::outlineColourId) == ui::toColour(ui::kInk));
        CHECK(theme.findColour(juce::ComboBox::buttonColourId) == ui::toColour(ui::kInk));
    }

    SECTION("every text slot is ink")
    {
        CHECK(theme.findColour(juce::Label::textColourId) == ui::toColour(ui::kInk));
        CHECK(theme.findColour(juce::ComboBox::textColourId) == ui::toColour(ui::kInk));
        CHECK(theme.findColour(juce::PopupMenu::textColourId) == ui::toColour(ui::kInk));
        CHECK(theme.findColour(juce::PopupMenu::highlightedTextColourId) == ui::toColour(ui::kInk));
        CHECK(theme.findColour(juce::ToggleButton::textColourId) == ui::toColour(ui::kInk));
    }

    SECTION("a selection is a value: fill behind ink")
    {
        CHECK(theme.findColour(juce::PopupMenu::highlightedBackgroundColourId)
              == ui::toColour(ui::kFill));
        CHECK(theme.findColour(juce::ScrollBar::thumbColourId) == ui::toColour(ui::kFill));
    }

    SECTION("no id that the scheme reaches is off-palette")
    {
        const juce::Colour ids[] = {
            theme.findColour(juce::TextButton::textColourOffId),
            theme.findColour(juce::TextButton::textColourOnId),
            theme.findColour(juce::ComboBox::arrowColourId),
            theme.findColour(juce::CaretComponent::caretColourId),
            theme.findColour(juce::Label::textWhenEditingColourId),
        };

        for (const juce::Colour& colour : ids)
        {
            const bool token = colour.getARGB() == ui::kGround || colour.getARGB() == ui::kFill
                               || colour.getARGB() == ui::kTrack || colour.getARGB() == ui::kInk
                               || colour.getARGB() == ui::kTrace;
            CHECK(token);
        }
    }
}

TEST_CASE("ui: skin: the overrides the constructor makes are still in force")
{
    const ui::Theme theme;

    /* Each of these is an id initialiseColours() would derive differently from our scheme, so a
       constructor that applied them before setColourScheme() — item 01 §5 arm 6 — leaves this leg red
       with no compile error to blame. */
    CHECK(theme.findColour(juce::ResizableWindow::backgroundColourId) == ui::toColour(ui::kGround));
    CHECK(theme.findColour(juce::Slider::trackColourId) == ui::toColour(ui::kTrack));
    CHECK(theme.findColour(juce::Slider::thumbColourId) == ui::toColour(ui::kInk));
    CHECK(theme.findColour(juce::Slider::textBoxHighlightColourId) == ui::toColour(ui::kFill));
    CHECK(theme.findColour(juce::TextEditor::highlightColourId) == ui::toColour(ui::kFill));
    CHECK(theme.findColour(juce::ToggleButton::tickDisabledColourId) == ui::toColour(ui::kInk));
    CHECK(theme.findColour(juce::TooltipWindow::backgroundColourId) == ui::toColour(ui::kTrack));
    CHECK(theme.findColour(juce::TooltipWindow::textColourId) == ui::toColour(ui::kInk));
    CHECK(theme.findColour(juce::TooltipWindow::outlineColourId) == ui::toColour(ui::kInk));

    SECTION("the tooltip is a surface with an edge, not JUCE's yellow box with none")
    {
        /* initialiseColours() derives TooltipWindow::backgroundColourId from highlightedFill and sets
           TooltipWindow::outlineColourId to transparent, and LookAndFeel_V2's own default ground is
           0xffeeeebb. The layout report travels in a tooltip (D-023), so this is the one floating
           surface the plugin shows a host by default, and here it is on-palette with an edge. */
        CHECK(theme.findColour(juce::TooltipWindow::backgroundColourId).getARGB() == ui::kTrack);
        CHECK(theme.findColour(juce::TooltipWindow::outlineColourId).getAlpha() == 255);
    }

    SECTION("none of the three alpha-derived ids keeps its base-class tone")
    {
        /* initialiseColours() builds these three as .withAlpha (0.5f) / .withAlpha (0.4f) of a
           scheme colour, which is an off-palette tone the design has no word for. Opaque means the
           override landed. */
        CHECK(theme.findColour(juce::ToggleButton::tickDisabledColourId).getAlpha() == 255);
        CHECK(theme.findColour(juce::TextEditor::highlightColourId).getAlpha() == 255);
        CHECK(theme.findColour(juce::Slider::textBoxHighlightColourId).getAlpha() == 255);
    }

    SECTION("a second theme is the same theme")
    {
        const ui::Theme other;
        CHECK(other.findColour(juce::Slider::trackColourId)
              == theme.findColour(juce::Slider::trackColourId));
        CHECK(other.findColour(juce::ResizableWindow::backgroundColourId)
              == theme.findColour(juce::ResizableWindow::backgroundColourId));
    }
}

TEST_CASE("ui: skin: no public API sets a slot after construction")
{
    /* Criterion 7's first half is a property of the interface, and it is decided in two parts because a
       requires-expression cannot ask about access here: naming an inaccessible member is a hard error
       outside the immediate context, not false. Measured 2026-11-13 — the two probes first written for
       this case produced "'void juce::LookAndFeel_V4::setColourScheme(ColourScheme)' is inaccessible
       within this context" and stopped the build. That refusal is the direct proof and §10 records the
       scratch-TU run that reproduces it; what belongs in the permanent suite is the mechanism that
       causes it, so that deleting it reddens something. */
    SECTION("both inherited slot setters are shadowed private in Theme.hpp")
    {
        const std::filesystem::path header =
            std::filesystem::path{ENTANGLE_SRC_DIR} / "UiJuce" / "Theme.hpp";
        REQUIRE(std::filesystem::is_regular_file(header));

        std::ifstream in(header);
        const std::string text{std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>()};

        const std::size_t start = text.find("\nprivate:");
        REQUIRE(start != std::string::npos);

        const std::string privateSection = text.substr(start);

        CHECK(privateSection.find("using juce::LookAndFeel_V4::setColourScheme;")
              != std::string::npos);
        CHECK(privateSection.find("using juce::LookAndFeel_V4::getCurrentColourScheme;")
              != std::string::npos);

        /* Nothing re-opens the class after the shadowing — a later public: section is how a caller who
           wants to repaint the palette would undo it without touching the comment above it. */
        CHECK(text.find("public:", start) == std::string::npos);
    }

    /* What the shadowing cannot reach, named rather than glossed: through a base reference both setters
       are still live. That residual is F-022's, and the source-policy case is its compensating control.
       Compile-time legs, labelled as such (F-012's caveat). */
    STATIC_CHECK(requires (juce::LookAndFeel_V4& t, const juce::LookAndFeel_V4::ColourScheme& s) {
        t.setColourScheme(s);
    });
    STATIC_CHECK(requires (juce::LookAndFeel_V4& t) {
        t.getCurrentColourScheme().setUIColour(Scheme::outline, juce::Colours::black);
    });

    /* The read path the theme does offer is const, which is why the inherited one is shadowed rather
       than merely unused. */
    const ui::Theme theme;
    CHECK(theme.colourScheme().getUIColour(Scheme::outline) == ui::toColour(ui::kInk));
}

TEST_CASE("ui: skin: the squeeze is applied, and JUCE's floor is on record")
{
    ui::Theme theme;
    const float floor = theme.squeezeFloorAtConstruction();
    const juce::Font label = theme.fontFor(ui::TypeRole::barLabel, 1.0f);

    CAPTURE(floor);
    INFO("JUCE's minimum horizontal scale is " << floor
                                               << "; the ramp asks for 0.65 on barLabel");

    CHECK(theme.fontFor(ui::TypeRole::barLabel, 3.0f).getHorizontalScale() == Catch::Approx(0.65f));
    CHECK(label.getHorizontalScale() == Catch::Approx(0.65f));
    CHECK(theme.fontFor(ui::TypeRole::barValue, 1.0f).getHorizontalScale() == Catch::Approx(1.10f));
    CHECK(theme.fontFor(ui::TypeRole::caption, 1.0f).getHorizontalScale() == Catch::Approx(1.00f));

    SECTION("and 0.65 really does draw narrower than 1.0")
    {
        const juce::String text = "VOL";
        const int squeezed = inkedWidth(text, label);
        const int plain = inkedWidth(text, label.withHorizontalScale(1.0f));

        INFO("inked width at 0.65: " << squeezed << ", at 1.0: " << plain);
        CHECK(squeezed > 0);
        CHECK(plain > 0);
        CHECK(squeezed < plain);
        CHECK(static_cast<float> (squeezed) / static_cast<float> (plain) == Catch::Approx(0.65f).margin(0.06f));
    }

    /* Criterion 3 asked for this leg to fail if the floor exceeds 0.65, and on the pinned submodule
       it does: FontValues::minimumHorizontalScale is 0.7f (juce_FontOptions.cpp:51), so the stated
       condition is met and is asserted here rather than quietly dropped. It is not a defect, because
       the floor is not what the criterion assumed — createFittedText multiplies it by the font's own
       horizontal scale, so it bounds *further* squeezing and cannot lift a 0.65 font to 0.7, and
       drawCapAlignedText goes through drawMultiLineText, which never enters that path. The hazard
       this does record — a 0.65 face drawn with drawFittedText may be squeezed to 0.455 — is F-022's. */
    CHECK(floor > 0.65f);
    CHECK(floor <= 1.0f);
}

TEST_CASE("ui: skin: cap height, not the em box")
{
    ui::Theme theme;

    for (const float k : {1.0f, kReference})
    {
        const int capTopRow = 40;

        for (const ui::TypeRole role : ui::kAllRoles)
        {
            const juce::Font font = theme.fontFor (role, k);
            const float cap = theme.capHeightOf (role, k);

            juce::Image capAligned = makeCanvas();
            drawWithTheme (theme, "E", role, k, capTopRow, capAligned);
            const Ink ink = scanInk (capAligned);

            INFO ("k = " << k << ", requested cap top " << capTopRow << ", ink top " << ink.top
                         << ", cap " << cap << ", ascent " << font.getAscent ()
                         << ", em height " << font.getHeight ());
            REQUIRE (ink.found);
            CHECK (std::abs (ink.top - capTopRow) <= 1);

            /* The control: the same string laid out from the em box's own height. It lands a quarter em
               under the requested row at every role, which is the difference criterion 4 is about.

               What this leg ran before 2026-11-15 was the same control laid out from getAscent(), and that
               control is void on the shipped face: measured with the embedded Neue, the ascent and the cap
               height are the same figure at all seven roles at both scales — a 0 px gap — so placing by the
               ascent puts the ink exactly where placing by the cap does, and the leg passed while seeing
               nothing. The em box cannot equal the cap for any face with lowercase, so this control is live
               wherever the design is drawn. */
            juce::Image emPlaced = makeCanvas();
            drawByMetric (theme, "E", role, k, capTopRow, font.getHeight (), emPlaced);
            const Ink control = scanInk (emPlaced);

            REQUIRE (control.found);
            INFO ("the em-box placement put the ink at " << control.top);
            CHECK (std::abs (control.top - capTopRow) > 1);
        }
    }
}

TEST_CASE("ui: skin: every role resolves at three scales")
{
    ui::Theme theme;

    for (const ui::TypeRole role : ui::kAllRoles)
    {
        for (const float k : {0.5f, 1.0f, 2.0f})
        {
            const juce::Font font = theme.fontFor(role, k);
            const ui::TypeSpec spec = ui::typeSpec(role);

            INFO("role " << static_cast<int> (role) << " at k " << k);
            CHECK(font.getTypefacePtr() != nullptr);
            CHECK(font.getHeight() > 0.0f);
            CHECK(font.getHeight() == Catch::Approx(ui::resolve(role, k)).margin(1e-3));
            CHECK(font.getHorizontalScale() == Catch::Approx(spec.horizontalScale));
        }
    }
}

TEST_CASE("ui: skin: a font is built once per role and scale")
{
    ui::Theme theme;
    CHECK(theme.fontBuildCount() == 0);

    theme.prepareFontsFor(1.0f);
    const int afterPrepare = theme.fontBuildCount();
    CHECK(afterPrepare == static_cast<int> (ui::kRoleCount));

    for (const ui::TypeRole role : ui::kAllRoles)
    {
        (void) theme.fontFor(role, 1.0f);

        /* The paint-time call itself, on a real context: after prepareFontsFor() it must reach the
           cache rather than build, which is the property arm 11 attacks. */
        juce::Image canvas = makeCanvas();
        juce::Graphics g(canvas);
        g.setColour(ui::toColour(ui::kInk));
        theme.drawCapAlignedText(g, "Out", role, 1.0f, {kInkX, 30}, juce::Justification::left);
    }
    CHECK(theme.fontBuildCount() == afterPrepare);

    (void) theme.fontFor(ui::TypeRole::caption, 2.0f);
    CHECK(theme.fontBuildCount() == afterPrepare + 1);

    SECTION("and a second theme keeps its own count")
    {
        ui::Theme other;
        CHECK(other.fontBuildCount() == 0);
        (void) other.fontFor(ui::TypeRole::caption, 1.0f);
        CHECK(other.fontBuildCount() == 1);
        CHECK(theme.fontBuildCount() == afterPrepare + 1);
    }
}

TEST_CASE("ui: skin: the face drawn is the face the design names")
{
    /* The identity, not a report. Until this leg could only print what the host had resolved,
       because a family-name request is a question the host answers — and off macOS it answered with nothing
       at all, JUCE 9 substituting for no unmatched family (F-022). The two weights are embedded now, so there
       is no question left to refuse and the assertion is the plain one: the sheet draws Helvetica Neue, Thin
       where the ramp says thin and Bold where it says bold, on every host. A face that comes back any other
       name means the embedded data did not load, which is the failure this leg now actually catches. */
    const juce::String thin = ui::typefaceDescriptionFor(ui::TypeWeight::thin);
    const juce::String bold = ui::typefaceDescriptionFor(ui::TypeWeight::bold);

    INFO("resolved faces — thin: " << thin.toRawUTF8() << ", bold: " << bold.toRawUTF8());
    CHECK(thin == "Helvetica Neue / Thin");
    CHECK(bold == "Helvetica Neue / Bold");

    for (const ui::TypeRole role : ui::kAllRoles)
    {
        const ui::TypeWeight weight = ui::typeSpec(role).weight;
        const juce::Typeface::Ptr face = ui::typefaceFor(weight);
        const juce::Typeface::Ptr drawn = ui::makeFont(role, 1.0f).getTypefacePtr();

        REQUIRE(face != nullptr);
        INFO("role " << static_cast<int> (role) << " at " << ui::typeSpec(role).emPx << " em, weight "
             << (weight == ui::TypeWeight::bold ? "bold" : "thin") << ", drawn face "
             << (drawn != nullptr ? (drawn->getName() + " / " + drawn->getStyle()).toRawUTF8() : "none"));
        CHECK(drawn != nullptr);
        CHECK(drawn->getName() == "Helvetica Neue");
        CHECK(drawn->getStyle() == (weight == ui::TypeWeight::bold ? "Bold" : "Thin"));
    }
}

int main(int argc, char** argv)
{
    /* Own main for one reason: an Image, a Typeface and the glyph cache all want JUCE's GUI side
       initialised, and shutdownJuce_GUI is what releases them. Headless is fine — the software
       rasteriser needs no window server, and nothing here creates a peer (INDEX §4). Catch2's
       amalgamated source is compiled with CATCH_AMALGAMATED_CUSTOM_MAIN so this is the only main and
       the binary still contains exactly one Catch2. */
    juce::ScopedJuceInitialiser_GUI gui;

    Catch::Session session;
    return session.run(argc, argv);
}
