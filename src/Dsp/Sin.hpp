#include <vector>
#include <cstdint>
#include "math.h"

namespace dsp {
    template <typename Num>
    std::vector<Num> sin(uint64_t samples, Num sample_rate, Num freq, Num phase){
        std::vector<Num> vec;
        vec.resize(samples);
        for (uint64_t i = 0; i < samples; i++){
            vec[i] = std::sin(((2.0* M_PI * freq * i) + phase) / sample_rate);
        }
        return vec;
    }
}