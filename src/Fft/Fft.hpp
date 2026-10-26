#pragma once

namespace fft
{
/** One real forward + one inverse transform of a fixed power-of-two size F.

    Layout and scaling are specified by this header, not by whichever engine is installed:
    bins[2k] = Re, bins[2k + 1] = Im for k in [0, F/2]; forward is unnormalised, inverse
    carries 1/F. So a bare inverse(forward(x)) == x with no factor left over, and the 1/F a
    spectral product needs is the caller's to fold into its own gain table.

    Bins [0, F/2] is the half-spectrum: bin 0 is DC and bin F/2 is Nyquist. For real input
    both are real, so an implementation writes zero into bins[1] and bins[F + 1] and nothing
    may ever read them back — a caller that leaves them unwritten cannot change a result.

    Buffers are caller-owned and never aliased to each other. Constructing an implementation
    may allocate; forward and inverse allocate nothing, take no lock, and touch nothing but
    their arguments and their own pre-built state, so they are audio-thread safe. */
class IFft
{
public:
    virtual ~IFft() = default;

    virtual int size() const noexcept = 0; ///< F

    /** F samples at @p in -> F + 2 floats at @p bins. */
    virtual void forward(const float* in, float* bins) const noexcept = 0;

    /** F + 2 floats at @p bins -> F samples at @p out. */
    virtual void inverse(const float* bins, float* out) const noexcept = 0;
};
}
