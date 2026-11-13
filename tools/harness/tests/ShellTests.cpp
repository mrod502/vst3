/* M5 item 01's shell-side Catch2 program: the first place outside OfflineRender that a leg takes a real processor
   from createPluginFilter(), pokes its APVTS, applies a setBusesLayout and renders through processBlock.

   Every case name starts with "harness: " because a case name is the only filter this repo has: `ctest -R
   ShellTests` selects nothing and exits 0 (M5 INDEX §3 rule 3), and the prefix list is the contract `09`'s runner
   consumes.

   The whole binary runs at the shape §5.20 runs its cells on — hop 512, L 1024, 8 192 samples — because a case
   that reached for kDefault costs 0.71 s per block plan (INDEX §8 entry 7) and this binary holds a 10 s budget for
   its entire run (`01` §0 H). Cost is recorded by `07`; nothing here gates a duration.

   `main()` owns the startup the default Catch2 main cannot: the GUI initialiser that makes this thread the message
   thread (F-018), the logger filter that keeps the shell's layout report out of the run's output for the whole run,
   the teardown order `Logger::~Logger` asserts on, and the `--dump-wavs` switch. */

#include <AllocWatch.hpp>
#include <HarnessShell.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using harness::blocksSummingTo;
using harness::configure;
using harness::constantStream;
using harness::dcOffsetStream;
using harness::expectDelayedBitExact;
using harness::expectIdentical;
using harness::FailDump;
using harness::kStereoNoB;
using harness::kStereoWithB;
using harness::kTightest;
using harness::makeProcessor;
using harness::noiseStream;
using harness::render;
using harness::renderReference;
using harness::Shape;
using harness::sineStream;
using harness::speechLikeStream;
using harness::squareStream;
using harness::chirpStream;
using harness::Stream;

/** Set once by `main()` before Catch2 runs and read-only afterwards, so the two dump legs describe one run
    rather than a race. Null unless the run asked for `--dump-wavs`. */
const juce::File *dumpDirectory = nullptr;

/** The harness expectations print `ok`/`FAIL` to stdout exactly as they always have. A probe that fails one on
    purpose would otherwise put a FAIL line in the middle of a passing case, so the probe runs with stdout
    redirected and asserts on what the expectation actually said instead. */
class QuietStdout
{
public:
    QuietStdout() : original(std::cout.rdbuf()) { std::cout.rdbuf(buffer.rdbuf()); }
    ~QuietStdout() { std::cout.rdbuf(original); }

    [[nodiscard]] std::string text() const { return buffer.str(); }

private:
    std::ostringstream buffer;
    std::streambuf *original;
};

/** True on the first difference, including a shape difference. The comparison is exact by design — the leg asks
    whether two generations from one seed are the same bytes — which is why it is here once rather than spelled
    `==` on floats at each call site. */
bool differs(const Stream &first, const Stream &second)
{
    if (first.size() != second.size())
        return true;
    for (std::size_t channel = 0; channel < first.size(); ++channel)
    {
        if (first[channel].size() != second[channel].size())
            return true;
        for (std::size_t i = 0; i < first[channel].size(); ++i)
            if (first[channel][i] != second[channel][i])
                return true;
    }
    return false;
}

struct RiffProbe
{
    bool wave = false;
    int channels = 0;
    int bitsPerSample = 0;
    int frames = 0;
};

std::uint32_t fourcc(const std::uint8_t *at)
{
    return (static_cast<std::uint32_t>(at[0]) << 24U) | (static_cast<std::uint32_t>(at[1]) << 16U)
           | (static_cast<std::uint32_t>(at[2]) << 8U) | static_cast<std::uint32_t>(at[3]);
}

std::uint32_t fourccOf(const char *text) { return fourcc(reinterpret_cast<const std::uint8_t *>(text)); }

std::uint32_t u32At(const std::uint8_t *at)
{
    return static_cast<std::uint32_t>(at[0]) | (static_cast<std::uint32_t>(at[1]) << 8U)
           | (static_cast<std::uint32_t>(at[2]) << 16U) | (static_cast<std::uint32_t>(at[3]) << 24U);
}

std::uint16_t u16At(const std::uint8_t *at)
{
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(at[0])
                                      | (static_cast<std::uint32_t>(at[1]) << 8U));
}

/** Walks the chunk list the way a reader has to: RIFF/WAVE, then fmt and data wherever the writer put them. */
RiffProbe probeWav(const juce::File &file)
{
    RiffProbe probe;
    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes))
        return probe;

    const auto *data = static_cast<const std::uint8_t *>(bytes.getData());
    const std::size_t size = bytes.getSize();
    if (size < 12 || fourcc(data) != fourccOf("RIFF") || fourcc(data + 8) != fourccOf("WAVE"))
        return probe;
    probe.wave = true;

    for (std::size_t at = 12; at + 8 <= size; at += 8 + u32At(data + at + 4) + (u32At(data + at + 4) & 1U))
    {
        const std::uint32_t chunk = u32At(data + at + 4);
        if (fourcc(data + at) == fourccOf("fmt ") && at + 24 <= size)
        {
            probe.channels = static_cast<int>(u16At(data + at + 10));
            probe.bitsPerSample = static_cast<int>(u16At(data + at + 22));
        }
        else if (fourcc(data + at) == fourccOf("data"))
            probe.frames = probe.channels * probe.bitsPerSample > 0
                               ? static_cast<int>(chunk / static_cast<std::uint32_t>(probe.channels
                                                                                    * probe.bitsPerSample / 8))
                               : -1;
    }
    return probe;
}

std::int64_t countFilesIn(const juce::File &directory)
{
    if (!directory.isDirectory())
        return 0;
    return static_cast<std::int64_t>(directory.findChildFiles(juce::File::findFiles, false).size());
}
}

TEST_CASE("harness: the shell adds nothing at the matrix shape (Shipped chain)")
{
    /* §5.1's claim, one hop away from the monolith: the processor's own render, bit for bit against the same
       chain built by entangle::makeChain with no shell in it. The shape is kTightest, which is the same {512,
       1024} pair §5.20 calls kMatrix — that name is private to the section's own table (`01` §1.6 records the
       duplication rather than fixing it), so this leg names the pair it runs on instead. */
    const Shape shape = kTightest;
    const int total = 8 * shape.chosen;
    const unsigned seedA = 20261109U;
    const unsigned seedB = 20261110U;
    CAPTURE(total, seedA, seedB);
    INFO("shape hop=" << shape.hop << " chosen=" << shape.chosen
                      << " bus=" << kStereoWithB.name << " variant=Shipped");

    auto processor = makeProcessor();
    REQUIRE(configure(*processor, kStereoWithB, shape));

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const auto a = noiseStream(channels, total, seedA, 0.4f);
    const auto b = noiseStream(sideChannels, total, seedB, 0.4f);

    const auto throughShell = render(*processor, a, &b, blocksSummingTo(total, 0));
    const auto direct = renderReference(shape, a, b, channels, sideChannels,
                                        blocksSummingTo(total, 0));

    CHECK(throughShell.size() == static_cast<std::size_t>(channels));
    CHECK(expectIdentical(throughShell, direct,
                          "harness: shell vs the Shipped chain at the matrix shape"));
}

TEST_CASE("harness: the host block plan does not reach the DSP at the matrix shape (Shipped chain)")
{
    /* §5.2's claim on the extracted drivers: two callback sequences summing to the same total are the same
       render, sample for sample. The ragged plans are the ones that also exercise the drain accounting, since
       blocksSummingTo keeps its zero-length callbacks in. */
    const Shape shape = kTightest;
    const int total = 8 * shape.chosen;
    CAPTURE(total);
    INFO("shape hop=" << shape.hop << " chosen=" << shape.chosen << " bus=" << kStereoWithB.name
                      << " variant=Shipped");

    auto processor = makeProcessor();
    REQUIRE(configure(*processor, kStereoWithB, shape));

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const auto a = noiseStream(channels, total, 5150U, 0.4f);
    const auto b = noiseStream(sideChannels, total, 5151U, 0.4f);

    const auto first = render(*processor, a, &b, blocksSummingTo(total, 0));
    const auto second = render(*processor, a, &b, blocksSummingTo(total, 1));

    CHECK(expectIdentical(first, second,
                          "harness: two block plans, one render, bit-identical"));
}

TEST_CASE("harness: with no sidechain the dry path is the main input delayed by R (Shipped chain)")
{
    /* §5.5's first claim, and the leg `01` §5 arm 1 reddens: an off-by-one in the extracted
       expectDelayedBitExact cannot pass it. R here is the shell's declaration, which EngineChain.h sets to the
       hop — the leg asserts the *measured* delay against that figure, never against the physical L (INDEX §5). */
    const Shape shape = kTightest;
    const int total = 8 * shape.chosen;
    CAPTURE(total);
    INFO("shape hop=" << shape.hop << " chosen=" << shape.chosen << " bus=" << kStereoNoB.name
                      << " variant=Shipped");

    auto processor = makeProcessor();
    REQUIRE(configure(*processor, kStereoNoB, shape));

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int latency = processor->getLatencySamples();
    CAPTURE(latency);
    const auto a = noiseStream(channels, total, 6060U, 0.4f);

    const auto dry = render(*processor, a, nullptr, blocksSummingTo(total, 0));
    CHECK(expectDelayedBitExact(dry, a, latency,
                               "harness: sidechain disabled, A delayed by exactly R"));
}

TEST_CASE("harness: every seeded generator is reproducible from its seed and moves when the seed moves")
{
    /* Both halves of criterion 6, asserted for each generator. The second half is the one a generator with a
       hard-coded seed survives, so it is not decoration (`01` §5 arm 3 reddens it by ignoring the seed).
       silenceStream, constantStream, impulseStream, singleImpulse and poisonStream take no seed: they have no
       randomness to pin, so they are not in this table. */
    struct Generator
    {
        const char *name;
        std::function<Stream(unsigned)> make;
    };

    const std::array<Generator, 6> generators{{
        Generator{"noiseStream", [](unsigned seed) { return noiseStream(2, 512, seed, 0.4f); }},
        Generator{"sineStream",
                  [](unsigned seed) { return sineStream(2, 512, seed, 440.0f, 0.5f); }},
        Generator{"chirpStream",
                  [](unsigned seed) { return chirpStream(2, 512, seed, 100.0f, 4000.0f, 0.5f); }},
        Generator{"squareStream",
                  [](unsigned seed) { return squareStream(2, 512, seed, 220.0f, 0.5f); }},
        Generator{"dcOffsetStream",
                  [](unsigned seed) { return dcOffsetStream(2, 512, seed, 0.05f, 0.2f); }},
        Generator{"speechLikeStream",
                  [](unsigned seed) { return speechLikeStream(2, 512, seed, 0.5f); }},
    }};

    constexpr unsigned firstSeed = 20261109U;
    constexpr unsigned secondSeed = 20261110U;

    for (const auto &generator : generators)
    {
        INFO("generator " << generator.name << " seeds " << firstSeed << " and " << secondSeed);
        const auto firstRun = generator.make(firstSeed);
        const auto sameSeed = generator.make(firstSeed);
        const auto otherSeed = generator.make(secondSeed);

        CHECK(firstRun.size() == 2);
        CHECK_FALSE(differs(firstRun, sameSeed));
        CHECK(differs(firstRun, otherSeed));
    }
}

TEST_CASE("harness: a failing expectation writes no WAV unless the caller asks for one")
{
    /* criterion 7's off half, and it is the default for all 200 existing OfflineRender call sites: an
       expectation that gets no FailDump fails silently, so one red run stays one red run instead of two hundred
       files. */
    const char *label = "harness: probe, deliberately unequal streams";
    const Stream got = constantStream(2, 64, 0.25f);
    const Stream want = constantStream(2, 64, -0.25f);

    const juce::File directory = juce::File::getCurrentWorkingDirectory()
                                     .getChildFile("offline-render")
                                     .getChildFile("shelltests");
    /* createDirectory() reports false for a directory that already exists, so the assertion is on the state it
       is meant to produce: "nothing appeared" has to be a measurement against a directory that is really there,
       on the second run as much as the first. */
    directory.createDirectory();
    REQUIRE(directory.isDirectory());
    const std::int64_t before = countFilesIn(directory);

    QuietStdout quiet;
    const bool held = expectIdentical(got, want, label);

    CHECK_FALSE(held);
    CHECK(quiet.text().find("FAIL harness: probe") != std::string::npos);
    CHECK(countFilesIn(directory) == before);
}

TEST_CASE("harness: a failing expectation dumps the offending stream when the run asks for it")
{
    /* criterion 7's on half, decided only by a run that passed --dump-wavs; the RIFF walk inspects the writer,
       and nothing here reads the file back as input — signals stay generated (INDEX §6 row 2). */
    if (dumpDirectory == nullptr)
    {
        SKIP("run the binary with --dump-wavs to decide the dump path");
        return;
    }

    const char *label = "harness: probe, dumped on request";
    const Stream got = constantStream(2, 1024, 0.25f);
    const Stream want = constantStream(2, 1024, -0.25f);

    const int filesBefore = countFilesIn(*dumpDirectory);

    QuietStdout quiet;
    const bool held = expectIdentical(got, want, label, FailDump{dumpDirectory});
    CHECK_FALSE(held);

    /* One file per red leg, and the leg does not re-implement the writer's naming rule to find it: it counts what
       appeared. Re-deriving the name here would let a change to the sanitiser pass by moving the file and this
       check with it. */
    const juce::Array<juce::File> written = dumpDirectory->findChildFiles(juce::File::findFiles, false);
    const std::int64_t appeared = static_cast<std::int64_t>(written.size()) - filesBefore;
    const juce::String newest = appeared > 0 ? written[written.size() - 1].getFileName()
                                             : juce::String("<none>");
    INFO("expected one new file in " << dumpDirectory->getFullPathName().toRawUTF8() << ", found " << appeared
                                     << " named " << newest.toRawUTF8());
    REQUIRE(appeared == 1);

    const RiffProbe probe = probeWav(written[written.size() - 1]);
    CHECK(probe.wave);
    CHECK(probe.channels == 2);
    CHECK(probe.bitsPerSample == 32);
    CHECK(probe.frames == 1024);
    CHECK(quiet.text().contains("wrote"));
}

TEST_CASE("harness: the allocation tripwire counts one allocation and counts none at all")
{
    /* criterion 8, both controls. The counts are read inside the armed window and compared outside it, because
       Catch2 allocating for a CHECK or an INFO while armed would otherwise be counted as the thing under test. */
    long withOneAllocation = -1;
    float readBack = 0.0f;
    {
        allocwatch::Watch armed;
        auto *samples = new float[64];
        samples[63] = 0.5f;
        readBack = samples[63];
        withOneAllocation = allocwatch::allocations;
        delete[] samples;
    }
    INFO("one new float[64] inside an armed window counted " << withOneAllocation);
    CHECK(withOneAllocation == 1);
    /* The buffer has to be written and read back. An untouched new/delete pair is elided by -O3 before the shim
       ever sees it, which counted 0 through a working replacement — a control that can never be satisfied is not
       a control, so the read-back is what makes the count mean an allocation happened. */
    CHECK(readBack > 0.25f);
    CHECK(readBack < 0.75f);

    double accumulator = 0.0;
    long withNoAllocation = -1;
    {
        allocwatch::Watch armed;
        for (int i = 1; i <= 4096; ++i)
            accumulator += 1.0 / static_cast<double>(i);
        withNoAllocation = allocwatch::allocations;
    }
    INFO("4 096 additions inside an armed window counted " << withNoAllocation);
    CHECK(withNoAllocation == 0);
    /* Same reason as above, other direction: the arithmetic has to survive the optimiser or the zero proves only
       that nothing was left to run. H(4 096) = 8.895 103 896 966. */
    CHECK(accumulator == Catch::Approx(8.895103896966).margin(1e-9));
}

namespace
{
/** `--dump-wavs` belongs to this binary, so it has to leave argv before Catch2 turns an unrecognised argument
    into a usage error and exits 2 without running a case. */
bool takeDumpWavFlag(int &argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string_view(argv[i]) != "--dump-wavs")
            continue;
        for (int next = i + 1; next < argc; ++next)
            argv[next - 1] = argv[next];
        --argc;
        argv[argc] = nullptr;
        return true;
    }
    return false;
}
}

int main(int argc, char **argv)
{
    const bool askedForWavs = takeDumpWavFlag(argc, argv);

    /* The APVTS flush timer needs a message manager, and constructing one claims this thread as the message
       thread (F-018) — which is exactly why the filter below is needed: every prepare in a leg on this thread
       writes a layoutReport() line. ScopedJuceInitialiser_GUI is OfflineRender main()'s first statement for the
       same reason (`01` §0 F). */
    juce::ScopedJuceInitialiser_GUI gui;
    harness::ReportLogFilter logFilter;
    juce::Logger::setCurrentLogger(&logFilter);

    const juce::File directory = juce::File::getCurrentWorkingDirectory()
                                     .getChildFile("offline-render")
                                     .getChildFile("shelltests");
    if (askedForWavs)
    {
        directory.createDirectory();
        dumpDirectory = &directory;
    }

    Catch::Session session;
    const int result = session.run(argc, argv);

    dumpDirectory = nullptr;
    /* Logger::~Logger asserts that it is not destroying the installed logger. Compiled out in Release, fatal in
       Debug, so the uninstall is unconditional — the trap M4 item 08 found on the way out of a run whose every
       leg was ok. */
    juce::Logger::setCurrentLogger(nullptr);
    return result;
}
