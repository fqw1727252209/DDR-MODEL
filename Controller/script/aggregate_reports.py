#!/usr/bin/env python3
"""
Aggregate all Statisic_report.csv files under Output_Dir into a single summary CSV.

Directory structure expected:
    Output_Dir/
        <Config>/
            <TestCase>/
                Statisic_report.csv

Output format (transposed wide-form):
    Rows are grouped by Config -> TestCase -> PC.
    Columns are Metrics in original order, followed by diff_* and diff_pct_* columns.
    This layout allows Config and TestCase columns to be merged in spreadsheet software.
"""

import os
import csv
from collections import defaultdict

# Root directory to scan for Statisic_report.csv files
OUTPUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "Output_Dir"
)

# Output summary file path
SUMMARY_FILENAME = "summary_report.csv"

def find_report_files(root_dir):
    """
    Walk root_dir and find all Statisic_report.csv files.

    Returns:
        List of tuples: (config_name, testcase_name, file_path)
    """
    reports = []
    for dirpath, _dirnames, filenames in os.walk(root_dir):
        if "Statisic_report.csv" not in filenames:
            continue
        rel_path = os.path.relpath(dirpath, root_dir)
        parts = rel_path.split(os.sep)
        if len(parts) < 2:
            continue
        config = parts[0]
        testcase = parts[1]
        file_path = os.path.join(dirpath, "Statisic_report.csv")
        reports.append((config, testcase, file_path))
    return reports

def read_report(file_path):
    """
    Read a single Statisic_report.csv.

    Returns:
        Tuple of (data_dict, metric_order_list)
        data_dict maps (metric, pc) -> value
        metric_order_list preserves the original row order of metrics
    """
    data = {}
    metric_order = []
    with open(file_path, "r", newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header or len(header) < 2:
            return data, metric_order
        pc_columns = header[1:]
        for row in reader:
            if not row or not row[0]:
                continue
            metric = row[0].strip()
            metric_order.append(metric)
            for i, pc in enumerate(pc_columns):
                val_idx = i + 1
                if val_idx >= len(row):
                    continue
                val_str = row[val_idx].strip()
                if val_str == "":
                    data[(metric, pc)] = ""
                    continue
                try:
                    data[(metric, pc)] = float(val_str)
                except ValueError:
                    data[(metric, pc)] = val_str
    return data, metric_order

def main():
    reports = find_report_files(OUTPUT_DIR)
    if not reports:
        print(f"No Statisic_report.csv found under: {OUTPUT_DIR}")
        return

    # data[(metric, testcase, pc)][config] = value
    data = defaultdict(dict)
    configs = set()
    metric_order = None

    for config, testcase, file_path in reports:
        configs.add(config)
        report_data, file_metric_order = read_report(file_path)
        if metric_order is None:
            metric_order = file_metric_order
        for (metric, pc), value in report_data.items():
            data[(metric, testcase, pc)][config] = value

    sorted_configs = sorted(configs)
    baseline = sorted_configs[0]
    other_configs = sorted_configs[1:]

    testcases = sorted(set(t for _m, t, _p in data.keys()))
    pcs = sorted(set(p for _m, _t, p in data.keys()))

    # Build column headers: Config, TestCase, PC, followed by each metric in original order
    fieldnames = ["Config", "TestCase", "PC"] + metric_order

    output_path = os.path.join(OUTPUT_DIR, SUMMARY_FILENAME)
    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        # Rows grouped by Config -> TestCase -> PC for easy merge-cell layout
        for config in sorted_configs:
            for testcase in testcases:
                for pc in pcs:
                    row = {
                        "Config": config,
                        "TestCase": testcase,
                        "PC": pc,
                    }

                    # Fill original metric values
                    for metric in metric_order:
                        row[metric] = data.get((metric, testcase, pc), {}).get(config, "")

                    writer.writerow(row)

    print(f"Summary report written to: {output_path}")

if __name__ == "__main__":
    main()