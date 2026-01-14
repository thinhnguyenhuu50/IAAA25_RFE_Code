#pragma once
#include <cmath>
#include "model_edge.h"
#include "hjorth_settings.h"

void scale_features(const float* input, float* output) {
    for(int i=0; i<RF_N_FEATURES; i++) {
        float s = RF_SCALE_STD[i];
        if(s < 1e-9f) s = 1.0f;
        output[i] = (input[i] - RF_SCALE_MEAN[i]) / s;
    }
}

int predict_rf(const float* raw_features, float* out_score) {
    float features[RF_N_FEATURES];
    scale_features(raw_features, features);

    float sum_prob = 0.0f;

    for(int t=0; t<RF_NUM_TREES; t++) {
        int node_idx = RF_TREE_ROOTS[t];

        while(RF_LEFT[node_idx] != -1) {
            int feat_idx = RF_FEATURE[node_idx];
            float threshold = RF_THRESHOLD[node_idx];

            if (features[feat_idx] <= threshold) {
                node_idx = RF_LEFT[node_idx];
            } else {
                node_idx = RF_RIGHT[node_idx];
            }
        }
        sum_prob += RF_VALUE[node_idx];
    }

    float avg_prob = sum_prob / (float)RF_NUM_TREES;
    if(out_score) *out_score = avg_prob;
    return (avg_prob >= 0.5f) ? 1 : 0;
}