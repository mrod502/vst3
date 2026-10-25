#pragma once

namespace engine
{
/** Everything an Engine needs to size itself, decided off the audio thread. */
struct EngineConfig
{
    double sampleRate = 0.0;
    int hop = 0;            ///< H, the internal step in samples.
    int maxChannels = 0;    ///< Channel capacity the Engine must be ready for.
    int latencySamples = 0; ///< Declared delay; see D-004.
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
    virtual int latencySamples() const = 0;

    /** @param a  @p numChannels pointers of config.hop samples.
        @param b  @p numChannels pointers of config.hop samples; silence where the
                  sidechain has no channel (D-002).
        @param out @p numChannels pointers of config.hop samples, not aliased with a or b.
    */
    virtual void processHop(const float* const* a, const float* const* b,
                            float* const* out, int numChannels) = 0;
};
}
