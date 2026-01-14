#pragma once
#include <cmath>
#include "model_edge.h"
#include "catch22_settings.h"

int predict_svm(const float* raw_features, float* out_score) {
    float decision = 0.0f;

    // Linear Kernel: Dot product of scaled features and coefficients
    // SVM_N_FEATURES comes from model_edge.h
    for(int i=0; i<SVM_N_FEATURES; i++) {
        // 1. Scale
        float s = SVM_SCALE_STD[i];
        if(s < 1e-9f) s = 1.0f;
        float val_norm = (raw_features[i] - SVM_SCALE_MEAN[i]) / s;
        
        // 2. Accumulate (Weight * Value)
        decision += val_norm * SVM_COEF[i];
    }
    
    // 3. Add Bias
    decision += SVM_BIAS;
    
    // Output score
    if(out_score) *out_score = decision;
    
    // Decision Boundary is 0.0 for SVM
    return (decision >= 0.0f) ? 1 : 0;
}