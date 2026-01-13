import pandas as pd
import numpy as np
import warnings
import os
import random
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import StandardScaler
from imblearn.over_sampling import SMOTE

warnings.filterwarnings("ignore")

# ================= CONFIGURATION =================
SEED = 42
np.random.seed(SEED)
random.seed(SEED)

TRAIN_FILES = [
    "./content/dataset/Train-set_1.xlsx",
    "./content/dataset/Train-set_2.xlsx"
]

ROLLING_WINDOWS = [5, 15]
NUM_LAGS = 3
WARMUP_PERIOD = 15

# ================= FEATURE EXTRACTORS =================
class ColdFeatureExtractor:
    def __init__(self, df, time_col="Time"):
        self.df = df.copy()
        if time_col in self.df.columns:
            self.df[time_col] = pd.to_datetime(self.df[time_col], errors='coerce')
        self.numeric_cols = [c for c in self.df.select_dtypes(include=np.number).columns if 'Label' not in c]

    def extract(self):
        out = pd.DataFrame(index=self.df.index)
        df_num = self.df[self.numeric_cols]
        for c in self.numeric_cols: out[c] = df_num[c]
        if len(self.numeric_cols) >= 2:
            for i, c1 in enumerate(self.numeric_cols):
                for c2 in self.numeric_cols[i+1:]:
                    out[f'inter_{c1}_x_{c2}'] = df_num[c1] * df_num[c2]
        return out.fillna(0)

class WarmFeatureExtractor:
    def __init__(self, df, rolling_windows, num_lags):
        self.df = df.copy()
        self.rolling_windows = rolling_windows
        self.num_lags = num_lags
        self.numeric_cols = [c for c in self.df.select_dtypes(include=np.number).columns if 'Label' not in c]

    def extract(self):
        out = pd.DataFrame(index=self.df.index)
        df_num = self.df[self.numeric_cols]
        for c in self.numeric_cols: out[c] = df_num[c]
        df_speed = df_num.diff()
        for c in self.numeric_cols: out[f'speed_change_{c}'] = df_speed[c]
        stats_list = ['mean', 'median', 'std', 'var', 'min', 'max']
        for w in self.rolling_windows:
            for c in self.numeric_cols:
                r = df_num[c].rolling(window=w, min_periods=1)
                for s in stats_list:
                    try: out[f'rolling_{s}_{w}_{c}'] = getattr(r, s)().fillna(0)
                    except: pass
        for w in self.rolling_windows:
            for c in self.numeric_cols:
                r = df_speed[c].rolling(window=w, min_periods=1)
                for s in ['mean', 'std']:
                    out[f'rolling_{s}_{w}_speed_change_{c}'] = getattr(r, s)().fillna(0)
        for i in range(1, self.num_lags + 1):
            for c in self.numeric_cols: out[f'lag_{i}_{c}'] = df_num[c].shift(i)
        span = self.rolling_windows[0]
        for c in self.numeric_cols: out[f'ewma_{span}_{c}'] = df_num[c].ewm(span=span).mean()
        if len(self.numeric_cols) >= 2:
            for i, c1 in enumerate(self.numeric_cols):
                for c2 in self.numeric_cols[i+1:]:
                    out[f'inter_{c1}_x_{c2}'] = df_num[c1] * df_num[c2]
        return out

# ================= C++ GENERATORS =================
def generate_specs_code(feature_names, raw_cols, array_name):
    lines = []
    stat_map = {'mean':0, 'median':1, 'std':2, 'var':3, 'min':4, 'max':5}
    for feat in feature_names:
        kind, stat, win, lag, ch1, ch2 = "FEAT_UNKNOWN", -1, 0, 0, -1, -1
        if feat in raw_cols:
            kind, ch1 = "FEAT_RAW", raw_cols.index(feat)
        elif feat.startswith("inter_"):
            kind = "FEAT_INTER"
            parts = feat[6:].split('_x_')
            if len(parts)==2 and parts[0] in raw_cols and parts[1] in raw_cols:
                ch1, ch2 = raw_cols.index(parts[0]), raw_cols.index(parts[1])
        elif feat.startswith("speed_change_"):
            kind = "FEAT_DIFF"
            col = feat.replace("speed_change_", "")
            if col in raw_cols: ch1 = raw_cols.index(col)
        elif "speed_change" in feat and "rolling_" in feat:
            kind = "FEAT_ROLL_DIFF"
            parts = feat.split('_')
            stat, win, col = stat_map.get(parts[1], -1), int(parts[2]), "_".join(parts[5:])
            if col in raw_cols: ch1 = raw_cols.index(col)
        elif feat.startswith("rolling_"):
            kind = "FEAT_ROLL_RAW"
            parts = feat.split('_')
            stat, win, col = stat_map.get(parts[1], -1), int(parts[2]), "_".join(parts[3:])
            if col in raw_cols: ch1 = raw_cols.index(col)
        elif feat.startswith("lag_"):
            kind = "FEAT_LAG"
            parts = feat.split('_')
            lag, col = int(parts[1]), "_".join(parts[2:])
            if col in raw_cols: ch1 = raw_cols.index(col)
        elif feat.startswith("ewma_"):
            kind = "FEAT_EWMA"
            parts = feat.split('_')
            win, col = int(parts[1]), "_".join(parts[2:])
            if col in raw_cols: ch1 = raw_cols.index(col)
        lines.append(f"  {{ {kind}, {stat}, {win}, {lag}, {ch1}, {ch2} }}, // {feat}")
    return f"static const FeatureSpec {array_name}[] = {{\n" + "\n".join(lines) + "\n};\n"

def export_rf_model(rf, scaler, prefix, filename):
    all_left, all_right, all_feat, all_thresh, all_prob1 = [], [], [], [], []
    offsets = [0]
    curr_offset = 0

    for est in rf.estimators_:
        tree = est.tree_
        for i in range(tree.node_count):
            is_leaf = (tree.children_left[i] == tree.children_right[i])
            all_thresh.append(tree.threshold[i])
            all_feat.append(tree.feature[i])
            if is_leaf:
                all_left.append(-1); all_right.append(-1)
                v = tree.value[i][0]
                all_prob1.append(v[1] / v.sum() if v.sum() > 0 else 0)
            else:
                all_left.append(tree.children_left[i] + curr_offset)
                all_right.append(tree.children_right[i] + curr_offset)
                all_prob1.append(0)
        curr_offset += tree.node_count
        offsets.append(curr_offset)

    with open(filename, "a") as f:
        f.write(f"\n// ===== MODEL: {prefix} =====\n")
        f.write(f"#define {prefix}_N_FEATURES {scaler.n_features_in_}\n")
        f.write(f"#define {prefix}_N_TREES {len(rf.estimators_)}\n")

        f.write(f"static const float {prefix}_SCALE_MEAN[] = {{ {', '.join(f'{x:.6f}' for x in scaler.mean_)} }};\n")
        f.write(f"static const float {prefix}_SCALE_STD[]  = {{ {', '.join(f'{x:.6f}' for x in scaler.scale_)} }};\n")

        def w(n, t, d): f.write(f"static const {t} {prefix}_{n}[] = {{ {', '.join(str(x) for x in d)} }};\n")
        w("TREE_OFFSETS", "int", offsets)
        w("FEATURE", "int", all_feat)
        w("THRESHOLD", "float", all_thresh)
        w("LEFT", "int", all_left)
        w("RIGHT", "int", all_right)
        w("PROB1", "float", all_prob1)

# ================= MAIN =================
def main():
    print("1. Loading Data...")
    dfs = []
    for f in TRAIN_FILES:
        try:
            d = pd.read_excel(f)
            d['Label'] = pd.to_numeric(d.get('Label', 1), errors='coerce').fillna(1)
            dfs.append(d)
        except: pass

    if not dfs:
        print("Error: No data loaded. Check file paths!")
        return

    df_all = pd.concat(dfs, ignore_index=True)
    raw_cols = [c for c in df_all.select_dtypes(include=np.number).columns if c != 'Label']
    
    sm = SMOTE(random_state=SEED)

    # --- TRAIN COLD MODEL ---
    print("2. Training COLD Model (with SMOTE)...")
    cold_ext = ColdFeatureExtractor(df_all)
    X_cold = cold_ext.extract()
    y_cold = df_all['Label']
    
    X_cold_res, y_cold_res = sm.fit_resample(X_cold, y_cold)
    scaler_cold = StandardScaler()
    X_cold_s = scaler_cold.fit_transform(X_cold_res)
    
    # n_jobs=1 ensures deterministic behavior
    rf_cold = RandomForestClassifier(n_estimators=10, max_depth=6, random_state=SEED, n_jobs=1)
    rf_cold.fit(X_cold_s, y_cold_res)

    # --- TRAIN WARM MODEL ---
    print("3. Training WARM Model (with SMOTE)...")
    warm_ext = WarmFeatureExtractor(df_all, ROLLING_WINDOWS, NUM_LAGS)
    X_warm = warm_ext.extract()
    mask = ~X_warm.isna().any(axis=1)
    mask.iloc[:WARMUP_PERIOD] = False
    X_warm_clean = X_warm[mask]
    y_warm_clean = df_all['Label'][mask]

    X_warm_res, y_warm_res = sm.fit_resample(X_warm_clean, y_warm_clean)
    scaler_warm = StandardScaler()
    X_warm_s = scaler_warm.fit_transform(X_warm_res)
    
    # n_jobs=1 ensures deterministic behavior
    rf_warm = RandomForestClassifier(n_estimators=20, max_depth=8, random_state=SEED, n_jobs=1)
    rf_warm.fit(X_warm_s, y_warm_res)

    # --- EXPORT ---
    print("4. Writing headers...")
    if os.path.exists("rfe_settings.h"): os.remove("rfe_settings.h")
    if os.path.exists("model_edge_dual.h"): os.remove("model_edge_dual.h")

    with open("rfe_settings.h", "w") as f:
        f.write("#pragma once\n\n")
        for i, c in enumerate(raw_cols): f.write(f"#define IDX_{c.upper().replace(' ','_')} {i}\n")
        f.write(f"#define NUM_RAW_INPUTS {len(raw_cols)}\n")
        f.write(f"#define WARMUP_PERIOD {WARMUP_PERIOD}\n")
        f.write(f"#define N_FEATURES_COLD {X_cold.shape[1]}\n")
        f.write(f"#define N_FEATURES_WARM {X_warm.shape[1]}\n\n")
        f.write("enum FeatureKind { FEAT_RAW, FEAT_DIFF, FEAT_ROLL_RAW, FEAT_ROLL_DIFF, FEAT_LAG, FEAT_EWMA, FEAT_INTER, FEAT_ROLL_TD, FEAT_UNKNOWN };\n")
        f.write("typedef struct { FeatureKind kind; int stat; int window; int lag; int channel1; int channel2; } FeatureSpec;\n\n")
        f.write(generate_specs_code(X_cold.columns, raw_cols, "FEATURE_SPECS_COLD"))
        f.write(generate_specs_code(X_warm.columns, raw_cols, "FEATURE_SPECS_WARM"))

    with open("model_edge_dual.h", "w") as f:
        f.write("#pragma once\n#include <stdint.h>\n")
    export_rf_model(rf_cold, scaler_cold, "RF_COLD", "model_edge_dual.h")
    export_rf_model(rf_warm, scaler_warm, "RF_WARM", "model_edge_dual.h")

    print("Done. Copy 'rfe_settings.h' and 'model_edge_dual.h' to your ESP32/src folder.")

if __name__ == "__main__":
    main()