#pragma once
#include <cmath>
#include <vector>
#include "model_edge_dual.h"

int predict_rf_generic(const float* features, float* out_score,
                       const int n_features, const int n_trees,
                       const float* scale_mean, const float* scale_std,
                       const int* tree_offsets, const int* feat_idx, const float* thresholds,
                       const int* left_children, const int* right_children, const float* probs) 
{
    std::vector<float> z(n_features);
    for(int i=0; i<n_features; i++) {
        float s = scale_std[i];
        if(s < 1e-9f) s = 1.0f;
        z[i] = (features[i] - scale_mean[i]) / s;
    }

    float total_prob1 = 0;
    for(int t=0; t<n_trees; t++) {
        int idx = tree_offsets[t]; 
        while(true) {
            int left = left_children[idx];
            int right = right_children[idx];
            if(left == -1 && right == -1) {
                total_prob1 += probs[idx];
                break;
            } else {
                int f_idx = feat_idx[idx];
                float val = z[f_idx];
                if(val <= thresholds[idx]) idx = left; 
                else idx = right;
            }
        }
    }
    float avg_prob = total_prob1 / (float)n_trees;
    if(out_score) *out_score = avg_prob;
    return (avg_prob >= 0.5f) ? 1 : 0;
}

int predict_cold(const float* features, float* out_score) {
    return predict_rf_generic(features, out_score, 
        RF_COLD_N_FEATURES, RF_COLD_N_TREES, 
        RF_COLD_SCALE_MEAN, RF_COLD_SCALE_STD,
        RF_COLD_TREE_OFFSETS, RF_COLD_FEATURE, RF_COLD_THRESHOLD, 
        RF_COLD_LEFT, RF_COLD_RIGHT, RF_COLD_PROB1);
}

int predict_warm(const float* features, float* out_score) {
    return predict_rf_generic(features, out_score, 
        RF_WARM_N_FEATURES, RF_WARM_N_TREES, 
        RF_WARM_SCALE_MEAN, RF_WARM_SCALE_STD,
        RF_WARM_TREE_OFFSETS, RF_WARM_FEATURE, RF_WARM_THRESHOLD, 
        RF_WARM_LEFT, RF_WARM_RIGHT, RF_WARM_PROB1);
}