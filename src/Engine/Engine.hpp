#pragma once

namespace engine
{
/** Everything an Engine needs to size itself, decided off the audio thread. */
struct EngineConfig
{
    double sampleRate = 0.0;
    int hop = 0;            ///< H, the internal step in samples.
    int maxChannels = 0;    ///< Channel capacity the Engine must be ready for.
    int latencySamples = 0; ///< Declared delay R; see D-004.
    int grainLength = 0;    ///< L, the analysis window length. 0 where the engine has none.
};

/** Processing step driven by the scheduler at a fixed hop.

    Contract (M1 §5): processHop is allocation-free, deterministic, and knows nothing about
    the host's callback size. It consumes exactly one hop of A and B and produces exactly
    one hop of output, so it must not add delay beyond what latencySamples() declares —
    HopScheduler realises the declared latency by pre-filling the output ring (D-004).
*/
class Engine
{
public:
    virtual ~Engine() = default;

    virtual void prepare(const EngineConfig&) = 0;
    virtual void reset() = 0;

    /** The delay declared to the host (R), which the host compensates for by delaying every
        other track. A declaration, not a measurement: what the engine physically takes is
        wetDelaySamples(). */
    virtual int latencySamples() const = 0;

    /** Physical delay L between an input event and the wet response it causes.

        Separate from latencySamples() because a convolution-style engine can declare R = 0 to
        the host while its response still arrives L samples later — that delay is part of the
        sound, like a reverb pre-delay, not a latency the host must compensate. Diagnostic and
        test use: M3's dry-path delay follows latencySamples(), never this. Defaults to the
        declared latency for engines with no separate wet path. */
    virtual int wetDelaySamples() const { return latencySamples(); }

    /** @param a  @p numChannels pointers of config.hop samples.
        @param b  @p numChannels pointers of config.hop samples; silence where the
                  sidechain has no channel (D-002).
        @param out @p numChannels pointers of config.hop samples, not aliased with a or b.
    */
    virtual void processHop(const float* const* a, const float* const* b,
                            float* const* out, int numChannels) = 0;
};
}
