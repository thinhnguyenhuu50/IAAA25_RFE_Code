#pragma once
#include <cmath>
#include "model_edge.h"
#include "hjorth_settings.h"

int predict_svm(const float* raw_features, float* out_score) {
    float decision = 0.0f;

    // Linear Kernel
    for(int i=0; i<SVM_N_FEATURES; i++) {
        float s = SVM_SCALE_STD[i];
        if(s < 1e-9f) s = 1.0f;
        float val_norm = (raw_features[i] - SVM_SCALE_MEAN[i]) / s;
        decision += val_norm * SVM_COEF[i];
    }
    
    decision += SVM_BIAS;
    
    // Score is distance
    if(out_score) *out_score = decision;
    
    // Class 1 if >= 0
    return (decision >= 0.0f) ? 1 : 0;
}