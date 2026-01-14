#pragma once
#include <cmath>
#include <vector>
#include "model_edge_dual.h" 

int predict_svm_generic(const float* features, float* out_score,
                       const int n_features,
                       const float* scale_mean, const float* scale_std,
                       const float* coef, const float bias,
                       const float probA, const float probB) 
{
    float decision = 0.0f;
    for(int i=0; i<n_features; i++) {
        float s = scale_std[i];
        if(s < 1e-9f) s = 1.0f;
        float val_norm = (features[i] - scale_mean[i]) / s;
        decision += val_norm * coef[i];
    }
    decision += bias;
    float prob = 1.0f / (1.0f + expf(probA * decision + probB));
    if(out_score) *out_score = prob;
    return (prob >= 0.5f) ? 1 : 0;
}

int predict_cold(const float* features, float* out_score) {
    return predict_svm_generic(features, out_score, 
        SVM_COLD_N_FEATURES, 
        SVM_COLD_SCALE_MEAN, SVM_COLD_SCALE_STD,
        SVM_COLD_COEF, SVM_COLD_BIAS,
        SVM_COLD_PROB_A, SVM_COLD_PROB_B);
}

int predict_warm(const float* features, float* out_score) {
    return predict_svm_generic(features, out_score, 
        SVM_WARM_N_FEATURES, 
        SVM_WARM_SCALE_MEAN, SVM_WARM_SCALE_STD,
        SVM_WARM_COEF, SVM_WARM_BIAS,
        SVM_WARM_PROB_A, SVM_WARM_PROB_B);
}