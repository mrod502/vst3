#pragma once

#include <cstddef>
#include <vector>

/** The JUCE-free half of the shared plugin-seam harness: the stream type every leg is written against, the
    shapes and tolerances those legs quote, the seeded generators that feed them, the callback plans that decide
    how a stream reaches a processor, and the metrics that turn a stream into a figure.

    No JUCE header may appear here or in HarnessStream.cpp. M5 `05`'s generator and fuzz legs link this target
    and nothing else, so a JUCE include would stop them being console programs (AGENTS §9), and the light
    libraries they drive are JUCE-light by the same rule.

    Extracted verbatim from tools/OfflineRender.cpp by M5 item 01. What a moved helper prints is part of that
    program's byte-identical baseline, so the moved bodies say exactly what they said before; the additions are
    the five new generators and `speechLikeStream`, documented below. */
namespace harness
{
constexpr double kSampleRate = 48000.0;
constexpr int kMaxBlock = 4096;

/** The library's tolerance, unchanged: an IFFT of a spectral product leaves ~1e-8 where the exact
    result is zero and the OLA ring carries that residue onward. Against a response peak of 1.0 it
    reads as absolute. */
constexpr double kDeltaTolerance = 1e-5;

/** How far a wet-minus-dry reconstruction can be trusted: the shell sums wet into the dry stream in
    float, so recovering it costs ~2 ulp of the dry sample it was added to. */
constexpr double kWetRecoveryTolerance = 1e-6;

using Stream = std::vector<std::vector<float>>;

struct Shape
{
    int hop;
    int chosen; ///< the dial position; the running L is the shell's clamp of it
};

constexpr Shape kDefault{128, 8192};    ///< both UI defaults
constexpr Shape kTightest{512, 1024};   ///< L = 2H, the least overlap the UI can express
constexpr Shape kLongest{512, 65536};   ///< top of the set, where §4.6's tail number lives

/** Deterministic material.

    Every generator that has any randomness takes the seed as a required argument: a default seed is hidden
    global state with a pleasant signature, and a generator with one is a leg that cannot be reproduced from
    what it prints (`01` §0 D rule 1). The three that take no seed — `constantStream`, `silenceStream`,
    `poisonStream` — have no randomness to pin.

    `noiseStream`'s distribution is frozen (rule 2): ~40 hard-coded seeds in `OfflineRender` have rendered
    bytes in the byte-identity baseline, so changing this distribution is not a fix, it is a new function with
    a versioned name and a reason. */
Stream noiseStream(int channels, int samples, unsigned seed, float amplitude);
Stream constantStream(int channels, int samples, float value);
Stream impulseStream(int channels, int samples, int at);
/** The same material with one channel lit and the rest exactly zero. */
Stream singleImpulse(int channels, int samples, int channel, int at);
Stream silenceStream(int channels, int samples);

/** One sine per channel, each starting at a seeded phase, so two channels of one stream are different
    material and an independence leg cannot be satisfied by a broadcast that went wrong. */
Stream sineStream(int channels, int samples, unsigned seed, float frequencyHz, float amplitude);

/** A linear sweep from `startHz` to `endHz` across the stream, on a seeded starting phase. */
Stream chirpStream(int channels, int samples, unsigned seed, float startHz, float endHz,
                   float amplitude);

/** Bipolar square at 50 % duty — the richest harmonic content a generator here produces, which is what a
    click metric or a DC-residue leg wants to be handed. */
Stream squareStream(int channels, int samples, unsigned seed, float frequencyHz, float amplitude);

/** A DC offset with seeded noise on top of it: the offset is what the blocker must remove, the noise is
    what makes the mean of a blocked render measurable rather than exactly zero by construction. */
Stream dcOffsetStream(int channels, int samples, unsigned seed, float offset, float noiseAmplitude);

enum class Poison
{
    NaN,
    Infinity,
    Denormal
};

/** Every sample the same non-finite or denormal value. Deterministic by kind, so it needs no seed. */
Stream poisonStream(int channels, int samples, Poison kind);

/** A stand-in for a speech sample: seeded, band-shaped by four descending partials of a drifting fundamental,
    with an envelope that moves on a syllabic time scale.

    `INDEX.md` §6 row 2 substitutes this for M5 §5 item 1's "speech sample" so the repository gains neither its
    first committed binary asset nor the licence question that comes with one; a real recording would also not be
    reproducible from a printed seed, which §3 rule 8 requires of every leg that feeds a render. */
Stream speechLikeStream(int channels, int samples, unsigned seed, float amplitude);

/** Block sequences summing to exactly `total`. */
std::vector<int> blocksSummingTo(int total, int pattern);
/** A uniform callback list: `total` in `size`-sample callbacks, remainder last, no zero-length entries. */
std::vector<int> constantBlocks(int total, int size);
int totalOf(const std::vector<int> &blocks);

/** The comparison a leg reports: worst |got − want| with its address, and whether the two are bit-identical.
    §5.1's `expectIdentical` prints its own line and stops at the first difference; §5.20 reports one line per
    (cell, leg) with the worst delta in it, so the tally and the figure agree.

    Lifted out of §5.20 by M5 item 02, which is its second consumer: restating it in the cross-product binary
    would put two definitions of "identical" in the suite, which is the F-020 failure mode in a smaller hat. */
struct StreamDelta
{
    double worst = 0.0;
    std::size_t channel = 0;
    std::size_t sample = 0;
    bool shapeMismatch = false;
    bool bitIdentical = true;
};

/** One channel of `got` against one channel of `want`. A channel index past either stream is a shape
    mismatch, not an out-of-bounds read: a leg that asks for slot 1 of a mono render is wrong about the
    layout and must be told so rather than compared against nothing. */
StreamDelta compareChannel(const Stream &got, std::size_t gotChannel, const Stream &want,
                           std::size_t wantChannel);
StreamDelta compareStreams(const Stream &got, const Stream &want);

/** The largest |sample| on one channel — reported per channel because a leak in one output is the
    finding, and a peak over the whole render would hide which one. */
double channelPeak(const Stream &stream, std::size_t channel);

/** Every sample exactly 0.0f. An FFT of an all-zero hop is exactly zero, so are its product with a
    spectrum and its inverse; the DC blocker's state stays zero on zero input from zero initial state, and
    tanh(0) = 0. A non-zero here is not rounding, it is a leak — which is why the ladder puts this rung
    below bit-exactness rather than beside it, and why it is scoped to a level stage parked at amount 0. */
bool channelIsExactSilence(const Stream &stream, std::size_t channel);

/** The dry-path model: `in` delayed by `latency`, zeros before it — the arithmetic of a bit-exact delay
    check without the reporting, so a caller can tally its own lines. */
Stream delayedBy(const Stream &in, int latency);

/** The same samples times `scale`. The scale lands on the *output* here and on the *input* in the
    scheduler, and the rounding difference between those two is exactly what `kDeltaTolerance` is for. */
Stream scaledCopy(const Stream &in, float scale);

/** `HopScheduler::foldSidechain`'s arithmetic restated: ascending source order, the reciprocal applied
    once, after the sum. The fold has to land on this, and `bitIdentical` is the figure that says whether
    it does. */
Stream meanWithOneReciprocal(const Stream &side, float scale);

/** The DC component of a render: the largest |mean| over the last `window` samples of any channel. */
double dcComponentOf(const Stream &stream, std::size_t window);
/** The level a settled stream is actually held at: RMS over the last `window` samples of each channel. */
std::vector<double> settledRms(const Stream &stream, std::size_t window);
/** How much of a render the clipper is actually shaping. */
double fractionAboveKnee(const Stream &stream);
/** max |x[n] - 2x[n-1] + x[n-2]| over every channel — the primary click ratio. */
double secondDifferencePeak(const Stream &stream);
/** Where a render first clears `threshold`, or `size_t`'s max when it never does. */
std::size_t arrivalIndex(const Stream &stream, double threshold);
double peakOf(const Stream &stream);
/** Level of a ratio, for the legs that have to quote a reshaping in the unit the decision log uses. */
double decibels(double ratio);
double hannAt(int i, int length) noexcept;
}
