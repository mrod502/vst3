#pragma once
#include <vector>
#include <algorithm>


namespace dsp {
    template <typename num>
    void extend(vector<num>& vec,int amount){
        const int vec_size = vec.size();
        if (amount > vec.size()) return;
        vec.resize(vec_size + amount);
        for (int i = 0; i < amount ; i++){
            vec[i+ vec_size] = vec[vec_size - (i+1)];
        }
    };
}