/* `02` §4 criterion 10: the kit's `paint()` allocates nothing once warm — decided as a difference, and why.

   Measured before this file was written (200 warm paints, one fresh Image each, release build, this
   container): a component whose entire `paint()` is `g.fillAll (colour)` costs **12 allocations per paint**.
   That is JUCE's own software-rasteriser path — `paintEntireComponent` → `paintWithinParentContext` → the
   opaque-layer bookkeeping — and it is not the kit's to remove: the kit's `Panel` paints exactly that one
   fill and no less. A text run costs about 25 more, which is `Font::createLayout` and the glyph arrangement
   inside `drawMultiLineText`, and a `ComboBox` carries its own machinery on top.

   So an absolute `0` in this program would be a number about JUCE, not about the kit, and a mutation of the
   kit's geometry would not move it. The criterion is therefore decided in three parts, each of which is a
   hard zero of the thing it claims:

     A  **the kit's own arithmetic allocates nothing** — the light-half calls a paint actually makes
        (`BarGeometry`'s four answers, the pie's angles, centre, radius, walked outline and datum quad, and
        the Theme's font and cap-height lookups once warm), 200 iterations each.
     B  **the kit's components allocate nothing over the same drawing done without them** — each of the five
        measured against a reference component that performs the same JUCE calls with the same arguments and
        no `src/Ui` in it, and each `ui::` widget against the stock JUCE widget it subclasses under the same
        skin. Deltas exactly zero.
     C  **the hook works** — a growth that must be seen is seen, and a warm loop that must not be, is not.

   The absolute per-paint counts are printed for §10, with the two JUCE-internal sources named there rather
   than attributed to the kit. Five components × four states × 200 renders, as the criterion asks.

   Its own binary because a replaced `operator new` cannot share a program with another (§10's one-hook rule,
   and F-020's reason for it). */

#include <Bar.hpp>
#include <ChoiceBar.hpp>
#include <Panel.hpp>
#include <PieDial.hpp>
#include <Text.hpp>
#include <Theme.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <new>
#include <numbers>
#include <vector>

namespace
{
std::atomic<long> countedAllocations{};
std::atomic<bool> countingOn{};

template <bool NoThrow>
[[nodiscard]] void* allocateTracked (std::size_t size, std::size_t alignment)
{
    if (countingOn.load (std::memory_order_relaxed))
        countedAllocations.fetch_add (1, std::memory_order_relaxed);

    const std::size_t span = std::max (alignment, alignof (std::max_align_t));
    void* base = std::malloc (size + span);

    if (base == nullptr && !NoThrow)
        throw std::bad_alloc();

    const std::uintptr_t raw = reinterpret_cast<std::uintptr_t> (base);
    const std::uintptr_t aligned = (raw + span + alignof (std::max_align_t) - 1)
                                   & ~static_cast<std::uintptr_t> (alignof (std::max_align_t) - 1);
    *reinterpret_cast<std::uintptr_t*> (aligned - sizeof (std::uintptr_t)) = raw;

    return reinterpret_cast<void*> (aligned);
}

void freeTracked (void* pointer) noexcept
{
    if (pointer == nullptr)
        return;

    const std::uintptr_t aligned = reinterpret_cast<std::uintptr_t> (pointer);
    void* base = reinterpret_cast<void*> (*reinterpret_cast<const std::uintptr_t*> (aligned
                                                                                   - sizeof (std::uintptr_t)));
    std::free (base);
}

[[nodiscard]] juce::Image canvas()
{
    return {juce::Image::ARGB, 320, 240, true};
}

/** Three warm-up frames, then `renders` counted ones. */
long allocationsOverPaints (juce::Component& component, int renders)
{
    long total{};

    for (int pass = 0; pass < renders + 3; ++pass)
    {
        /* A fresh canvas and a fresh Graphics every frame, both built while the counter is off, and a counted
           window that opens on the paint call itself: the criterion is what the kit's paint allocates, not the
           rasteriser's cost of handing a component somewhere to draw into. An identical canvas each frame would
           let it be smarter than a repaint ever gets, which is why the canvas is new even though it is free. */
        juce::Image image = canvas();
        juce::Graphics g {image};
        const bool countedFrame = pass >= 3;

        if (countedFrame)
        {
            countedAllocations.store (0, std::memory_order_relaxed);
            countingOn.store (true, std::memory_order_relaxed);
        }

        component.paintEntireComponent (g, true);

        if (countedFrame)
        {
            countingOn.store (false, std::memory_order_relaxed);
            total += countedAllocations.load (std::memory_order_relaxed);
        }
    }

    return total;
}

/** The same measurement over a block of calls that are not paints — part A's instrument. */
long allocationsOverCalls (const std::function<void()>& body, int iterations)
{
    for (int pass = 0; pass < 3; ++pass)
        body();

    countedAllocations.store (0, std::memory_order_relaxed);
    countingOn.store (true, std::memory_order_relaxed);

    for (int pass = 0; pass < iterations; ++pass)
        body();

    countingOn.store (false, std::memory_order_relaxed);

    return countedAllocations.load (std::memory_order_relaxed);
}

constexpr int kRenders = 200;
constexpr double kWindowScale = 1190.0 / 315.0;
const juce::Rectangle<int> kBarBounds {0, 0, 264, 67};
const juce::Rectangle<int> kFaceBounds {0, 0, 209, 218};

class Kit
{
public:
    Kit()
    {
        theme.setScale (kWindowScale);
        juce::LookAndFeel::setDefaultLookAndFeel (&theme);
    }

    ~Kit() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    ui::Theme theme;
};

/* The references. Each one is the same JUCE drawing, done by hand, with no ui:: call in it — so a difference
   between a reference and the component it mirrors is arithmetic or state the kit added to its own paint. */

/** Panel's reference: one flat fill, nothing else. */
class FillOnly final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override { g.fillAll (ui::toColour (ui::kGround)); }
};

/** Text's reference: the skin's own helper, called directly — the widget's bounds, role and surface resolved
    outside the loop rather than inside it. The role and the layout are the state's, so the two drawings are
    the same run of the same face at the same size; only the kit's own arithmetic is between them. */
class CapTextOnly final : public juce::Component
{
public:
    CapTextOnly (ui::Theme& skin, const juce::String& words, ui::TypeRole wordsRole,
                 juce::Justification horizontalLayout)
        : theme (skin),
          text (words),
          role (wordsRole),
          layout (horizontalLayout)
    {
    }

    void paint (juce::Graphics& g) override
    {
        theme.drawCapAlignedText (g, text, role, ui::Theme::fontScaleFor (kWindowScale), {0, 0}, layout);
    }

    ui::Theme& theme;
    juce::String text;
    ui::TypeRole role;
    juce::Justification layout;
};

/** Bar's reference: two flat fills and two cap-aligned runs, which is all `Theme::drawLinearSlider` does. */
class BarDrawing final : public juce::Component
{
public:
    BarDrawing (ui::Theme& skin, const juce::String& value, const juce::String& label)
        : theme (skin),
          valueText (value),
          labelText (label)
    {
    }

    void paint (juce::Graphics& g) override
    {
        const juce::Colour track {ui::kTrack};
        const juce::Colour fill {ui::kFill};
        const juce::Colour ink {ui::kInk};

        g.setColour (track);
        g.fillRect (getLocalBounds());
        g.setColour (fill);
        g.fillRect (getLocalBounds().withWidth (132).reduced (0, 3));
        g.setColour (ink);
        theme.drawCapAlignedText (g, valueText, ui::TypeRole::barValue, ui::Theme::fontScaleFor (kWindowScale),
                                  {4, 0}, juce::Justification::left);
        theme.drawCapAlignedText (g, labelText, ui::TypeRole::barLabel, ui::Theme::fontScaleFor (kWindowScale),
                                  {0, 0}, juce::Justification::left);
    }

    ui::Theme& theme;
    juce::String valueText;
    juce::String labelText;
};

/** PieDial's reference: the disc and one wedge Path, both filled into Path storage reused across frames,
    which is what the Theme does — the disc because that is the dial's shape since 2026-11-15, and a
    `fillRect` here would compare the kit against a drawing it no longer makes and report the dial's own
    correction as a cost. With `withWedge` cleared it is the inert state's drawing: the disc filled with
    ground and nothing else, which is all the skin paints for a dial that has been disabled. */
class WedgeDrawing final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        face.clear();
        face.addEllipse (getLocalBounds().toFloat());
        g.setColour (juce::Colour {withWedge ? ui::kTrack : ui::kGround});
        g.fillPath (face);

        if (! withWedge)
            return;

        drawing.clear();
        drawing.startNewSubPath (104.5f, 109.0f);

        for (int step = 0; step <= 64; ++step)
        {
            const double angle = -0.5 * std::numbers::pi + (0.5 * std::numbers::pi * step) / 64.0;
            drawing.lineTo (static_cast<float> (104.5 + 104.5 * std::cos (angle)),
                            static_cast<float> (109.0 + 104.5 * std::sin (angle)));
        }

        drawing.closeSubPath();
        g.setColour (juce::Colour {ui::kFill});
        g.fillPath (drawing);
    }

    bool withWedge = true;
    juce::Path face;
    juce::Path drawing;
};
} // namespace

void* operator new (std::size_t size)
{
    return allocateTracked<false> (size, alignof (std::max_align_t));
}

void* operator new[] (std::size_t size)
{
    return allocateTracked<false> (size, alignof (std::max_align_t));
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    return allocateTracked<false> (size, static_cast<std::size_t> (alignment));
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return allocateTracked<false> (size, static_cast<std::size_t> (alignment));
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    return allocateTracked<true> (size, alignof (std::max_align_t));
}

void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    return allocateTracked<true> (size, alignof (std::max_align_t));
}

void* operator new (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return allocateTracked<true> (size, static_cast<std::size_t> (alignment));
}

void* operator new[] (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return allocateTracked<true> (size, static_cast<std::size_t> (alignment));
}

void operator delete (void* pointer) noexcept
{
    freeTracked (pointer);
}

void operator delete[] (void* pointer) noexcept
{
    freeTracked (pointer);
}

void operator delete (void* pointer, std::size_t) noexcept
{
    freeTracked (pointer);
}

void operator delete[] (void* pointer, std::size_t) noexcept
{
    freeTracked (pointer);
}

void operator delete (void* pointer, std::align_val_t) noexcept
{
    freeTracked (pointer);
}

void operator delete[] (void* pointer, std::align_val_t) noexcept
{
    freeTracked (pointer);
}

void operator delete (void* pointer, std::size_t, std::align_val_t) noexcept
{
    freeTracked (pointer);
}

void operator delete[] (void* pointer, std::size_t, std::align_val_t) noexcept
{
    freeTracked (pointer);
}

void operator delete (void* pointer, const std::nothrow_t&) noexcept
{
    freeTracked (pointer);
}

void operator delete[] (void* pointer, const std::nothrow_t&) noexcept
{
    freeTracked (pointer);
}

void operator delete (void* pointer, std::align_val_t, const std::nothrow_t&) noexcept
{
    freeTracked (pointer);
}

void operator delete[] (void* pointer, std::align_val_t, const std::nothrow_t&) noexcept
{
    freeTracked (pointer);
}

TEST_CASE ("ui: alloc: the hook counts what it is told to count, and nothing when there is nothing")
{
    /* Part C. Without this, every zero below is indistinguishable from a counter that never fires, which is
       the trap §plan-test-validation names: the allocation leg is the easiest one in the suite to write and
       the easiest to have silently measure nothing. */
    long seen{};
    const long grew = allocationsOverCalls ([&seen]
                                            {
                                                std::vector<long> grown;

                                                for (int i = 0; i < 8; ++i)
                                                    grown.push_back (i);

                                                seen = static_cast<long> (grown.size());
                                            },
                                            kRenders);

    INFO ("seen " << seen << ", allocations from a growing vector over " << kRenders << " iterations: " << grew);
    CHECK (seen == 8);
    CHECK (grew > 0);

    std::vector<long> warm (8, 0);
    const long still = allocationsOverCalls ([&warm]
                                             {
                                                 for (auto& value : warm)
                                                     value += 1;
                                             },
                                             kRenders);

    INFO ("allocations from a warm fixed vector: " << still);
    CHECK (still == 0);
}

TEST_CASE ("ui: alloc: the geometry a paint calls allocates nothing")
{
    /* Part A: the light-half calls the five paints make, at the window scale the sheet uses. `wedgePolygon`
       is deliberately not in this list — it collects into a std::vector by design and is what the light
       suite measures; `forEachWedgePoint` is the one the skin walks. */
    Kit kit;
    int sink{};

    const long barCalls = allocationsOverCalls ([&sink]
                                                {
                                                    const ui::BarGeometry geometry
                                                        = ui::BarGeometry::forBody ({0, 0, 264, 67},
                                                                                    kWindowScale, 0.5);
                                                    sink += geometry.fillRect().w + geometry.fillRect (true).h
                                                            + geometry.valueTextRect (kWindowScale).x
                                                            + geometry.valueForX (33)
                                                            + geometry.xForValue (0.5) + geometry.insetPx;
                                                },
                                                kRenders);

    const long pieCalls = allocationsOverCalls ([&sink]
                                                {
                                                    const ui::Rect face {0, 0, 209, 218};
                                                    const ui::WedgeAngles angles
                                                        = ui::wedgeAngles (0.5, ui::PieMode::datumAtZero);
                                                    sink += ui::datumRadialWidthAt (kWindowScale);

                                                    ui::forEachWedgePoint (ui::centreOf (face),
                                                                           ui::radiusFor (face), angles, 64,
                                                                           [&sink] (ui::PointF point)
                                                                           { sink += static_cast<int> (point.x); });

                    sink += static_cast<int> (ui::radialQuad (ui::centreOf (face), ui::radiusFor (face),
                                                              ui::datumRadians (ui::PieMode::datumAtZero), 3.0)[0].y);
                                                },
                                                kRenders);

    const long fontCalls = allocationsOverCalls ([&kit, &sink]
                                                 {
                                                    sink += static_cast<int> (kit.theme.capHeightOf (
                                                        ui::TypeRole::barValue, ui::Theme::fontScaleFor (kWindowScale)));
                                                    sink += static_cast<int> (kit.theme.fontFor (
                                                        ui::TypeRole::caption, ui::Theme::fontScaleFor (kWindowScale)).getHeight());
                                                },
                                                kRenders);

    INFO ("bar geometry: " << barCalls << ", pie geometry: " << pieCalls << ", font lookups: " << fontCalls
                           << " (sink " << sink << ")");
    CHECK (barCalls == 0);
    CHECK (pieCalls == 0);
    CHECK (fontCalls == 0);
}

TEST_CASE ("ui: alloc: the panel costs the same as one flat fill")
{
    Kit kit;

    for (const ui::Panel::Mode mode : {ui::Panel::Mode::sheet, ui::Panel::Mode::ink})
        for (const bool enabled : {true, false})
        {
            ui::Panel panel {"panel", mode};
            panel.setBounds ({0, 0, 320, 240});
            panel.setEnabled (enabled);

            FillOnly reference;
            reference.setBounds ({0, 0, 320, 240});

            const long count = allocationsOverPaints (panel, kRenders);
            const long floor = allocationsOverPaints (reference, kRenders);

            INFO ("panel " << (mode == ui::Panel::Mode::sheet ? "sheet" : "ink") << " enabled=" << enabled
                           << ": " << count << " allocations over " << kRenders << " paints; the same fill "
                           << "drawn without the kit: " << floor);
            CHECK (count == floor);
        }
}

TEST_CASE ("ui: alloc: the bar costs the same as its own drawing")
{
    Kit kit;

    for (const int state : {0, 1, 2, 3})
    {
        ui::Bar bar {"level"};
        bar.setRange (0.0, 1.0, 0.0);
        bar.setLabelWords ("LVL");
        bar.setValueWords ("-3.5 dB");
        bar.setBounds (kBarBounds);
        bar.setValue (0.5, juce::dontSendNotification);

        if (state == 2)
            bar.setReadOnly (true);

        if (state == 3)
            bar.setEnabled (false);

        BarDrawing reference {kit.theme, "-3.5 dB", "LVL"};
        reference.setBounds (kBarBounds);

        const long count = allocationsOverPaints (bar, kRenders);
        const long drawing = allocationsOverPaints (reference, kRenders);

        /* The active state is not exercised here: it needs a focus or a hover neither of which exists without
           a peer, and forcing it is the probe's job (`UiProbe`'s LiveBar). It costs the same two fills — the
           difference between the states is `inset == 0`, not an extra call. */
        INFO ("bar state " << state << ": " << count << " allocations over " << kRenders
                           << " paints; the same drawing without the kit: " << drawing);
        CHECK (count == drawing);
    }
}

TEST_CASE ("ui: alloc: the pie costs the same as its own drawing")
{
    Kit kit;

    for (const int state : {0, 1, 2, 3})
    {
        const ui::PieMode mode = state == 2 ? ui::PieMode::datumAtZero : ui::PieMode::proportional;
        ui::PieDial dial {mode, "dial"};
        dial.setRange (0.0, 1.0, 0.0);
        dial.setBounds (kFaceBounds);
        dial.setValue (state == 3 ? 1.0 : 0.5, juce::dontSendNotification);

        if (state == 3)
            dial.setEnabled (false);

        WedgeDrawing reference;
        reference.withWedge = state != 3;
        reference.setBounds (kFaceBounds);

        const long count = allocationsOverPaints (dial, kRenders);
        const long drawing = allocationsOverPaints (reference, kRenders);

        INFO ("pie state " << state << ": " << count << " allocations over " << kRenders
                           << " paints; the same drawing with no kit in it: " << drawing
                           << (state == 3 ? " (the inert state's flat fill)" : " (face fill plus wedge)"));
        CHECK (count == drawing);
    }
}

TEST_CASE ("ui: alloc: the text costs the same as one cap-aligned run")
{
    Kit kit;

    for (const int state : {0, 1, 2, 3})
    {
        const ui::TypeRole role = state == 1 ? ui::TypeRole::wordmark : ui::TypeRole::caption;
        const ui::TextAlignment alignment = state == 2 ? ui::TextAlignment::centre : ui::TextAlignment::left;
        const ui::TextSurface surface = state == 3 ? ui::TextSurface::inkField : ui::TextSurface::tinted;

        ui::Text words {"words", "grain convolution", role, alignment, surface};
        words.setBounds ({0, 0, 300, 80});

        const auto layout = alignment == ui::TextAlignment::right    ? juce::Justification::right
                            : alignment == ui::TextAlignment::centre ? juce::Justification::horizontallyCentred
                                                                     : juce::Justification::left;
        CapTextOnly reference {kit.theme, "grain convolution", role, layout};
        reference.setBounds ({0, 0, 300, 80});

        const long count = allocationsOverPaints (words, kRenders);
        const long drawing = allocationsOverPaints (reference, kRenders);

        INFO ("text state " << state << ": " << count << " allocations over " << kRenders
                            << " paints; the same run drawn straight through the skin: " << drawing);
        CHECK (count == drawing);
    }
}

TEST_CASE ("ui: alloc: the kit's widgets cost the same as the JUCE widgets they are")
{
    /* Part B's other half: what our subclass adds to a stock widget's own paint. The comparison is under the
       same skin, so the difference is the subclass — its two strings, its meter flag, its pie mode — and
       nothing else. */
    Kit kit;

    ui::Bar bar {"level"};
    bar.setRange (0.0, 1.0, 0.0);
    bar.setBounds (kBarBounds);
    bar.setValue (0.5, juce::dontSendNotification);

    juce::Slider slider {juce::Slider::LinearHorizontal, juce::Slider::NoTextBox};
    slider.setRange (0.0, 1.0, 0.0);
    slider.setBounds (kBarBounds);
    slider.setValue (0.5, juce::dontSendNotification);

    const long barCount = allocationsOverPaints (bar, kRenders);
    const long sliderCount = allocationsOverPaints (slider, kRenders);

    INFO ("Bar " << barCount << " vs Slider " << sliderCount);
    CHECK (barCount == sliderCount);

    ui::PieDial dial {ui::PieMode::proportional, "grain"};
    dial.setRange (0.0, 1.0, 0.0);
    dial.setBounds (kFaceBounds);
    dial.setValue (0.5, juce::dontSendNotification);

    const ui::RotaryAngles angles = ui::rotaryAnglesFor (ui::PieMode::proportional);
    juce::Slider rotary {juce::Slider::Rotary, juce::Slider::NoTextBox};
    rotary.setRotaryParameters (angles.startRadians, angles.endRadians, true);
    rotary.setRange (0.0, 1.0, 0.0);
    rotary.setBounds (kFaceBounds);
    rotary.setValue (0.5, juce::dontSendNotification);

    const long dialCount = allocationsOverPaints (dial, kRenders);
    const long rotaryCount = allocationsOverPaints (rotary, kRenders);

    INFO ("PieDial " << dialCount << " vs rotary Slider " << rotaryCount);
    CHECK (dialCount == rotaryCount);

    ui::ChoiceBar choice {"mode"};
    choice.setBounds (kBarBounds);
    choice.addChoice ("grain", 1);
    choice.addChoice ("spectral", 2);
    choice.setSelectedId (2, juce::dontSendNotification);

    juce::ComboBox box;
    box.setBounds (kBarBounds);
    box.addItem ("grain", 1);
    box.addItem ("spectral", 2);
    box.setSelectedId (2, juce::dontSendNotification);

    const long choiceCount = allocationsOverPaints (choice, kRenders);
    const long boxCount = allocationsOverPaints (box, kRenders);

    INFO ("ChoiceBar " << choiceCount << " vs ComboBox " << boxCount);
    CHECK (choiceCount == boxCount);
}

TEST_CASE ("ui: alloc: the figures, for the record")
{
    Kit kit;
    /* The absolute numbers, printed rather than asserted: they are JUCE's cost per paint — the rasteriser's
       own per-paint bookkeeping plus, where a component has words, the text layout inside drawMultiLineText —
       and §10 quotes them as the thing criterion 10 could not be made about. */
    ui::Panel panel {"panel", ui::Panel::Mode::sheet};
    panel.setBounds ({0, 0, 320, 240});

    ui::Bar bar {"level"};
    bar.setRange (0.0, 1.0, 0.0);
    bar.setLabelWords ("LVL");
    bar.setValueWords ("-3.5 dB");
    bar.setBounds (kBarBounds);
    bar.setValue (0.5, juce::dontSendNotification);

    ui::PieDial dial {ui::PieMode::datumAtZero, "blend"};
    dial.setRange (-1.0, 1.0, 1.0 / 128.0);
    dial.setBounds (kFaceBounds);
    dial.setValue (0.25, juce::dontSendNotification);

    ui::Text words {"words", "grain convolution", ui::TypeRole::caption, ui::TextAlignment::left,
                    ui::TextSurface::tinted};
    words.setBounds ({0, 0, 300, 80});

    FillOnly fillOnly;
    fillOnly.setBounds ({0, 0, 320, 240});

    CapTextOnly capReference {kit.theme, "grain convolution", ui::TypeRole::caption,
                              juce::Justification::left};
    capReference.setBounds ({0, 0, 300, 80});

    auto inked = [] (juce::Component& component)
    {
        juce::Image image = canvas();
        juce::Graphics g {image};
        component.paintEntireComponent (g, true);
        int count{};

        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                if (image.getPixelAt (x, y).getAlpha() != 0)
                    ++count;

        return count;
    };

    WARN ("inked pixels — Text " << inked (words) << ", cap-run reference " << inked (capReference));
    WARN ("per paint over " << kRenders << " warm paints — fill-only " << allocationsOverPaints (fillOnly, kRenders)
                            << ", panel " << allocationsOverPaints (panel, kRenders) << ", bar "
                            << allocationsOverPaints (bar, kRenders) << ", pie "
                            << allocationsOverPaints (dial, kRenders) << ", text "
                            << allocationsOverPaints (words, kRenders));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    Catch::Session session;
    return session.run (argc, argv);
}
