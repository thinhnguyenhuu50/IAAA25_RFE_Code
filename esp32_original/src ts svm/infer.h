#pragma once
#include <cmath>
#include <vector>
#include "model_edge.h" 
#include "tsassure_settings.h"

int predict_svm(const float* features, float* out_score) {
    float decision = 0.0f;
    
    for(int i=0; i<SVM_N_FEATURES; i++) {
        float s = SVM_SCALE_STD[i];
        if(s < 1e-9f) s = 1.0f;
        float val_norm = (features[i] - SVM_SCALE_MEAN[i]) / s;
        decision += val_norm * SVM_COEF[i];
    }
    
    decision += SVM_BIAS;
    float prob = 1.0f / (1.0f + expf(SVM_PROB_A * decision + SVM_PROB_B));
    
    if(out_score) *out_score = prob;
    return (prob >= 0.5f) ? 1 : 0;
}