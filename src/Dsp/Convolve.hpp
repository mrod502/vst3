#pragma once
#include <Fft.hpp>
#include <vector>
#include <complex>
#include <cmath>

namespace convolve
{
    using namespace std;
    template <typename Num>
    vector<Num> fft_convolve(vector<Num> imp, vector<Num> sig) {
        auto imp_fft = fft::fft(to_complex(imp));
        auto sig_fft = fft::fft(to_complex(sig));
        vector<complex<Num>> t_fft;
        t_fft.resize(sig_fft.size());

        transform(imp_fft.begin(), imp_fft.end(),
                sig_fft.begin(), t_fft.begin(),
                std::multiplies<complex<Num>>() );
        

    };

    template <typename Num>
   vector<complex<Num>> to_complex(vector<Num> inp){
        vector<complex<Num>> out;
        out.resize(inp.size());
        for (int i = 0;i<inp.size();i++){
            out[i] = complex(inp[i]);
        }
        return out;
    };

};
