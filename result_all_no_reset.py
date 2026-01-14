# result.py  (pos_label = 0 => 0 = fault, 1 = normal)
# Print results for ALL experiments (Class 0 and Class 1 metrics).

from pathlib import Path
import re
import pandas as pd
import numpy as np
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, confusion_matrix

# =========================
# Config (NO RESET)
# =========================
DETECTIONS_DIR = Path("output")           # folder contains: detections {method} {model}.csv
FILE_GLOB = "detections *.csv"

# ground truth is inside the same folder
REAL_LABEL_PATH = DETECTIONS_DIR / "real_label.csv"

# If detections lacks Real_Label, attach from REAL_LABEL_PATH and (optionally) write back to detections
WRITE_BACK_REAL_LABEL = True

# =========================
# Helpers
# =========================
def find_col(df, keywords):
    cols = {c.lower(): c for c in df.columns}
    for k in keywords:
        k = k.lower()
        for lc, orig in cols.items():
            if k in lc:
                return orig
    return None

def coerce_binary(s):
    x = pd.to_numeric(s, errors="coerce").fillna(0).astype(int)
    return (x != 0).astype(int)

def lat_stats(series: pd.Series):
    if series is None:
        return {"mean": np.nan, "p50": np.nan, "p95": np.nan}
    s = pd.to_numeric(series, errors="coerce").dropna()
    if s.empty:
        return {"mean": np.nan, "p50": np.nan, "p95": np.nan}
    return {"mean": float(s.mean()), "p50": float(s.median()), "p95": float(np.percentile(s, 95))}

def parse_method_model_from_name(path: Path):
    name = path.name.lower()
    m = re.match(r"^detections (.+?) (lr|rf|svm)\.csv$", name)
    if not m:
        return None, None
    return m.group(1).strip(), m.group(2).strip()

def ensure_real_label(df: pd.DataFrame, detections_path: Path) -> tuple[pd.DataFrame, str]:
    df.columns = [c.strip() for c in df.columns]
    if "Real_Label" in df.columns:
        return df, str(detections_path)

    if not REAL_LABEL_PATH.exists():
        raise SystemExit(
            f"❌ {detections_path.name}: missing 'Real_Label' and cannot find {REAL_LABEL_PATH.resolve()}"
        )

    df_real = pd.read_csv(REAL_LABEL_PATH, encoding="utf-8-sig")
    df_real.columns = [c.strip() for c in df_real.columns]

    real_col = "Real_Label" if "Real_Label" in df_real.columns else find_col(
        df_real, ["real_label", "gt", "ground truth", "y_true"]
    )
    if real_col is None:
        raise SystemExit(
            f"❌ {REAL_LABEL_PATH.name}: cannot find ground-truth column. Columns={list(df_real.columns)}"
        )

    if len(df_real[real_col]) < len(df):
        raise SystemExit(
            f"❌ {detections_path.name}: rows in real_label.csv ({len(df_real)}) < detections ({len(df)})"
        )

    df["Real_Label"] = df_real[real_col].iloc[:len(df)].to_list()

    if WRITE_BACK_REAL_LABEL:
        df.to_csv(detections_path, index=False, encoding="utf-8-sig")
        return df, str(REAL_LABEL_PATH) + " (merged & written back)"
    return df, str(REAL_LABEL_PATH) + " (merged)"

def evaluate_one_file(detections_path: Path) -> dict:
    df = pd.read_csv(detections_path, encoding="utf-8-sig")
    df, real_label_source = ensure_real_label(df, detections_path)

    pred_col = "Label" if "Label" in df.columns else find_col(df, ["label", "pred"])
    true_col = "Real_Label" if "Real_Label" in df.columns else find_col(
        df, ["real_label", "gt", "ground truth", "y_true"]
    )
    if pred_col is None or true_col is None:
        raise SystemExit(
            f"❌ {detections_path.name}: need 'Label' and 'Real_Label'. Found={list(df.columns)}"
        )

    feat_col_name = find_col(df, ["feature extraction time"])
    test_col_name = find_col(df, ["testing time"])

    # Drop warm-up rows: Label == -1
    df_work = df.copy()
    dropped_warmup = 0
    try:
        mask_warm = pd.to_numeric(df_work[pred_col], errors="coerce") == -1
        dropped_warmup = int(mask_warm.sum())
        df_work = df_work.loc[~mask_warm].copy()
    except Exception:
        pass

    method, model = parse_method_model_from_name(detections_path)

    if df_work.empty:
        return {
            "file": detections_path.name,
            "method": method, "model": model,
            "samples": 0,
            "accuracy": np.nan, 
            "f1_pos0": np.nan, "prec_pos0": np.nan, "rec_pos0": np.nan,
            "f1_pos1": np.nan, "prec_pos1": np.nan, "rec_pos1": np.nan,
            "feat_mean_ms": np.nan, "test_mean_ms": np.nan,
        }

    y_pred = coerce_binary(df_work[pred_col])
    y_true = coerce_binary(df_work[true_col])

    acc = accuracy_score(y_true, y_pred)
    
    # --- CLASS 0 (Fault) Metrics ---
    prec0 = precision_score(y_true, y_pred, pos_label=0, zero_division=0)
    rec0 = recall_score(y_true, y_pred, pos_label=0, zero_division=0)
    f10 = f1_score(y_true, y_pred, pos_label=0, zero_division=0)

    # --- CLASS 1 (Normal) Metrics ---
    prec1 = precision_score(y_true, y_pred, pos_label=1, zero_division=0)
    rec1 = recall_score(y_true, y_pred, pos_label=1, zero_division=0)
    f11 = f1_score(y_true, y_pred, pos_label=1, zero_division=0)

    # Confusion Matrix (Labels: 0=Fault, 1=Normal)
    # [TP0  FN0]
    # [FP0  TN0]
    cm = confusion_matrix(y_true, y_pred, labels=[0, 1])
    TP0, FN0, FP0, TN0 = int(cm[0, 0]), int(cm[0, 1]), int(cm[1, 0]), int(cm[1, 1])

    feat_stats = lat_stats(df_work[feat_col_name] if feat_col_name else None)
    test_stats = lat_stats(df_work[test_col_name] if test_col_name else None)

    return {
        "file": detections_path.name,
        "method": method, "model": model,
        "samples": len(df_work),
        "accuracy": acc,
        # Class 0 (Fault)
        "f1_pos0": f10, "prec_pos0": prec0, "rec_pos0": rec0,
        # Class 1 (Normal)
        "f1_pos1": f11, "prec_pos1": prec1, "rec_pos1": rec1,
        # Latency
        "feat_mean_ms": feat_stats["mean"],
        "test_mean_ms": test_stats["mean"],
        # Confusion Matrix Raw Counts
        "TP0": TP0, "FP0": FP0, "FN0": FN0, "TN0": TN0,
    }

# =========================
# Main
# =========================
def main():
    if not DETECTIONS_DIR.exists():
        raise SystemExit(f"❌ Folder not found: {DETECTIONS_DIR.resolve()}")

    files = sorted(DETECTIONS_DIR.glob(FILE_GLOB))

    # exclude real_label.csv itself
    files = [p for p in files if p.name.lower() != "real_label.csv"]

    if not files:
        raise SystemExit(f"❌ No detections files found in {DETECTIONS_DIR.resolve()}")

    rows = [evaluate_one_file(p) for p in files]
    df_all = pd.DataFrame(rows)

    # Sort nicely by method and model
    sort_cols = [c for c in ["method", "model", "file"] if c in df_all.columns]
    if sort_cols:
        df_all = df_all.sort_values(sort_cols, kind="stable")

    # Define columns to PRINT (removed 'dropped_warmup' and 'file')
    show_cols = [
        "method", "model", "samples",
        "accuracy",
        "f1_pos0", "prec_pos0", "rec_pos0",  # Fault metrics
        "f1_pos1", "prec_pos1", "rec_pos1",  # Normal metrics
        "feat_mean_ms", "test_mean_ms",
        "TP0", "FP0", "FN0", "TN0",
    ]
    
    # Filter columns that actually exist
    show_cols = [c for c in show_cols if c in df_all.columns]

    print("\n=== ALL EXPERIMENTS (Pos0=FAULT, Pos1=NORMAL) ===")
    print(df_all[show_cols].to_string(index=False))

if __name__ == "__main__":
    main()