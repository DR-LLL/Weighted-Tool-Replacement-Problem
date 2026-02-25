
from pathlib import Path
import pandas as pd

# ---- Hardcoded paths ----
CANDIDATE_ROOTS = [Path(".").resolve(), Path("/mnt/data").resolve()]
OUTPUT_NAME = "combined_results.csv"
OUTPUT_GROUPED_NAME = "combined_results_grouped.csv"

def find_results_dir():
    for root in CANDIDATE_ROOTS:
        res = root / "results"
        if res.exists():
            return res
    # If none exist, create under the first root
    res = CANDIDATE_ROOTS[0] / "results"
    res.mkdir(parents=True, exist_ok=True)
    return res

def norm_cols(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df.columns = [str(c).strip() for c in df.columns]
    return df

def algo_name_from_path(path: Path, idx: int) -> str:

    stem = path.stem  
    for prefix in ("results", "result"):
        if stem.lower().startswith(prefix):  
            stem = stem[len(prefix):]
            break
    stem = stem.lstrip("_-")
    if not stem:
        stem = f"algo{idx+1}"
    return stem

def load_one_results(path: Path, algo: str) -> pd.DataFrame:
 
    df = norm_cols(pd.read_csv(path))

   
    rename_map = {}
    if "total_cost" in df.columns:
        rename_map["total_cost"] = f"total_cost_{algo}"
    if "millis" in df.columns:
        rename_map["millis"] = f"millis_{algo}" 
    if "N" in df.columns:
        rename_map["N"] = f"N_{algo}"
    if "M" in df.columns:
        rename_map["M"] = f"M_{algo}"
    if "C" in df.columns:
        rename_map["C"] = f"C_{algo}"

    df = df.rename(columns=rename_map)

    if "test" not in df.columns:
        raise ValueError(f"File {path} has no column 'test'")

    return df

def main():
    results_dir = find_results_dir()
    out_csv = results_dir / OUTPUT_NAME
    out_grouped_csv = results_dir / OUTPUT_GROUPED_NAME

    csv_paths = sorted(
        p for p in results_dir.glob("*.csv")
        if p.name.lower().startswith("result")
    )

    if not csv_paths:
        raise FileNotFoundError(
            f"No CSV in {results_dir}, started with 'result'"
        )
    if len(csv_paths) == 1:
        raise FileNotFoundError(
            f"Only one file ({csv_paths[0]}). "
            f"For merge you have to have at least two."
        )

    dfs = []
    algo_names = []
    tests_per_file = {}  

    for idx, path in enumerate(csv_paths):
        algo = algo_name_from_path(path, idx)
        algo_names.append(algo)
        print(f"[load] {path.name} -> algo='{algo}'")

        df = load_one_results(path, algo)

       
        if df["test"].duplicated().any():
            dups = df.loc[df["test"].duplicated(), "test"].astype(str).unique().tolist()
            raise ValueError(
                f"In file {path.name} (algo='{algo}') were founded 'test' with same name. "
                f"Example: {dups[:10]} (всего {len(dups)})"
            )

        dfs.append(df)
        tests_per_file[algo] = set(df["test"].tolist())

    common_tests = set.intersection(*(tests_per_file[a] for a in algo_names))
    print(f"[merge] common tests across all files = {len(common_tests)}")


    for algo in algo_names:
        missing = sorted(tests_per_file[algo] - common_tests)
        if missing:
            N_SHOW = 20
            preview = ", ".join(map(str, missing[:N_SHOW]))
            tail = "" if len(missing) <= N_SHOW else f", ... (+{len(missing) - N_SHOW})"
            print(
                f"[warn] algo='{algo}': {len(missing)} test(s) will be dropped (not present in all CSVs): "
                f"{preview}{tail}"
            )

    if not common_tests:
        raise ValueError(
            "The intersection of the tests is empty: there is no 'test' that is present in all the CSV files."
        )


    dfs = [df[df["test"].isin(common_tests)].copy() for df in dfs]

    merged = dfs[0]
    for df in dfs[1:]:
        merged = pd.merge(
            merged, df,
            on="test",
            how="inner",     
            validate="one_to_one"
        )

    print(f"[merge] rows after intersecting all tests = {len(merged)}")


    for col in ["N", "M", "C"]:
        algo_cols = [c for c in merged.columns if c.startswith(f"{col}_")]
        if not algo_cols:
            merged[col] = None
            continue
        first_col = algo_cols[0]
        merged[col] = merged[first_col]
        merged.drop(columns=algo_cols, inplace=True)

    base_cols = ["test", "N", "M", "C"]
    cost_cols = [f"total_cost_{algo}" for algo in algo_names if f"total_cost_{algo}" in merged.columns]
    time_cols = [f"millis_{algo}" for algo in algo_names if f"millis_{algo}" in merged.columns]

    cols = [c for c in base_cols + cost_cols + time_cols if c in merged.columns]
    merged = merged[cols].sort_values("test").reset_index(drop=True)

    out_csv.parent.mkdir(parents=True, exist_ok=True)
    merged.to_csv(out_csv, index=False)
    print(f"[merge] written: {out_csv}")


    if all(col in merged.columns for col in ["N", "M", "C"]):
        agg_dict = {}
        for c in merged.columns:
            if c.startswith("total_cost_") or c.startswith("millis_"):
                agg_dict[c] = "mean"
        agg_dict["test"] = "count"

        grouped = merged.groupby(["N", "M", "C"]).agg(agg_dict).reset_index()
        grouped = grouped.rename(columns={"test": "test_count"})

        total_cost_cols = [c for c in grouped.columns if c.startswith("total_cost_")]
        if total_cost_cols:
            grouped["avg_total_cost"] = grouped[total_cost_cols].mean(axis=1)
            grouped.drop(columns=total_cost_cols, inplace=True)
        else:
            grouped["avg_total_cost"] = None

        millis_cols = [c for c in grouped.columns if c.startswith("millis_")]
        rename_time = {c: f"avg_{c}" for c in millis_cols}
        grouped = grouped.rename(columns=rename_time)

        numeric_cols = ["avg_total_cost"] + list(rename_time.values())
        for col in numeric_cols:
            if col in grouped.columns:
                grouped[col] = grouped[col].round(4)

        grouped.insert(0, "group_id", range(1, len(grouped) + 1))

        avg_time_cols = [c for c in grouped.columns if c.startswith("avg_millis_")]
        final_cols = ["group_id", "N", "M", "C", "test_count", "avg_total_cost"] + sorted(avg_time_cols)
        final_cols = [c for c in final_cols if c in grouped.columns]
        grouped = grouped[final_cols]

        grouped.to_csv(out_grouped_csv, index=False)
        print(f"[merge] grouped rows={len(grouped)}")
        print(f"[merge] written: {out_grouped_csv}")
    else:
        print("[merge] Warning: Cannot create grouped version - missing N, M, or C columns")
if __name__ == "__main__":
    main()
