#pragma once

#include "Engine.hpp"

namespace engine
{
/** Passes A through unchanged and ignores B.

    The whole latency of this engine is the scheduler's output pre-fill (D-004): processHop
    is a bit-exact copy, and the latencySamples zeros HopScheduler writes into the output
    ring before the first callback make the end-to-end stream exactly A delayed by
    latencySamples() — for any latency, including one that is not a multiple of the hop.
*/
class PassthroughEngine final : public Engine
{
public:
    void prepare(const EngineConfig& config) override { engineConfig = config; }

    void reset() override {}

    int latencySamples() const override { return engineConfig.latencySamples; }

    void processHop(const float* const* a, const float* const*,
                    float* const* out, int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < engineConfig.hop; ++i)
                out[channel][i] = a[channel][i];
    }

private:
    EngineConfig engineConfig;
};
}
