#pragma once
#include <cmath>
#include <vector>
#include "model_edge.h" 
#include "tsassure_settings.h"

int predict_rf(const float* features, float* out_score) {
    std::vector<float> z(TS_N_FEATURES);
    for(int i=0; i<TS_N_FEATURES; i++) {
        float s = RF_SCALE_STD[i];
        if(s < 1e-9f) s = 1.0f;
        z[i] = (features[i] - RF_SCALE_MEAN[i]) / s;
    }

    float total_prob1 = 0;
    for(int t=0; t<RF_N_TREES; t++) {
        int idx = RF_TREE_OFFSETS[t];
        while(true) {
            int left = RF_LEFT[idx];
            int right = RF_RIGHT[idx];
            if(left == -1 && right == -1) {
                total_prob1 += RF_PROB1[idx];
                break;
            } else {
                int f_idx = RF_FEATURE[idx];
                if(z[f_idx] <= RF_THRESHOLD[idx]) idx = left; 
                else idx = right;
            }
        }
    }

    float avg_prob = total_prob1 / (float)RF_N_TREES;
    if(out_score) *out_score = avg_prob;
    return (avg_prob >= 0.5f) ? 1 : 0;
}