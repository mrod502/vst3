#pragma once
#include <vector>
#include <algorithm>

namespace dsp
{
    using namespace std;

    template <typename num>
    void fade(vector<num> *a, vector<num> *b, int32_t overlap)
    {
        int a_size = a->size();
        for (int32_t ix = 0; ix < overlap; ix++)
        {
            num coeff = num(1 + ix) / num(overlap + 1);
            b[ix] = coeff * b[ix] + (num(1) - coeff) * a[a_size - (overlap- ix)];
        }
        a->resize(a_size- overlap);
    }
}