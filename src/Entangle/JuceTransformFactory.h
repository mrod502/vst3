#pragma once

#include <GrainEngine.hpp>
#include <JuceFft.hpp>

#include <memory>

/** Bridges grain::TransformFactory to the one JUCE-aware transform.

    It lives here rather than in FftJuce because that would have a leaf backend link an engine
    library in order to see grain::TransformFactory, reversing the Grain-to-Fft direction.

    Stateless, and GrainConvolutionEngine only borrows it, so the shell must declare this member
    above the engine: members are destroyed in reverse declaration order, and the borrow has to
    outlive the borrower. */
class JuceTransformFactory final : public grain::TransformFactory
{
public:
    [[nodiscard]] std::unique_ptr<fft::IFft> create(int sizeF) const override
    {
        return std::make_unique<fft::JuceFft>(sizeF);
    }
};
