#include <Palette.hpp>
#include <TypeRamp.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

/* A source-policy case, not a numeric one. INDEX §3 rule 2 makes the theme the only colour source,
   and item 01 §3 step 10 asks for a leg that fails when a colour name, a hex literal or a numeric
   font size appears outside the theme files.

   What it cannot see is a colour computed at run time — the behavioural version of the same rule is
   item 02's probe reading named pixels, and the two are cited together or not at all.

   The shell half was a pinned ceiling until M10 item 03 landed the real editor and became a zero: the
   placeholder's seven recorded sites are gone, and the case now refuses a colour name, a hex literal, a
   numeric font size, a findColour call and a ColourId anywhere under src/Entangle. */
namespace
{
struct Rule
{
    const char* name;
    std::regex pattern;
};

std::vector<std::filesystem::path> sourcesIn(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> found;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
            continue;
        const std::string extension = entry.path().extension().string();
        if (extension == ".cpp" || extension == ".hpp" || extension == ".h")
            found.push_back(entry.path());
    }
    std::sort(found.begin(), found.end());
    return found;
}

/** "file:line  trimmed source" for every line of file that the pattern matches. */
std::vector<std::string> matchingLines(const std::filesystem::path& file, const std::regex& pattern)
{
    std::vector<std::string> found;
    std::ifstream in(file);
    std::string line;
    int number = 0;
    while (std::getline(in, line))
    {
        ++number;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (std::regex_search(line, pattern))
            found.push_back(file.filename().string() + ":" + std::to_string(number) + "  " + line);
    }
    return found;
}

std::string join(const std::vector<std::string>& lines)
{
    std::ostringstream out;
    for (const std::string& line : lines)
        out << "\n      " << line;
    return out.str();
}

const std::regex kColourName(R"(Colours::)");
const std::regex kHexLiteral(R"(0[xX][0-9a-fA-F]{6,})");
const std::regex kNumericSetFont(R"(setFont\s*[({]\s*[0-9])");
/* A font height written as a number rather than taken from the ramp. Both spellings and both bracket
   styles, because JUCE 9 builds a Font out of FontOptions and either form compiles. A named constant
   is not a match and does not need one: the height createSystemTypefaceFor() demands is an argument
   whose value never reaches a drawn glyph, and it is named for that reason. */
const std::regex kNumericFontHeight(R"(((FontOptions|withHeight|Font)\s*[({]\s*[0-9]*\.?[0-9]+f?))");

const std::array<Rule, 4> kSkinRules{{
    {"a Colours:: name", kColourName},
    {"a hex literal", kHexLiteral},
    {"a numeric setFont", kNumericSetFont},
    {"a numeric font height", kNumericFontHeight},
}};

/* The seven em figures. Only TypeRamp.hpp may name them; anywhere else in either library one of
   them is an eighth size, which is what criterion 5 forbids. */
const std::array<const char*, 7> kEmFigures{"64.0f", "56.0f", "80.0f", "63.0f", "52.0f", "35.0f",
                                            "22.0f"};

const std::filesystem::path kSrc(ENTANGLE_SRC_DIR);
}

TEST_CASE("ui: policy: the skin names no colour and no size of its own")
{
    const std::filesystem::path skin = kSrc / "UiJuce";
    REQUIRE(std::filesystem::is_directory(skin));

    const std::vector<std::filesystem::path> files = sourcesIn(skin);
    REQUIRE_FALSE(files.empty());

    std::vector<std::string> offenders;
    for (const auto& file : files)
        for (const Rule& rule : kSkinRules)
            for (const std::string& line : matchingLines(file, rule.pattern))
                offenders.push_back(std::string(rule.name) + ": " + line);

    INFO(join(offenders));
    CHECK(offenders.empty());
}

TEST_CASE("ui: policy: no size outside the ramp exists in either library")
{
    std::vector<std::filesystem::path> files = sourcesIn(kSrc / "Ui");
    const std::vector<std::filesystem::path> skin = sourcesIn(kSrc / "UiJuce");
    files.insert(files.end(), skin.begin(), skin.end());
    REQUIRE_FALSE(files.empty());

    std::vector<std::string> offenders;
    for (const auto& file : files)
    {
        if (file.filename() == "TypeRamp.hpp")
            continue;

        std::ifstream in(file);
        std::string line;
        int number = 0;
        while (std::getline(in, line))
        {
            ++number;
            for (const char* figure : kEmFigures)
            {
                if (line.find(figure) != std::string::npos)
                    offenders.push_back(std::string("an em figure outside the ramp table: ") +
                                        file.filename().string() + ":" + std::to_string(number) +
                                        "  " + line);
            }
        }
    }

    for (const auto& file : skin)
        for (const std::string& line : matchingLines(file, kNumericSetFont))
            offenders.push_back("a numeric setFont in the skin: " + line);

    INFO(join(offenders));
    CHECK(offenders.empty());
}

TEST_CASE("ui: policy: the shell names no colour, no colour id and no size of its own")
{
    const std::filesystem::path shell = kSrc / "Entangle";
    REQUIRE(std::filesystem::is_directory(shell));

    /* Zero, from: M10 item 03 replaced the placeholder editor, which this case had pinned at seven
       recorded sites (four colours — `Colours::red` on the thumb, `Colours::grey` on seven labels,
       `Colours::green` on the header, `Colours::orangered` on "B not connected" — and three numeric `setFont`
       heights). spec §1 rules the red and the green out of the design; the ceiling existed so the placeholder
       could not grow while `01` and `02` landed, and item 03's criterion 8 is the emptying of it.

       Two rules beyond the three the ceiling counted, because the shell has two other ways to reach past the
       palette and the theme: `findColour` asks the look and feel for a colour the design never named, and a
       `ColourId` — written on a component or read from one — is the same reach in its setter form. `03` §5 arms 5
       and 11 are the status line dyed `orangered` and `paint()` filled from `ResizableWindow::backgroundColourId`:
       both are colour *ids*, and neither matches `Colours::` or a hex literal, so without these two the arms would
       run green over the exact defect criterion 8 exists to refuse.

       What the case still cannot see is a colour computed at run time; that half is §5.22's palette scan, which
       reads the painted sheet, and the two are cited together or not at all. */
    const std::regex kFindColour(R"(findColour\s*\()");
    const std::regex kColourId(R"(ColourId)");

    const std::array<Rule, 5> kShellRules{{
        {"a Colours:: name", kColourName},
        {"a hex literal", kHexLiteral},
        {"a numeric setFont", kNumericSetFont},
        {"a findColour call", kFindColour},
        {"a ColourId", kColourId},
    }};

    std::size_t totalFound = 0;
    std::vector<std::string> offenders;
    for (const auto& file : sourcesIn(shell))
        for (const Rule& rule : kShellRules)
            for (const std::string& line : matchingLines(file, rule.pattern))
            {
                ++totalFound;
                offenders.push_back(std::string(rule.name) + " in the shell: " + line);
            }

    INFO(join(offenders));
    INFO("criterion 8: the palette is src/Ui's and the type is TypeRamp.hpp's; the shell may name neither");
    CHECK(totalFound == 0);
    CHECK(offenders.empty());
}
