/* The kit, painted and then read back off the pixels.

   Offscreen only, like `UiJuceTests`: an Image with the software rasteriser, no peer, no display (INDEX §4).
   A component is placed by its own bounds and rendered with `paintEntireComponent`, which is JUCE's public
   offscreen entry point and the reason this program needs no accessor into a protected `paint()`.

   The canvas starts at `ground` rather than transparent, because criterion 6 asks for the pixel *outside*
   every body edge to be `ground` — a stroke, a shadow or an antialiased ring would show up as a colour that
   is not the sheet's. Criterion 7's palette scan runs on canvases with no words in them, because glyph
   antialiasing produces intermediate values by design and a leg that fails on those is a leg that never
   gets to see the fills.

   Case names carry "ui: probe: " so `ctest -R "ui:"` selects the whole M10 UI suite and a target name stays
   unselectable, which is §10's rule. */

#include <Bar.hpp>
#include <ChoiceBar.hpp>
#include <Panel.hpp>
#include <PieDial.hpp>
#include <Text.hpp>
#include <Theme.hpp>

#include <catch_amalgamated.hpp>

#include <cstdint>
#include <set>
#include <vector>

namespace
{
/* The grid's window scale at the shipped 1190 px sheet, and the ramp's own scale at that window — the two
   k's `02` §1 distinguishes. Every figure below is predicted from the light half, never copied from a
   screenshot. */
constexpr double kWindowScale = 1190.0 / 315.0;
constexpr int kCanvasWidth = 560;
constexpr int kCanvasHeight = 420;
constexpr std::uint32_t kTransparent = 0x00000000U;

juce::Image sheet ()
{
    juce::Image image {juce::Image::ARGB, kCanvasWidth, kCanvasHeight, true};
    juce::Graphics g {image};
    g.fillAll (ui::toColour (ui::kGround));

    return image;
}

/** Render one component into the sheet at the position its own bounds name.

    `paintEntireComponent` draws in the component's local coordinates and does not apply its own offset —
    that is the parent's paint pass, which has no peer here — so the component's position goes on the context
    before the call. */
void render (const juce::Image& image, juce::Component& component)
{
    juce::Graphics g {image};
    g.setOrigin (component.getPosition());
    component.paintEntireComponent (g, true);
}

[[nodiscard]] std::uint32_t argbOf (const juce::Image& image, int x, int y)
{
    return static_cast<std::uint32_t> (image.getPixelAt (x, y).getARGB ());
}

[[nodiscard]] bool isToken (const juce::Image& image, int x, int y, std::uint32_t token)
{
    return argbOf (image, x, y) == token;
}

/** The exact-match bounding box of one token over the whole canvas — the instrument for "where did that
    colour actually land", which is the question criteria 1, 5 and 8 are all asked in pixels. */
[[nodiscard]] juce::Rectangle<int> boundsOf (const juce::Image& image, std::uint32_t token)
{
    juce::Rectangle<int> found;

    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (argbOf (image, x, y) == token)
                found = found.getUnion ({x, y, 1, 1});

    return found;
}

[[nodiscard]] int countOf (const juce::Image& image, std::uint32_t token, juce::Rectangle<int> area)
{
    int count{};

    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
            if (argbOf (image, x, y) == token)
                ++count;

    return count;
}

/** Is the pixel a blend of the two tokens — the same question betweenTokens asks of the pair, in the general
    form the criterion-7 leg needs for antialiased words. */
[[nodiscard]] bool betweenTokens (const juce::Image& image, int x, int y, std::uint32_t one,
                                  std::uint32_t two)
{
    const juce::Colour colour {image.getPixelAt (x, y)};
    const juce::Colour first {one};
    const juce::Colour second {two};

    auto inside = [] (juce::uint8 value, juce::uint8 a, juce::uint8 b)
    {
        const int low = std::min (a, b);
        const int high = std::max (a, b);

        return value >= low - 1 && value <= high + 1;
    };

    return inside (colour.getRed(), first.getRed(), second.getRed())
           && inside (colour.getGreen(), first.getGreen(), second.getGreen())
           && inside (colour.getBlue(), first.getBlue(), second.getBlue());
}

/** Every distinct pixel value on the canvas — criterion 7's question, and the reason the canvases it runs on
    carry no words: antialiased glyph edges are blends of two tokens by construction. */
[[nodiscard]] std::set<std::uint32_t> distinctTokens (const juce::Image& image)
{
    std::set<std::uint32_t> seen;

    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            seen.insert (argbOf (image, x, y));

    return seen;
}

/** The kit with a window scale, installed as the default so every component built under it reads the same
    number through kitScaleFor. */
class Kit
{
public:
    explicit Kit (double pxPerMm = kWindowScale)
    {
        theme.setScale (pxPerMm);
        juce::LookAndFeel::setDefaultLookAndFeel (&theme);
    }

    ~Kit() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    ui::Theme theme;
};

/** A Bar whose active state can be held on without a pointer or a focus engine: a headless canvas can do
    neither, and criterion 5 asks for the active pixel set rather than for a comment about it. Production
    never subclasses this way and `isActive()`'s own expression stays the one under test for the other three
    states. */
class LiveBar final : public ui::Bar
{
public:
    using ui::Bar::Bar;

    void forceActive (bool nowActive)
    {
        heldActive = nowActive;
        repaint();
    }

    [[nodiscard]] bool isActive() const override { return heldActive; }

private:
    bool heldActive{};
};

/** The synthetic pointer. `Slider`'s drag path is `mouseDown` → `mouseDrag`, and the value it lands on is
    the whole of criteria 3 and 4; with no peer there is nobody to deliver the event, so the probe hands it
    to the handler directly. */
juce::MouseEvent makeEvent (juce::Component& target, juce::Point<int> where, bool dragged)
{
    auto& mouse = juce::Desktop::getInstance().getMouseSources().getReference (0);
    const juce::Time now = juce::Time::getCurrentTime();
    /* JUCE's own event object handed straight to the widget's handler. Pressure and the pen angles are the
       pen-and-touch extras this design has no use for, so they carry the zeros a mouse would. */
    return {mouse,
            where.toFloat(),
            juce::ModifierKeys{},
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            &target,
            &target,
            now,
            where.toFloat(),
            now,
            1,
            dragged};
}

/** A Bar that will accept a hand-delivered pointer. */
class ClickableBar final : public ui::Bar
{
public:
    using ui::Bar::Bar;
    using ui::Bar::mouseDown;
    using ui::Bar::mouseDrag;
};

class ClickableDial final : public ui::PieDial
{
public:
    using ui::PieDial::PieDial;
    using ui::PieDial::mouseDown;
    using ui::PieDial::mouseDrag;
};
} // namespace

TEST_CASE ("ui: probe: the bar paints the measured body, inset and fill")
{
    Kit kit;
    const int inset = ui::BarGeometry::insetAt (kWindowScale);
    const int labelColumn = ui::BarGeometry::labelColumnAt (kWindowScale);
    const int bodyHeight = ui::BarGeometry::bodyHeightAt (kWindowScale);

    /* Box 6's row: a labelled bar 264 px wide, so the body is the cell less the label column and the fill
       is half of that at value 0.5. */
    const auto cell = juce::Rectangle<int> {40, 60, 264, bodyHeight};

    LiveBar bar {"level"};
    bar.setRange (0.0, 1.0, 0.0);
    bar.setLabelWords ("LVL");
    bar.setBounds (cell);
    bar.setValue (0.5, juce::dontSendNotification);

    const auto body = bar.body().translated (cell.getX(), cell.getY());

    juce::Image image = sheet();
    render (image, bar);

    SECTION ("the body is the cell less the label column, and it is track")
    {
        CHECK (body.getX() == cell.getX() + labelColumn);
        CHECK (body.getHeight() == bodyHeight);
        CHECK (body.getWidth() == cell.getWidth() - labelColumn);
        CHECK (boundsOf (image, ui::kTrack).getWidth() >= body.getWidth() - 4);
    }

    SECTION ("the fill is flush left and inset top and bottom by the same rounding")
    {
        const auto fill = boundsOf (image, ui::kFill);
        const int predictedWidth = static_cast<int> (std::lround (body.getWidth() * 0.5));

        CHECK (fill.getX() == body.getX());
        CHECK (fill.getWidth() == predictedWidth);
        CHECK (fill.getY() == body.getY() + inset);
        CHECK (fill.getHeight() == body.getHeight() - 2 * inset);

        /* The two inset rows read as the sheet behind the value, which is the only visual difference
           between rest and active and therefore the thing criterion 5 turns on. */
        CHECK (isToken (image, fill.getX(), body.getY(), ui::kTrack));
        CHECK (isToken (image, fill.getX(), body.getBottom() - 1, ui::kTrack));
        CHECK (isToken (image, fill.getX(), fill.getY(), ui::kFill));
    }

    SECTION ("nothing was drawn outside the body's own rect but the label column's words")
    {
        const auto below = body.getY() + body.getHeight();
        const auto right = body.getRight();

        CHECK (isToken (image, body.getX() + 2, below, ui::kGround));
        CHECK (isToken (image, body.getX() + 2, body.getY() - 1, ui::kGround));
        CHECK (isToken (image, right, body.getY() + inset + 1, ui::kGround));
    }
}

TEST_CASE ("ui: probe: no thumb, no outline, anywhere in the body")
{
    Kit kit;
    const auto cell = juce::Rectangle<int> {40, 80, 264, ui::BarGeometry::bodyHeightAt (kWindowScale)};

    /* A bare bar — no label column, no value words — so every pixel in the body has exactly two jobs: it is
       either the track body or the value fill. A thumb, an outline or a gradient would be a third value
       inside that rect, which is what criterion 3 and criterion 6 ask for in one leg. */
    ui::Bar bar {"bare"};
    bar.setRange (0.0, 1.0, 0.0);
    bar.setBounds (cell);
    bar.setValue (0.5, juce::dontSendNotification);

    juce::Image image = sheet();
    render (image, bar);

    const auto body = bar.body().translated (cell.getX(), cell.getY());
    const std::set<std::uint32_t> allowed {ui::kTrack, ui::kFill};

    std::set<std::uint32_t> found = distinctTokens (image);
    found.erase (ui::kGround);

    INFO ("distinct tokens on a wordless bar canvas: " << found.size());
    CHECK (found == allowed);

    int outside{};

    for (int y = body.getY(); y < body.getBottom(); ++y)
        for (int x = body.getX(); x < body.getRight(); ++x)
            if (argbOf (image, x, y) != ui::kTrack && argbOf (image, x, y) != ui::kFill)
                ++outside;

    INFO ("pixels in the body that are neither track nor fill: " << outside);
    CHECK (outside == 0);
}

TEST_CASE ("ui: probe: a click is the body's proportion, at both ends and the middle")
{
    Kit kit;
    const auto cell = juce::Rectangle<int> {0, 0, 264, 24};

    ClickableBar bar {"blend"};
    bar.setRange (-1.0, 1.0, 1.0 / 128.0);
    bar.setBounds (cell);
    bar.setValue (0.0, juce::dontSendNotification);

    const auto body = bar.body().translated (cell.getX(), cell.getY());
    REQUIRE (body.getWidth() == 264);

    /* Five positions: both ends, the centre and the two quarter points. The blend's quantum is 1/128 and the
       body's width is a multiple of nothing, so the predicted value at each pixel is the light half's own
       snap of the position-proportional value — the F-010 identity, checked against the widget rather than
       against a second copy of the arithmetic. */
    const int positions[] = {body.getX(), body.getX() + body.getWidth() / 4, body.getX() + body.getWidth() / 2,
                             body.getX() + (3 * body.getWidth()) / 4, body.getRight() - 1};

    for (const int x : positions)
    {
        bar.setValue (-1.0, juce::dontSendNotification);

        bar.mouseDown (makeEvent (bar, {x, body.getY() + body.getHeight() / 2}, false));

        const double proportion = ui::BarGeometry::forBody ({}, 1.0, 0.0).valueForX (x - body.getX());
        const double across = static_cast<double> (x - body.getX()) / static_cast<double> (body.getWidth());
        const double predicted = ui::snapToQuantum (-1.0 + 2.0 * across, 1.0 / 128.0);

        INFO ("click at x=" << x << " of a body spanning " << body.getX() << "…" << body.getRight()
                            << " gave " << bar.getValue());
        CHECK (bar.getValue() == Catch::Approx (predicted).margin (1e-9));
        CHECK (bar.getValue() == ui::snapToQuantum (bar.getValue(), 1.0 / 128.0));
        CHECK (proportion >= 0.0);
        CHECK (proportion <= 1.0);
    }

    SECTION ("the centre is the range's midpoint, not the midpoint plus half a thumb")
    {
        const int centre = body.getX() + body.getWidth() / 2;
        bar.mouseDown (makeEvent (bar, {centre, body.getY() + 4}, false));

        /* 132/264 is exactly 0.5, and the midpoint of −1…+1 is exactly 0.0 — no quantum can move it. */
        CHECK (bar.getValue() == 0.0);
    }

    SECTION ("a drag that leaves the body clamps instead of wrapping")
    {
        bar.setValue (0.0, juce::dontSendNotification);
        bar.mouseDown (makeEvent (bar, {body.getX() + 66, body.getY() + 4}, false));
        const double onDown = bar.getValue();
        bar.mouseDrag (makeEvent (bar, {body.getRight() + 40, body.getY() + 4}, true));

        INFO ("value on the down-click: " << onDown);
        CHECK (onDown == Catch::Approx (-0.5).margin (1e-9));
        CHECK (bar.getValue() == 1.0);
    }
}

TEST_CASE ("ui: probe: the four states are four pixel sets")
{
    Kit kit;
    const auto cell = juce::Rectangle<int> {40, 90, 264, ui::BarGeometry::bodyHeightAt (kWindowScale)};
    const int inset = ui::BarGeometry::insetAt (kWindowScale);

    auto paint = [] (LiveBar& bar)
    {
        juce::Image image = sheet();
        render (image, bar);

        return image;
    };

    LiveBar bar {"level"};
    bar.setRange (0.0, 1.0, 0.0);
    bar.setLabelWords ("LVL");
    bar.setBounds (cell);
    bar.setValue (0.5, juce::dontSendNotification);

    const juce::Image rest = paint (bar);

    bar.forceActive (true);
    const juce::Image active = paint (bar);

    bar.forceActive (false);
    bar.setEnabled (false);
    const juce::Image inert = paint (bar);

    bar.setEnabled (true);
    bar.setReadOnly (true);
    const juce::Image meter = paint (bar);

    SECTION ("rest and active differ only at the inset rows")
    {
        const auto body = bar.body().translated (cell.getX(), cell.getY());
        int differingOutsideInset{};

        for (int y = body.getY(); y < body.getBottom(); ++y)
            for (int x = body.getX(); x < body.getRight(); ++x)
            {
                const bool anInsetRow = y < body.getY() + inset || y >= body.getBottom() - inset;

                if (argbOf (rest, x, y) != argbOf (active, x, y) && !anInsetRow)
                    ++differingOutsideInset;
            }

        INFO ("pixels differing outside the inset rows: " << differingOutsideInset);
        CHECK (differingOutsideInset == 0);

        /* And they do differ — a state pair that renders identically would satisfy the loop above too. */
        CHECK (argbOf (rest, body.getX(), body.getY()) != argbOf (active, body.getX(), body.getY()));
        CHECK (isToken (rest, body.getX(), body.getY(), ui::kTrack));
        CHECK (isToken (active, body.getX(), body.getY(), ui::kFill));

        /* The other half of "only at the inset rows": at rest the fill is off *both* edges, so the bottom
           inset rows read track and the first inset row reads fill. Containment alone would let a top-only
           inset through — its difference set stays inside the inset bands — which is what arm 2 measured on
           2026-11-14 before these three lines were here. */
        CHECK (isToken (rest, body.getX(), body.getBottom() - 1, ui::kTrack));
        CHECK (isToken (active, body.getX(), body.getBottom() - 1, ui::kFill));
        CHECK (isToken (rest, body.getX(), body.getY() + inset, ui::kFill));
    }

    SECTION ("inert trades the body for ground and keeps the words")
    {
        const auto body = bar.body().translated (cell.getX(), cell.getY());

        CHECK (isToken (inert, body.getX() + 200, body.getY() + inset + 1, ui::kGround));
        CHECK (countOf (inert, ui::kTrack, body) == 0);
        CHECK (countOf (inert, ui::kFill, body) == 0);
        CHECK (boundsOf (inert, ui::kInk) == boundsOf (rest, ui::kInk));
    }

    SECTION ("a meter keeps its body and has no active state at all")
    {
        const auto body = bar.body().translated (cell.getX(), cell.getY());

        CHECK (countOf (meter, ui::kTrack, body) > 0);
        CHECK (countOf (meter, ui::kFill, body) > 0);

        /* `hitTest` takes the component's own coordinates, so the refusal is tested against `body()`
           untranslated. Arm 7 found that the two lines here passed the translated rect on 2026-11-14: that
           point is outside the widget in local space whatever the meter flag says, so the check was true for
           the wrong reason and a read-only bar that answered to the pointer looked clean. */
        const auto local = bar.body();
        CHECK_FALSE (bar.hitTest (local.getX() + 4, local.getY() + 4));
        CHECK_FALSE (bar.hitTest (local.getX() + local.getWidth() / 2, local.getY() + local.getHeight() / 2));

        /* The positive control the two lines above need in order to be able to go red for the reason they
           claim: the same point, on the same widget, is a hit as soon as it stops being a meter. */
        bar.setReadOnly (false);
        CHECK (bar.hitTest (local.getX() + 4, local.getY() + 4));
        bar.setReadOnly (true);

        bar.forceActive (true);
        const juce::Image pressedMeter = paint (bar);
        CHECK (pressedMeter.getPixelAt (body.getX(), body.getY()).getARGB()
               == meter.getPixelAt (body.getX(), body.getY()).getARGB());
    }
}

TEST_CASE ("ui: probe: the fill says the engine's value and the words say the dial's")
{
    Kit kit;
    /* §2.2's two content slots on one widget, in the state the plan calls value≠request: the fill is the
       engine's, the words are the control's, and neither is allowed to move the other. */
    const auto cell = juce::Rectangle<int> {40, 90, 264, ui::BarGeometry::bodyHeightAt (kWindowScale)};

    ui::Bar bar {"applied"};
    bar.setRange (0.0, 1.0, 0.0);
    bar.setLabelWords ("COR");
    bar.setValueWords ("-3.5 dB");
    bar.setBounds (cell);
    bar.setValue (0.25, juce::dontSendNotification);

    juce::Image image = sheet();
    render (image, bar);

    const auto body = bar.body().translated (cell.getX(), cell.getY());
    const auto fill = boundsOf (image, ui::kFill);
    const auto ink = boundsOf (image, ui::kInk);

    CHECK (fill.getWidth() == static_cast<int> (std::lround (body.getWidth() * 0.25)));
    CHECK (ink.getWidth() > 0);

    /* The value words begin at the light half's inset inside the body, not at the body's edge and not in the
       label column. */
    CHECK (ink.getX() >= cell.getX());
    CHECK (ink.getX() < body.getX() + ui::BarGeometry::valueInsetAt (kWindowScale) + 4);
}

TEST_CASE ("ui: probe: the pie's datum and its two mirror wedges")
{
    Kit kit;
    const int faceWidth = ui::faceWidthAt (kWindowScale);
    const int faceHeight = ui::faceHeightAt (kWindowScale);
    const auto face = juce::Rectangle<int> {40, 40, faceWidth, faceHeight};

    SECTION ("proportional has no wedge at zero and its datum at 6 o'clock")
    {
        ui::PieDial dial {ui::PieMode::proportional, "grain"};
        dial.setRange (0.0, 1.0, 0.0);
        dial.setBounds (face);
        dial.setValue (0.0, juce::dontSendNotification);

        juce::Image image = sheet();
        render (image, dial);

        CHECK (countOf (image, ui::kFill, face) == 0);

        dial.setValue (1.0, juce::dontSendNotification);
        juce::Image full = sheet();
        render (full, dial);

        const auto swept = boundsOf (full, ui::kFill);
        const double radius = ui::radiusFor ({face.getX(), face.getY(), face.getWidth(), face.getHeight()});

        INFO ("swept bounding box " << swept.toString() << " against a face radius of " << radius);
        CHECK (swept.getWidth() >= static_cast<int> (2 * radius) - 2);
        CHECK (swept.getHeight() >= static_cast<int> (2 * radius) - 2);

        /* A quarter of the range from a 6 o'clock datum, growing clockwise, lands in the lower-left
           quadrant: filled pixels below the centre and left of it, and none below-right. */
        dial.setValue (0.25, juce::dontSendNotification);
        juce::Image quarter = sheet();
        render (quarter, dial);

        const ui::PointF centre = ui::centreOf ({face.getX(), face.getY(), face.getWidth(), face.getHeight()});
        const int probe = static_cast<int> (radius * 0.7);

        CHECK (isToken (quarter, static_cast<int> (centre.x) - probe / 2, static_cast<int> (centre.y) + probe,
                        ui::kFill));
        CHECK (isToken (quarter, static_cast<int> (centre.x) + probe / 2, static_cast<int> (centre.y) + probe,
                        ui::kTrack));
    }

    SECTION ("datumAtZero: no wedge at p = 0, and the ink radial instead")
    {
        ui::PieDial dial {ui::PieMode::datumAtZero, "blend"};
        dial.setRange (-1.0, 1.0, 1.0 / 128.0);
        dial.setBounds (face);
        dial.setValue (0.0, juce::dontSendNotification);

        juce::Image image = sheet();
        render (image, dial);

        CHECK (countOf (image, ui::kFill, face) == 0);

        const auto radial = boundsOf (image, ui::kInk);
        const ui::PointF centre = ui::centreOf ({face.getX(), face.getY(), face.getWidth(), face.getHeight()});

        /* The datum is a 12 o'clock mark: a vertical strip from the rim of the inscribed circle down to the
           centre, one measured stroke wide. The rim — not the face rect's top edge — because the face is
           209 × 218 and the circle is inscribed by the width, leaving the face's own top rows outside it. */
        const double radius = ui::radiusFor ({face.getX(), face.getY(), face.getWidth(), face.getHeight()});
        const int rimRow = static_cast<int> (std::lround (centre.y - radius));

        INFO ("radial bounds " << radial.toString() << ", rim row " << rimRow);
        CHECK (radial.getWidth() == ui::datumRadialWidthAt (kWindowScale));
        CHECK (std::abs (radial.getY() - rimRow) <= 1);
        CHECK (radial.getBottom() >= static_cast<int> (std::lround (centre.y)) - 2);
        CHECK (std::abs (radial.getX() + radial.getWidth() / 2 - static_cast<int> (std::lround (centre.x))) <= 1);
    }

    SECTION ("p = +0.5 and p = −0.5 are mirror images across the vertical")
    {
        auto wedgeFor = [&] (double value)
        {
            ui::PieDial dial {ui::PieMode::datumAtZero, "blend"};
            dial.setRange (-1.0, 1.0, 1.0 / 128.0);
            dial.setBounds (face);
            dial.setValue (value, juce::dontSendNotification);

            juce::Image image = sheet();
            render (image, dial);

            return image;
        };

        const juce::Image positive = wedgeFor (0.5);
        const juce::Image negative = wedgeFor (-0.5);

        const double centreLine = static_cast<double> (face.getX()) + face.getWidth() * 0.5;
        const ui::PointF centre = ui::centreOf ({face.getX(), face.getY(), face.getWidth(), face.getHeight()});
        const double radius = ui::radiusFor ({face.getX(), face.getY(), face.getWidth(), face.getHeight()});

        /* A wedge that grew the wrong way for the law has nothing to hide behind any of the three
           measurements below, and none of them is sensitive to the one thing that is not a shape: the two
           extreme radius lines land exactly on pixel boundaries (the rim at centre ± radius) and the
           rasteriser resolves that pair one pixel apart, so the pixel comparison runs only where the arc is
           well inside the rim and the count comparison carries a fraction of a percent. */
        const int positiveFill = countOf (positive, ui::kFill, face);
        const int negativeFill = countOf (negative, ui::kFill, face);

        INFO ("+ fill " << positiveFill << ", − fill " << negativeFill);
        CHECK (positiveFill > 0);
        CHECK (std::abs (positiveFill - negativeFill) * 1000 <= positiveFill);

        auto centroidX = [&] (const juce::Image& image)
        {
            double sum{};
            int count{};

            for (int y = face.getY(); y < face.getBottom(); ++y)
                for (int x = face.getX(); x < face.getRight(); ++x)
                    if (isToken (image, x, y, ui::kFill))
                    {
                        sum += x;
                        ++count;
                    }

            return count > 0 ? sum / static_cast<double> (count) : centre.x;
        };

        const double positiveCentroid = centroidX (positive);
        const double negativeCentroid = centroidX (negative);

        INFO ("centroid offset from the datum: +" << (positiveCentroid - centreLine) << " vs −"
              << (negativeCentroid - centreLine));
        /* Measured on this host: +44.422 px against −45.430 px, a 1.007 px residue. That is the two extreme
           radius lines — each of which lands exactly on a pixel boundary at centre ± radius — resolved one way
           on one side and the other way on its mirror, and it is recorded in §11 rather than tuned away. */
        CHECK (std::abs ((positiveCentroid - centreLine) + (negativeCentroid - centreLine)) <= 2.0);

        int unmatched{};
        juce::Rectangle<int> firstMiss;
        juce::Rectangle<int> lastMiss;

        /* Two pixels of window, which is the rasteriser's edge resolution on a boundary that lands on a
           pixel line, measured as the centroid residue above. The window is what makes the leg about the
           wedge rather than about that; the direction legs below are strict. */
        auto filledAt = [] (const juce::Image& image, int x, int y)
        {
            for (int dx = -2; dx <= 2; ++dx)
                if (isToken (image, x + dx, y, ui::kFill))
                    return true;

            return false;
        };

        for (int y = face.getY(); y < face.getBottom(); ++y)
            for (int x = face.getX(); x < face.getRight(); ++x)
            {
                const double dx = x - centre.x;
                const double dy = y - centre.y;

                /* Two places are not the wedge's to prove: the rim, where the fill's outermost boundary is
                   the face's own bounds, and the datum column, where the ink radial is painted over the top
                   of it and its own one-pixel edge is what a mirrored comparison would otherwise trip on —
                   measured: the last mismatches this leg ever reported were all in that column. Both are
                   measured strictly, and separately, by the two legs above and the radial leg below. */
                if (std::sqrt (dx * dx + dy * dy) > radius - 3.0)
                    continue;

                if (std::abs (x - centreLine) <= 4.0)
                    continue;

                const bool here = filledAt (positive, x, y);
                const bool there = filledAt (negative, static_cast<int> (std::lround (2 * centreLine)) - x, y);

                if (here != there)
                {
                    if (unmatched == 0)
                        firstMiss = {x, y, 1, 1};

                    lastMiss = {x, y, 1, 1};
                    ++unmatched;
                }
            }

        INFO ("unmatched mirrored pixels, inside the rim and clear of the datum: " << unmatched
              << " first at " << firstMiss.toString() << " last at " << lastMiss.toString());
        CHECK (unmatched == 0);

        /* And the positive one is on the right: clockwise from 12 o'clock is the design's "more wet". */
        const auto right = boundsOf (positive, ui::kFill);
        CHECK (right.getX() + right.getWidth() / 2 >= static_cast<int> (std::lround (centreLine)));
    }

    SECTION ("the corners of the face are not the dial, in the pointer or on the sheet")
    {
        ui::PieDial dial {ui::PieMode::proportional, "grain"};
        dial.setRange (0.0, 1.0, 0.0);
        dial.setBounds (face);

        CHECK (dial.hitTest (face.getWidth() / 2, face.getHeight() / 2));
        CHECK_FALSE (dial.hitTest (1, 1));
        CHECK_FALSE (dial.hitTest (face.getWidth() - 2, face.getHeight() - 2));

        /* The same shape, painted. The hit test has refused the corners since `02` wrote it; the paint filled
           them with `track` until 2026-11-15, so the sheet showed a square and the pointer took a circle and
           the two halves of one widget disagreed about its shape — a corner that lights up and does not drag.
           Now everything the pointer refuses is the sheet's own ground, the disc inside it is tinted, and the
           tint's area is the disc's area rather than the widget's. The rim's own antialiased line is excluded
           on both sides, because a curved edge crossing a pixel grid is the one place the rasteriser is
           allowed to mix. */
        dial.setValue (0.0, juce::dontSendNotification);
        juce::Image image = sheet();
        render (image, dial);

        const ui::Rect faceRect {face.getX(), face.getY(), face.getWidth(), face.getHeight()};
        const ui::PointF centre = ui::centreOf (faceRect);
        const double radius = ui::radiusFor (faceRect);

        int tintedInside{};
        int tintedOutside{};

        for (int y = face.getY(); y < face.getBottom(); ++y)
        {
            for (int x = face.getX(); x < face.getRight(); ++x)
            {
                const double dx = x + 0.5 - centre.x;
                const double dy = y + 0.5 - centre.y;
                const double distance = std::sqrt (dx * dx + dy * dy);

                if (isToken (image, x, y, ui::kGround))
                    continue;

                if (distance <= radius - 1.0)
                    ++tintedInside;
                else if (distance > radius + 1.0)
                    ++tintedOutside;
            }
        }

        const double discArea = std::numbers::pi * radius * radius;
        INFO ("tinted pixels — inside the rim " << tintedInside << " against a disc area of " << discArea
              << ", outside it " << tintedOutside);
        CHECK (tintedOutside == 0);
        CHECK (tintedInside >= static_cast<int> (discArea * 0.97));
        CHECK (tintedInside <= static_cast<int> (discArea * 1.03));
    }
}

TEST_CASE ("ui: probe: the choice bar's body, press and words")
{
    Kit kit;
    const auto box = juce::Rectangle<int> {40, 90, 264, ui::BarGeometry::bodyHeightAt (kWindowScale)};

    ui::ChoiceBar choice {"mode"};
    choice.setBounds (box);
    choice.addChoice ("grain", 1);
    choice.addChoice ("spectral", 2);
    choice.setColour (juce::ComboBox::textColourId, ui::toColour (ui::kInk));
    choice.setSelectedId (2, juce::dontSendNotification);

    juce::Image image = sheet();
    render (image, choice);

    SECTION ("the body is track and the box carries no arrow")
    {
        const auto track = boundsOf (image, ui::kTrack);

        CHECK (track.getX() <= box.getX() + 1);
        CHECK (track.getBottom() >= box.getBottom() - 1);
        CHECK (track.getY() >= box.getY());
    }

    SECTION ("the current choice's words sit at the value inset, cap-aligned to the body")
    {
        /* JUCE's ComboBox draws its current value through a child Label it owns and positions through
           LookAndFeel::positionComboBoxText; there is no accessor for it, so the probe finds it among the
           children. */
        juce::Label* label = nullptr;

        for (auto* child : choice.getChildren())
            if (auto* asLabel = dynamic_cast<juce::Label*> (child); asLabel != nullptr)
                label = asLabel;

        REQUIRE (label != nullptr);

        const int inset = ui::BarGeometry::valueInsetAt (kWindowScale);
        const auto ink = boundsOf (image, ui::kInk);
        const float cap = kit.theme.capHeightOf (ui::TypeRole::barValue,
                                                 ui::Theme::fontScaleFor (kWindowScale));

        CHECK (label->getText() == "spectral");
        CHECK (label->getX() == inset);
        INFO ("inked cap top at " << ink.getY() << " against the box top " << box.getY()
                                  << " and a cap height of " << cap);
        CHECK (ink.getWidth() > 0);
        CHECK (std::abs (ink.getY() - box.getY()) <= 2);
    }

    SECTION ("pressed is fill behind the ink, with no gradient and no outline")
    {
        juce::Image pressed = sheet();
        juce::Graphics g {pressed};
        g.setOrigin (box.getPosition());
        kit.theme.drawComboBox (g, box.getWidth(), box.getHeight(), true, box.getWidth() - 20, 0, 20,
                                box.getHeight(), choice);

        CHECK (isToken (pressed, box.getX() + 4, box.getY() + box.getHeight() / 2, ui::kFill));
        CHECK (countOf (pressed, ui::kTrack, box) == 0);
        CHECK (distinctTokens (pressed).size() == 2);
    }
}

TEST_CASE ("ui: probe: the two panel modes and the word that survives them")
{
    Kit kit;
    const auto sheetBounds = juce::Rectangle<int> {12, 12, 420, 300};

    SECTION ("sheet is ground, ink is ink")
    {
        ui::Panel panel {"sheet"};
        panel.setBounds (sheetBounds);

        juce::Image image = sheet();
        render (image, panel);

        CHECK (countOf (image, ui::kGround, sheetBounds) == sheetBounds.getWidth() * sheetBounds.getHeight());
    }

    SECTION ("ink fills ink and the ink-field words are ground")
    {
        ui::Panel panel {"ink", ui::Panel::Mode::ink};
        panel.setBounds (sheetBounds);

        ui::Text title {"wordmark", "ENTANGLE", ui::TypeRole::wordmark, ui::TextAlignment::left,
                        ui::TextSurface::inkField};
        title.setBounds (panel.contentArea().withHeight (ui::resolve (ui::TypeRole::wordmark, 1.0) * 2));

        /* addAndMakeVisible, not addChildComponent: JUCE's paint pass skips a child whose visibility flag is
           down, and a component built without a peer has it down until something raises it. This is also what
           `03`'s sheet does, so the probe is not inventing a state production will not create. */
        panel.addAndMakeVisible (title);

        juce::Image image = sheet();
        render (image, panel);

        const auto titleOnSheet = title.getBounds().translated (sheetBounds.getX(), sheetBounds.getY());
        const int groundWords = countOf (image, ui::kGround, titleOnSheet);

        INFO ("title box on the sheet: " << titleOnSheet.toString() << ", ground pixels in it: " << groundWords);
        CHECK (isToken (image, sheetBounds.getX() + 2, sheetBounds.getY() + 2, ui::kInk));
        CHECK (title.getWidth() > 0);
        CHECK (groundWords > 0);
    }
}

TEST_CASE ("ui: probe: the button is flat and two-token")
{
    Kit kit;
    const auto bounds = juce::Rectangle<int> {40, 90, 120, 40};

    /* The word inside a bypass button is JUCE's own drawButtonText pass, in the theme's ink and — until
       `03` overrides getTextButtonFont — JUCE's default face, so the exact-token legs run on a button with no
       word and the labelled one is measured as a two-token canvas. */
    juce::TextButton button {"bypass"};
    button.setButtonText ({});
    button.setBounds (bounds);

    juce::Image atRest = sheet();
    render (atRest, button);

    CHECK (countOf (atRest, ui::kTrack, bounds) == bounds.getWidth() * bounds.getHeight());

    button.setToggleState (true, juce::dontSendNotification);
    juce::Image engaged = sheet();
    render (engaged, button);

    CHECK (countOf (engaged, ui::kFill, bounds) == bounds.getWidth() * bounds.getHeight());

    button.setToggleState (false, juce::dontSendNotification);
    button.setEnabled (false);
    juce::Image inert = sheet();
    render (inert, button);

    CHECK (countOf (inert, ui::kGround, bounds) == bounds.getWidth() * bounds.getHeight());

    SECTION ("a labelled button adds ink and nothing else")
    {
        juce::TextButton labelled {"bypass"};
        labelled.setButtonText ("bypass");
        labelled.setBounds (bounds);

        juce::Image image = sheet();
        render (image, labelled);

        /* The word is antialiased against the body, so the honest form of criterion 7 here is not
           "every pixel is a token" but "every pixel is a blend of the two tokens it lies between": a colour
           whose red, green and blue each sit inside the range the pair spans is a rasteriser artefact of
           ink-on-track, and anything outside it is a new colour the design does not have. */
        int offPalette{};

        for (int y = bounds.getY(); y < bounds.getBottom(); ++y)
            for (int x = bounds.getX(); x < bounds.getRight(); ++x)
                if (! betweenTokens (image, x, y, ui::kTrack, ui::kInk))
                    ++offPalette;

        INFO ("pixels outside the track–ink span: " << offPalette);
        CHECK (offPalette == 0);
    }
}

TEST_CASE ("ui: probe: the colour ids the kit relies on are the theme's own")
{
    Kit kit;
    /* Criterion 11: nothing here invents an id. Every id the kit reads is printed with the value this theme
       gives it, so §10 records what the painting depends on rather than a claim about it. The ids that come
       out not-one-of-the-five are the ones the kit must therefore override rather than colour, which is the
       list `Theme::drawButtonBackground` and friends exist for. */
    const std::vector<std::pair<juce::String, int>> reliedOn {
        {"ResizableWindow::backgroundColourId", juce::ResizableWindow::backgroundColourId},
        {"Slider::trackColourId", juce::Slider::trackColourId},
        {"Slider::thumbColourId", juce::Slider::thumbColourId},
        {"Slider::textBoxHighlightColourId", juce::Slider::textBoxHighlightColourId},
        {"ComboBox::textColourId", juce::ComboBox::textColourId},
        {"ComboBox::backgroundColourId", juce::ComboBox::backgroundColourId},
        {"ComboBox::arrowColourId", juce::ComboBox::arrowColourId},
        {"ComboBox::buttonColourId", juce::ComboBox::buttonColourId},
        {"Label::textColourId", juce::Label::textColourId},
        {"TextButton::buttonColourId", juce::TextButton::buttonColourId},
        {"TextButton::buttonOnColourId", juce::TextButton::buttonOnColourId},
        {"TextButton::textColourOffId", juce::TextButton::textColourOffId},
        {"ToggleButton::tickDisabledColourId", juce::ToggleButton::tickDisabledColourId},
        {"TooltipWindow::backgroundColourId", juce::TooltipWindow::backgroundColourId},
        {"TooltipWindow::textColourId", juce::TooltipWindow::textColourId},
        {"TooltipWindow::outlineColourId", juce::TooltipWindow::outlineColourId},
    };

    const std::set<std::uint32_t> five {ui::kGround, ui::kFill, ui::kTrack, ui::kTrace, ui::kInk};
    std::vector<juce::String> notFive;

    for (const auto& [name, id] : reliedOn)
    {
        const std::uint32_t value = static_cast<std::uint32_t> (kit.theme.findColour (id).getARGB());
        WARN (name << " → 0x" << std::hex << value << std::dec);

        if (five.count (value) == 0)
            notFive.push_back (name);
    }

    /* The kit never paints with an id it has not either set or overridden: the four it overrides by drawing
       (the slider body/thumb/outline, the combo box, the button background) are allowed to answer with
       something else, because the answer is never used. This leg's teeth are the ids it *reads* — the two
       text ids — and those must be tokens. */
    INFO ("ids outside the five: " << [&notFive]
                                   {
                                       juce::String list;
                                       for (const auto& name : notFive)
                                           list += name + " ";
                                       return list;
                                   }().toRawUTF8());

    const std::uint32_t labelInk = static_cast<std::uint32_t> (kit.theme.findColour (juce::Label::textColourId).getARGB());
    const std::uint32_t comboInk = static_cast<std::uint32_t> (kit.theme.findColour (juce::ComboBox::textColourId).getARGB());

    CHECK (labelInk == ui::kInk);
    CHECK (comboInk == ui::kInk);
}

TEST_CASE ("ui: probe: the kit's geometry follows the window scale, not the window's guesses")
{
    /* Criterion 12's other half in pixels: at k = 1 and k = 3.778 the same component draws at the ratio of
       those two numbers, because both the geometry and the ramp go through the single scale the Theme
       holds. */
    const int bodyAtRef = ui::BarGeometry::bodyHeightAt (kWindowScale);
    const int bodyAtOne = ui::BarGeometry::bodyHeightAt (1.0);

    SECTION ("a bar at k = 1")
    {
        Kit kit {1.0};
        ui::Bar bar {"level"};
        bar.setRange (0.0, 1.0, 0.0);
        bar.setBounds ({10, 10, 70, bodyAtOne});
        bar.setValue (0.5, juce::dontSendNotification);

        juce::Image image = sheet();
        render (image, bar);

        const auto fill = boundsOf (image, ui::kFill);

        CHECK (ui::BarGeometry::insetAt (1.0) == 1);
        CHECK (bodyAtRef == 67);
        CHECK (bodyAtOne == 18);
        CHECK (fill.getY() == 10 + 1);
        CHECK (fill.getHeight() == bodyAtOne - 2);
        CHECK (fill.getWidth() == 35);
    }

    SECTION ("a bar at k = 2")
    {
        Kit kit {2.0};
        ui::Bar bar {"level"};
        bar.setRange (0.0, 1.0, 0.0);
        bar.setBounds ({10, 10, 140, ui::BarGeometry::bodyHeightAt (2.0)});
        bar.setValue (0.5, juce::dontSendNotification);

        juce::Image image = sheet();
        render (image, bar);

        CHECK (ui::BarGeometry::bodyHeightAt (2.0) == 35);
        CHECK (ui::BarGeometry::insetAt (2.0) == 2);
        CHECK (boundsOf (image, ui::kFill).getY() == 12);
        CHECK (boundsOf (image, ui::kFill).getWidth() == 70);
    }
}

int main (int argc, char** argv)
{
    /* Own main, for UiJuceTests' stated reasons: an Image, a Typeface and the glyph cache all need JUCE's
       GUI side up, and shutdownJuce_GUI is what releases them. Nothing here creates a peer, so the software
       rasteriser is happy with no display (INDEX §4). CATCH_AMALGAMATED_CUSTOM_MAIN keeps this the only main
       and the binary to one Catch2. */
    juce::ScopedJuceInitialiser_GUI gui;

    Catch::Session session;
    return session.run (argc, argv);
}
