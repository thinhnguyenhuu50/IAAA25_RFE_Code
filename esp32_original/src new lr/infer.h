#pragma once
#include <cmath>
#include <vector>
#include "model_edge_dual.h" 

inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

int predict_lr_generic(const float* features, float* out_score,
                       const int n_features,
                       const float* scale_mean, const float* scale_std,
                       const float* coef, const float bias) 
{
    float decision = 0.0f;
    for(int i=0; i<n_features; i++) {
        float s = scale_std[i];
        if(s < 1e-9f) s = 1.0f; 
        float val_norm = (features[i] - scale_mean[i]) / s;
        decision += val_norm * coef[i];
    }
    decision += bias;
    float prob = sigmoid(decision);
    if(out_score) *out_score = prob;
    return (prob >= 0.5f) ? 1 : 0;
}

int predict_cold(const float* features, float* out_score) {
    return predict_lr_generic(features, out_score, 
        LR_COLD_N_FEATURES, 
        LR_COLD_SCALE_MEAN, LR_COLD_SCALE_STD,
        LR_COLD_COEF, LR_COLD_BIAS);
}

int predict_warm(const float* features, float* out_score) {
    return predict_lr_generic(features, out_score, 
        LR_WARM_N_FEATURES, 
        LR_WARM_SCALE_MEAN, LR_WARM_SCALE_STD,
        LR_WARM_COEF, LR_WARM_BIAS);
}