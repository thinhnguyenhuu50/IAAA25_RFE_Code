#pragma once
#include <cmath>
#include <vector>
#include "model_edge.h" 
#include "hjorth_settings.h"

inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

int predict_lr(const float* features, float* out_score) {
    float decision = 0.0f;
    
    for(int i=0; i<LR_N_FEATURES; i++) {
        float s = LR_SCALE_STD[i];
        if(s < 1e-9f) s = 1.0f;
        
        float val_norm = (features[i] - LR_SCALE_MEAN[i]) / s;
        decision += val_norm * LR_COEF[i];
    }
    
    decision += LR_BIAS;
    
    float prob = sigmoid(decision);
    if(out_score) *out_score = prob;
    return (prob >= 0.5f) ? 1 : 0;
}