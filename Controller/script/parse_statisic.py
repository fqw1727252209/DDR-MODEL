#!/usr/bin/env python3
"""
Parse Statisic.log and generate CSV reports:
1. Summary report per pseudo_channel.
2. Per-transaction main CSV (sorted by CAM Leave).
3. Per-transaction UIF CSV (sorted by UIF Begin, with read/write separation for overlap).
"""

import sys
import csv
import re
import os
from collections import defaultdict

# NOTE: Interactive bandwidth plots require plotly. Please install manually:
#       pip install plotly
try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    PLOTLY_AVAILABLE = True
except ImportError:
    PLOTLY_AVAILABLE = False

# Default transaction size in bytes if not explicitly specified in the log.
DEFAULT_TRANSACTION_SIZE_BYTES = 64

# Peak bandwidth in GB/s, used to calculate bandwidth utilization ratio.
PEAK_BANDWIDTH_GB_PER_S = 25.6

# Window multiplier for bandwidth sampling. Window size = avg_transaction_duration * multiplier.
WINDOW_MULTIPLIER = 500


def parse_time(time_string: str) -> float:
    """
    Parse a time string like '1250000ps' into a float representing picoseconds.

    Args:
        time_string: A string ending with 'ps' (picoseconds) or a raw number.

    Returns:
        The time value in picoseconds as a float.
    """
    time_string = time_string.strip()
    if time_string.endswith("ps"):
        return float(time_string[:-2])
    return float(time_string)


def calculate_bandwidth_gb_per_s(total_bytes: float, time_picoseconds: float) -> float:
    """
    Convert total bytes transferred over a given time into bandwidth (GB/s).

    Args:
        total_bytes: The total number of bytes transferred.
        time_picoseconds: The time duration in picoseconds.

    Returns:
        Bandwidth in GB/s. Returns 0.0 if the time is zero to avoid division by zero.
    """
    if time_picoseconds == 0:
        return 0.0

    # Convert picoseconds to seconds (1 ps = 1e-12 s)
    seconds = time_picoseconds * 1e-12
    # Calculate bytes per second, then convert to GB/s (1 GB = 2^30 bytes)
    return (total_bytes / seconds) / (2 ** 30)


def infer_read_write_type(command_lines: list) -> str:
    """
    Infer the transaction type (Read or Write) from CMD lines in the log block.

    Args:
        command_lines: A list of strings representing command lines from the log.

    Returns:
        'RD' if any read command (RD/RDA) is found,
        'WR' if any write command (WR/WRA) is found,
        'UNKNOWN' otherwise.
    """
    for line in command_lines:
        # Check for write commands first (priority if both are present)
        if "Cmd=WR" in line or "Cmd=WRA" in line:
            return "WR"
        # Check for read commands
        if "Cmd=RD" in line or "Cmd=RDA" in line:
            return "RD"
    return "UNKNOWN"


def parse_log(file_path: str):
    """
    Parse the log file and return a dictionary mapping pseudo_channel to a list
    of transaction dictionaries.

    Args:
        file_path: Path to the log file to be parsed.

    Returns:
        A defaultdict where keys are pseudo_channel integers and values are lists
        of transaction dictionaries.
    """
    if not os.path.exists(file_path):
        print(f"Error: file not found: {file_path}", file=sys.stderr)
        sys.exit(1)

    # Dictionary to hold transactions grouped by pseudo_channel
    transactions_by_channel = defaultdict(list)
    # Dictionary to track seen transaction IDs per pseudo_channel to detect duplicates
    seen_transaction_ids = defaultdict(set)

    with open(file_path, "r", encoding="utf-8") as log_file:
        lines = log_file.readlines()

    current_line_index = 0
    total_lines = len(lines)

    # Iterate through the log file line by line
    while current_line_index < total_lines:
        line = lines[current_line_index]
        # Look for the start of a transaction statistics block
        if "===== Transaction Statistics =====" in line:
            # Initialize a new transaction dictionary with default values
            transaction = {
                "id": None,
                "pseudo_channel": None,
                "size": DEFAULT_TRANSACTION_SIZE_BYTES,
                "rd_wr_type": "UNKNOWN",
                "port_enter": None,
                "port_leave": None,
                "cam_enter": None,
                "cam_leave": None,
                "uif_begin": None,
                "uif_end": None,
                "dfi_begin": None,
                "dfi_end": None,
                "dq_begin": None,
                "dq_end": None,
            }
            command_lines = []
            current_line_index += 1

            # Parse all lines within the transaction block until the closing delimiter
            while (
                current_line_index < total_lines
                and "==================================================" not in lines[current_line_index]
            ):
                stripped_line = lines[current_line_index].strip()

                # Parse transaction ID, size, and pseudo_channel from the ID line
                if stripped_line.startswith("ID:"):
                    id_match = re.search(r"ID:\s*(\d+)", stripped_line)
                    if id_match:
                        transaction["id"] = int(id_match.group(1))
                    size_match = re.search(r"Size:\s*(\d+)", stripped_line)
                    if size_match:
                        transaction["size"] = int(size_match.group(1))
                    channel_match = re.search(
                        r"pseudo_channel:\s*(\d+)", stripped_line
                    )
                    if channel_match:
                        transaction["pseudo_channel"] = int(channel_match.group(1))

                # Parse Port entry and leave timestamps
                elif stripped_line.startswith("Port:"):
                    regex_match = re.search(
                        r"Enter=(\d+ps)\s+Leave=(\d+ps)", stripped_line
                    )
                    if regex_match:
                        transaction["port_enter"] = parse_time(regex_match.group(1))
                        transaction["port_leave"] = parse_time(regex_match.group(2))

                # Parse CAM entry and leave timestamps
                elif stripped_line.startswith("CAM:"):
                    regex_match = re.search(
                        r"Enter=(\d+ps)\s+Leave=(\d+ps)", stripped_line
                    )
                    if regex_match:
                        transaction["cam_enter"] = parse_time(regex_match.group(1))
                        transaction["cam_leave"] = parse_time(regex_match.group(2))

                # Parse UIF begin and end timestamps
                elif stripped_line.startswith("UIF:"):
                    regex_match = re.search(
                        r"Begin=(\d+ps)\s+End=(\d+ps)", stripped_line
                    )
                    if regex_match:
                        transaction["uif_begin"] = parse_time(regex_match.group(1))
                        transaction["uif_end"] = parse_time(regex_match.group(2))

                # Parse DFI begin and end timestamps
                elif stripped_line.startswith("DFI:"):
                    regex_match = re.search(
                        r"Begin=(\d+ps)\s+End=(\d+ps)", stripped_line
                    )
                    if regex_match:
                        transaction["dfi_begin"] = parse_time(regex_match.group(1))
                        transaction["dfi_end"] = parse_time(regex_match.group(2))

                # Parse DQ begin and end timestamps
                elif stripped_line.startswith("DQ:"):
                    regex_match = re.search(
                        r"Begin=(\d+ps)\s+End=(\d+ps)", stripped_line
                    )
                    if regex_match:
                        transaction["dq_begin"] = parse_time(regex_match.group(1))
                        transaction["dq_end"] = parse_time(regex_match.group(2))

                # Collect command lines to determine read/write type later
                elif stripped_line.startswith("Cmd=") or stripped_line.startswith(
                    "Time="
                ):
                    command_lines.append(stripped_line)

                current_line_index += 1

            # Infer the transaction type (Read or Write) after parsing the block
            transaction["rd_wr_type"] = infer_read_write_type(command_lines)

            # Validate that the transaction has a valid ID and pseudo_channel
            if transaction["id"] is None:
                print(
                    "Error: transaction without ID encountered.",
                    file=sys.stderr,
                )
                sys.exit(1)
            if transaction["pseudo_channel"] is None:
                print(
                    f"Error: transaction ID {transaction['id']} without pseudo_channel.",
                    file=sys.stderr,
                )
                sys.exit(1)

            channel = transaction["pseudo_channel"]
            transaction_id = transaction["id"]

            # Check for duplicate transaction IDs within the same pseudo_channel
            if transaction_id in seen_transaction_ids[channel]:
                print(
                    f"Error: duplicate transaction ID {transaction_id} in pseudo_channel {channel}.",
                    file=sys.stderr,
                )
                sys.exit(1)

            seen_transaction_ids[channel].add(transaction_id)
            transactions_by_channel[channel].append(transaction)

        current_line_index += 1

    return transactions_by_channel


def compute_statistics(transactions_by_channel):
    """
    Compute per-pseudo_channel summary statistics.

    Args:
        transactions_by_channel: A dictionary mapping pseudo_channel to a list
            of transaction dictionaries.

    Returns:
        A dictionary mapping pseudo_channel to a dictionary of computed statistics.
    """
    statistics = {}

    for pseudo_channel, transactions in transactions_by_channel.items():
        all_port_latencies = []
        all_cam_latencies = []
        rd_port_latencies = []
        rd_cam_latencies = []
        wr_port_latencies = []
        wr_cam_latencies = []
        rd_count = 0
        wr_count = 0

        # Track the global minimum and maximum timestamps across all stages
        global_min_time = None
        global_max_time = None
        last_end_time = 0.0

        # Sum of all transaction sizes for bandwidth calculation
        total_bytes = sum(transaction["size"] for transaction in transactions)

        # Collect time intervals for UIF, DFI, and DQ stages
        uif_intervals = []
        all_dfi_intervals = []
        rd_dfi_intervals = []
        wr_dfi_intervals = []
        all_dq_intervals = []
        rd_dq_intervals = []
        wr_dq_intervals = []

        for transaction in transactions:
            rw_type = transaction["rd_wr_type"]
            if rw_type == "RD":
                rd_count += 1
            elif rw_type == "WR":
                wr_count += 1

            # Process Port stage latency and timestamps
            if (
                transaction["port_enter"] is not None
                and transaction["port_leave"] is not None
            ):
                port_lat = transaction["port_leave"] - transaction["port_enter"]
                all_port_latencies.append(port_lat)
                if rw_type == "RD":
                    rd_port_latencies.append(port_lat)
                elif rw_type == "WR":
                    wr_port_latencies.append(port_lat)
                time_min = transaction["port_enter"]
                time_max = transaction["port_leave"]
                if global_min_time is None or time_min < global_min_time:
                    global_min_time = time_min
                if global_max_time is None or time_max > global_max_time:
                    global_max_time = time_max
                if time_max > last_end_time:
                    last_end_time = time_max

            # Process CAM stage latency and timestamps
            if (
                transaction["cam_enter"] is not None
                and transaction["cam_leave"] is not None
            ):
                cam_lat = transaction["cam_leave"] - transaction["cam_enter"]
                all_cam_latencies.append(cam_lat)
                if rw_type == "RD":
                    rd_cam_latencies.append(cam_lat)
                elif rw_type == "WR":
                    wr_cam_latencies.append(cam_lat)
                time_min = transaction["cam_enter"]
                time_max = transaction["cam_leave"]
                if global_min_time is None or time_min < global_min_time:
                    global_min_time = time_min
                if global_max_time is None or time_max > global_max_time:
                    global_max_time = time_max
                if time_max > last_end_time:
                    last_end_time = time_max

            # Process UIF stage intervals
            if (
                transaction["uif_begin"] is not None
                and transaction["uif_end"] is not None
            ):
                uif_intervals.append(
                    (transaction["uif_begin"], transaction["uif_end"])
                )
                time_min = transaction["uif_begin"]
                time_max = transaction["uif_end"]
                if global_min_time is None or time_min < global_min_time:
                    global_min_time = time_min
                if global_max_time is None or time_max > global_max_time:
                    global_max_time = time_max
                if time_max > last_end_time:
                    last_end_time = time_max

            # Process DFI stage intervals
            if (
                transaction["dfi_begin"] is not None
                and transaction["dfi_end"] is not None
            ):
                dfi_interval = (transaction["dfi_begin"], transaction["dfi_end"])
                all_dfi_intervals.append(dfi_interval)
                if rw_type == "RD":
                    rd_dfi_intervals.append(dfi_interval)
                elif rw_type == "WR":
                    wr_dfi_intervals.append(dfi_interval)
                time_min = transaction["dfi_begin"]
                time_max = transaction["dfi_end"]
                if global_min_time is None or time_min < global_min_time:
                    global_min_time = time_min
                if global_max_time is None or time_max > global_max_time:
                    global_max_time = time_max
                if time_max > last_end_time:
                    last_end_time = time_max

            # Process DQ stage intervals
            if (
                transaction["dq_begin"] is not None
                and transaction["dq_end"] is not None
            ):
                dq_interval = (transaction["dq_begin"], transaction["dq_end"])
                all_dq_intervals.append(dq_interval)
                if rw_type == "RD":
                    rd_dq_intervals.append(dq_interval)
                elif rw_type == "WR":
                    wr_dq_intervals.append(dq_interval)
                time_min = transaction["dq_begin"]
                time_max = transaction["dq_end"]
                if global_min_time is None or time_min < global_min_time:
                    global_min_time = time_min
                if global_max_time is None or time_max > global_max_time:
                    global_max_time = time_max
                if time_max > last_end_time:
                    last_end_time = time_max

        # Calculate total simulation time from the earliest to the latest timestamp
        simulation_total_time = (
            (global_max_time - global_min_time)
            if global_min_time is not None
            else 0.0
        )

        def merge_intervals(intervals):
            """
            Merge overlapping or adjacent intervals and return the total active time.

            Args:
                intervals: A list of (begin, end) tuples.

            Returns:
                Total active time after merging overlapping intervals.
            """
            if not intervals:
                return 0.0

            # Sort intervals by their start time
            sorted_intervals = sorted(intervals, key=lambda interval: interval[0])
            merged_intervals = [list(sorted_intervals[0])]

            for begin, end in sorted_intervals[1:]:
                # Check if the current interval overlaps with the last merged interval
                if begin <= merged_intervals[-1][1]:
                    # Extend the end if the current interval ends later
                    if end > merged_intervals[-1][1]:
                        merged_intervals[-1][1] = end
                else:
                    # No overlap, add as a new interval
                    merged_intervals.append([begin, end])

            # Sum the durations of all merged intervals
            return sum(end - begin for begin, end in merged_intervals)


        def check_no_overlap(intervals, stage_name, channel):
            """Check that intervals do not overlap and abort if they do."""
            if not intervals:
                return

            sorted_intervals = sorted(intervals, key=lambda interval: interval[0])

            for i in range(1, len(sorted_intervals)):
                prev_begin, prev_end = sorted_intervals[i - 1]
                curr_begin, curr_end = sorted_intervals[i]

                if curr_begin < prev_end:
                    print(
                        f"Error: {stage_name} overlap detected in pseudo_channel {channel} "
                        f"between intervals {prev_begin}-{prev_end} and {curr_begin}-{curr_end}.",
                        file=sys.stderr,
                    )
                    sys.exit(1)


        def calculate_data_total_time(intervals):
            """Calculate span from earliest begin to latest end."""
            if not intervals:
                return 0.0

            return max(end for _, end in intervals) - min(
                begin for begin, _ in intervals
            )


        def calculate_ratio(active_time, total_time):
            """Return active/total ratio, or empty string if total_time is zero."""
            if total_time:
                return active_time / total_time
            return ""


        # Check DQ overlap for all transactions in this channel
        check_no_overlap(all_dq_intervals, "DQ", pseudo_channel)

        # Calculate active times by merging overlapping intervals
        dfi_active_time = merge_intervals(all_dfi_intervals)
        dq_active_time = merge_intervals(all_dq_intervals)

        # Calculate total bus time from the first begin to the last end
        dfi_bus_time = calculate_data_total_time(all_dfi_intervals)
        dq_bus_time = calculate_data_total_time(all_dq_intervals)

        # Calculate active-to-bus time ratios (efficiency)
        dfi_active_bus_ratio = (
            (dfi_active_time / dfi_bus_time) if dfi_bus_time else 0.0
        )
        dfi_bandwidth_value = calculate_bandwidth_gb_per_s(total_bytes, dfi_bus_time)
        dfi_bandwidth_ratio = (
            (dfi_bandwidth_value / PEAK_BANDWIDTH_GB_PER_S) if PEAK_BANDWIDTH_GB_PER_S else 0.0
        )

        dq_active_bus_ratio = (
            (dq_active_time / dq_bus_time) if dq_bus_time else 0.0
        )
        dq_bandwidth_value = calculate_bandwidth_gb_per_s(total_bytes, dq_bus_time)
        dq_bandwidth_ratio = (
            (dq_bandwidth_value / PEAK_BANDWIDTH_GB_PER_S) if PEAK_BANDWIDTH_GB_PER_S else 0.0
        )

        # DFI data total time and ratios
        dfi_rd_active = sum(end - begin for begin, end in rd_dfi_intervals)
        dfi_rd_total = calculate_data_total_time(rd_dfi_intervals)
        dfi_wr_active = sum(end - begin for begin, end in wr_dfi_intervals)
        dfi_wr_total = calculate_data_total_time(wr_dfi_intervals)
        dfi_all_active = sum(end - begin for begin, end in all_dfi_intervals)
        dfi_all_total = calculate_data_total_time(all_dfi_intervals)

        # DQ data total time and ratios
        dq_rd_active = sum(end - begin for begin, end in rd_dq_intervals)
        dq_rd_total = calculate_data_total_time(rd_dq_intervals)
        dq_wr_active = sum(end - begin for begin, end in wr_dq_intervals)
        dq_wr_total = calculate_data_total_time(wr_dq_intervals)
        dq_all_active = sum(end - begin for begin, end in all_dq_intervals)
        dq_all_total = calculate_data_total_time(all_dq_intervals)

        def compute_safe_statistics(values_array):
            """
            Compute average, minimum, and maximum of a list of values safely.

            Args:
                values_array: A list of numeric values.

            Returns:
                A tuple of (average, minimum, maximum). Returns (0.0, 0.0, 0.0)
                if the list is empty.
            """
            if not values_array:
                return 0.0, 0.0, 0.0
            return sum(values_array) / len(values_array), min(values_array), max(values_array)

        port_average, port_minimum, port_maximum = compute_safe_statistics(
            all_port_latencies
        )
        rd_port_average, rd_port_minimum, rd_port_maximum = compute_safe_statistics(
            rd_port_latencies
        )
        wr_port_average, wr_port_minimum, wr_port_maximum = compute_safe_statistics(
            wr_port_latencies
        )
        cam_average, cam_minimum, cam_maximum = compute_safe_statistics(
            all_cam_latencies
        )
        rd_cam_average, rd_cam_minimum, rd_cam_maximum = compute_safe_statistics(
            rd_cam_latencies
        )
        wr_cam_average, wr_cam_minimum, wr_cam_maximum = compute_safe_statistics(
            wr_cam_latencies
        )

        # Store all computed statistics for this pseudo_channel
        statistics[pseudo_channel] = {
            "pseudo_channel": pseudo_channel,
            "transaction_count": len(transactions),
            "rd_transaction_count": rd_count,
            "wr_transaction_count": wr_count,
            "port_latency_avg_ps": port_average,
            "port_latency_min_ps": port_minimum,
            "port_latency_max_ps": port_maximum,
            "rd_port_latency_avg_ps": rd_port_average,
            "rd_port_latency_min_ps": rd_port_minimum,
            "rd_port_latency_max_ps": rd_port_maximum,
            "wr_port_latency_avg_ps": wr_port_average,
            "wr_port_latency_min_ps": wr_port_minimum,
            "wr_port_latency_max_ps": wr_port_maximum,
            "cam_latency_avg_ps": cam_average,
            "cam_latency_min_ps": cam_minimum,
            "cam_latency_max_ps": cam_maximum,
            "rd_cam_latency_avg_ps": rd_cam_average,
            "rd_cam_latency_min_ps": rd_cam_minimum,
            "rd_cam_latency_max_ps": rd_cam_maximum,
            "wr_cam_latency_avg_ps": wr_cam_average,
            "wr_cam_latency_min_ps": wr_cam_minimum,
            "wr_cam_latency_max_ps": wr_cam_maximum,
            "sim_total_time_ps": simulation_total_time,
            "last_end_time_ps": last_end_time,
            "dfi_bus_time_ps": dfi_bus_time,
            "dfi_active_bus_ratio": dfi_active_bus_ratio,
            "dfi_bw_gb_s": dfi_bandwidth_value,
            "dfi_bw_ratio": dfi_bandwidth_ratio,
            "dfi_rd_data_total_time_ps": dfi_rd_total,
            "dfi_rd_active_ratio": calculate_ratio(dfi_rd_active, dfi_rd_total),
            "dfi_wr_data_total_time_ps": dfi_wr_total,
            "dfi_wr_active_ratio": calculate_ratio(dfi_wr_active, dfi_wr_total),
            "dfi_all_data_total_time_ps": dfi_all_total,
            "dfi_all_active_ratio": calculate_ratio(dfi_all_active, dfi_all_total),
            "dq_bus_time_ps": dq_bus_time,
            "dq_active_bus_ratio": dq_active_bus_ratio,
            "dq_bw_gb_s": dq_bandwidth_value,
            "dq_bw_ratio": dq_bandwidth_ratio,
            "dq_rd_data_total_time_ps": dq_rd_total,
            "dq_rd_active_ratio": calculate_ratio(dq_rd_active, dq_rd_total),
            "dq_wr_data_total_time_ps": dq_wr_total,
            "dq_wr_active_ratio": calculate_ratio(dq_wr_active, dq_wr_total),
            "dq_all_data_total_time_ps": dq_all_total,
            "dq_all_active_ratio": calculate_ratio(dq_all_active, dq_all_total),
        }

    return statistics


def write_summary_csv(statistics, output_file_path):
    """
    Write summary statistics to a transposed CSV file.

    Each row represents a metric and each column represents a pseudo_channel,
    making it easier to browse when there are many metrics.

    Args:
        statistics: A dictionary mapping pseudo_channel to statistics dictionaries.
        output_file_path: Path to the output CSV file.
    """
    field_names = [
        "pseudo_channel",
        "transaction_count",
        "rd_transaction_count",
        "wr_transaction_count",
        "port_latency_avg_ps",
        "port_latency_min_ps",
        "port_latency_max_ps",
        "rd_port_latency_avg_ps",
        "rd_port_latency_min_ps",
        "rd_port_latency_max_ps",
        "wr_port_latency_avg_ps",
        "wr_port_latency_min_ps",
        "wr_port_latency_max_ps",
        "cam_latency_avg_ps",
        "cam_latency_min_ps",
        "cam_latency_max_ps",
        "rd_cam_latency_avg_ps",
        "rd_cam_latency_min_ps",
        "rd_cam_latency_max_ps",
        "wr_cam_latency_avg_ps",
        "wr_cam_latency_min_ps",
        "wr_cam_latency_max_ps",
        "sim_total_time_ps",
        "last_end_time_ps",
        "dfi_bus_time_ps",
        "dfi_active_bus_ratio",
        "dfi_bw_gb_s",
        "dfi_bw_ratio",
        "dfi_rd_data_total_time_ps",
        "dfi_rd_active_ratio",
        "dfi_wr_data_total_time_ps",
        "dfi_wr_active_ratio",
        "dfi_all_data_total_time_ps",
        "dfi_all_active_ratio",
        "dq_bus_time_ps",
        "dq_active_bus_ratio",
        "dq_bw_gb_s",
        "dq_bw_ratio",
        "dq_rd_data_total_time_ps",
        "dq_rd_active_ratio",
        "dq_wr_data_total_time_ps",
        "dq_wr_active_ratio",
        "dq_all_data_total_time_ps",
        "dq_all_active_ratio",
    ]

    channels = sorted(statistics.keys())
    header = ["Metric"] + [f"PC{c}" for c in channels]

    rows = []
    for field in field_names:
        row = [field]
        for c in channels:
            row.append(statistics[c].get(field, ""))
        rows.append(row)

    with open(output_file_path, "w", newline="", encoding="utf-8") as output_file:
        csv_writer = csv.writer(output_file)
        csv_writer.writerow(header)
        csv_writer.writerows(rows)

    print(f"Summary CSV report written to: {output_file_path}")


def _compute_overlap_map(
    transaction_list_for_channel, begin_key, end_key
):
    """
    Pre-compute overlap and gap information by sorting transactions on the
    actual time axis (e.g., dfi_begin).

    Args:
        transaction_list_for_channel: A list of transaction dictionaries for a single channel.
        begin_key: The dictionary key for the interval begin time.
        end_key: The dictionary key for the interval end time.

    Returns:
        A dictionary mapping transaction['id'] to a tuple (overlap_boolean, gap_value).
    """
    overlap_map = {}
    # Sort transactions by the specified begin time, placing None values at the end
    sorted_transactions = sorted(
        transaction_list_for_channel,
        key=lambda transaction: (
            transaction[begin_key] if transaction[begin_key] is not None else float("inf")
        ),
    )

    previous_transaction = None
    for transaction in sorted_transactions:
        if transaction[begin_key] is not None and transaction[end_key] is not None:
            if previous_transaction is not None:
                # Determine if the current transaction overlaps with the previous one
                # Overlap occurs if they are NOT completely disjoint
                is_overlapping = not (
                    transaction[begin_key] >= previous_transaction[end_key]
                    or transaction[end_key] <= previous_transaction[begin_key]
                )
                gap = transaction[begin_key] - previous_transaction[end_key]
                overlap_map[transaction["id"]] = (is_overlapping, gap)
            previous_transaction = transaction

    return overlap_map


def write_main_transaction_csv(transactions_by_channel, output_file_path):
    """
    Write per-transaction main CSV sorted by CAM Leave per pseudo_channel.

    Columns include DFI/DQ duration and overlap with the previous transaction.
    Overlap is computed based on the actual DFI/DQ time order, not CAM Leave order.

    Args:
        transactions_by_channel: A dictionary mapping pseudo_channel to transaction lists.
        output_file_path: Path to the output CSV file.
    """
    field_names = [
        "pseudo_channel",
        "id",
        "rd_wr_type",
        "port_latency_ps",
        "cam_latency_ps",
        "cam_leave_ps",
        "cmd_gap_to_prev_ps",
        "dfi_begin_ps",
        "dfi_end_ps",
        "dfi_duration_ps",
        "dq_begin_ps",
        "dq_end_ps",
        "dq_duration_ps",
        "dq_overlap_with_prev",
        "dq_gap_to_prev_ps",
    ]

    rows = []
    for pseudo_channel in sorted(transactions_by_channel.keys()):
        # Output order: sorted by CAM Leave time
        sorted_transactions = sorted(
            transactions_by_channel[pseudo_channel],
            key=lambda transaction: (
                transaction["cam_leave"]
                if transaction["cam_leave"] is not None
                else float("inf")
            ),
        )

        # Overlap must be computed on the actual DQ time axis, not CAM Leave order
        dq_overlap_map = _compute_overlap_map(
            transactions_by_channel[pseudo_channel], "dq_begin", "dq_end"
        )

        previous_cam_leave = None
        for transaction in sorted_transactions:
            # Calculate DFI and DQ durations
            dfi_duration = (
                (transaction["dfi_end"] - transaction["dfi_begin"])
                if transaction["dfi_begin"] is not None
                and transaction["dfi_end"] is not None
                else 0.0
            )

            dq_duration = (
                (transaction["dq_end"] - transaction["dq_begin"])
                if transaction["dq_begin"] is not None
                and transaction["dq_end"] is not None
                else 0.0
            )

            dq_overlap, dq_gap = dq_overlap_map.get(
                transaction["id"], (False, "")
            )

            cmd_gap = ""
            if (
                previous_cam_leave is not None
                and transaction["cam_leave"] is not None
            ):
                cmd_gap = transaction["cam_leave"] - previous_cam_leave

            rows.append(
                {
                    "pseudo_channel": pseudo_channel,
                    "id": transaction["id"],
                    "rd_wr_type": transaction["rd_wr_type"],
                    "port_latency_ps": (
                        (transaction["port_leave"] - transaction["port_enter"])
                        if transaction["port_enter"] is not None
                        and transaction["port_leave"] is not None
                        else 0.0
                    ),
                    "cam_latency_ps": (
                        (transaction["cam_leave"] - transaction["cam_enter"])
                        if transaction["cam_enter"] is not None
                        and transaction["cam_leave"] is not None
                        else 0.0
                    ),
                    "cam_leave_ps": (
                        transaction["cam_leave"]
                        if transaction["cam_leave"] is not None
                        else ""
                    ),
                    "cmd_gap_to_prev_ps": cmd_gap,
                    "dfi_begin_ps": (
                        transaction["dfi_begin"]
                        if transaction["dfi_begin"] is not None
                        else ""
                    ),
                    "dfi_end_ps": (
                        transaction["dfi_end"]
                        if transaction["dfi_end"] is not None
                        else ""
                    ),
                    "dfi_duration_ps": dfi_duration,
                    "dq_begin_ps": (
                        transaction["dq_begin"]
                        if transaction["dq_begin"] is not None
                        else ""
                    ),
                    "dq_end_ps": (
                        transaction["dq_end"]
                        if transaction["dq_end"] is not None
                        else ""
                    ),
                    "dq_duration_ps": dq_duration,
                    "dq_overlap_with_prev": "TRUE" if dq_overlap else "FALSE",
                    "dq_gap_to_prev_ps": dq_gap,
                }
            )

            if transaction["cam_leave"] is not None:
                previous_cam_leave = transaction["cam_leave"]

    with open(output_file_path, "w", newline="", encoding="utf-8") as output_file:
        csv_writer = csv.DictWriter(output_file, fieldnames=field_names)
        csv_writer.writeheader()
        csv_writer.writerows(rows)

    print(f"Main transaction CSV written to: {output_file_path}")


def write_uif_transaction_csv(transactions_by_channel, output_file_path):
    """
    Write per-transaction UIF CSV sorted by UIF Begin per pseudo_channel,
    with read/write separation for overlap detection.

    Args:
        transactions_by_channel: A dictionary mapping pseudo_channel to transaction lists.
        output_file_path: Path to the output CSV file.
    """
    field_names = [
        "pseudo_channel",
        "id",
        "rd_wr_type",
        "uif_begin_ps",
        "uif_end_ps",
        "uif_duration_ps",
        "uif_overlap_with_prev",
    ]

    rows = []
    for pseudo_channel in sorted(transactions_by_channel.keys()):
        # Separate transactions by read/write type to detect overlaps within each type
        type_groups = defaultdict(list)
        for transaction in transactions_by_channel[pseudo_channel]:
            type_groups[transaction["rd_wr_type"]].append(transaction)

        for read_write_type in sorted(type_groups.keys()):
            sorted_transactions = sorted(
                type_groups[read_write_type],
                key=lambda transaction: (
                    transaction["uif_begin"]
                    if transaction["uif_begin"] is not None
                    else float("inf")
                ),
            )
            previous_uif = None
            for transaction in sorted_transactions:
                uif_duration = (
                    (transaction["uif_end"] - transaction["uif_begin"])
                    if transaction["uif_begin"] is not None
                    and transaction["uif_end"] is not None
                    else 0.0
                )

                # Determine if this UIF interval overlaps with the previous one
                uif_overlap = False
                if (
                    previous_uif is not None
                    and transaction["uif_begin"] is not None
                    and transaction["uif_end"] is not None
                ):
                    uif_overlap = not (
                        transaction["uif_begin"] >= previous_uif[1]
                        or transaction["uif_end"] <= previous_uif[0]
                    )

                rows.append(
                    {
                        "pseudo_channel": pseudo_channel,
                        "id": transaction["id"],
                        "rd_wr_type": transaction["rd_wr_type"],
                        "uif_begin_ps": (
                            transaction["uif_begin"]
                            if transaction["uif_begin"] is not None
                            else ""
                        ),
                        "uif_end_ps": (
                            transaction["uif_end"]
                            if transaction["uif_end"] is not None
                            else ""
                        ),
                        "uif_duration_ps": uif_duration,
                        "uif_overlap_with_prev": "TRUE" if uif_overlap else "FALSE",
                    }
                )

                # Update previous_uif for the next iteration if current interval is valid
                if (
                    transaction["uif_begin"] is not None
                    and transaction["uif_end"] is not None
                ):
                    previous_uif = (transaction["uif_begin"], transaction["uif_end"])

    with open(output_file_path, "w", newline="", encoding="utf-8") as output_file:
        csv_writer = csv.DictWriter(output_file, fieldnames=field_names)
        csv_writer.writeheader()
        csv_writer.writerows(rows)

    print(f"UIF transaction CSV written to: {output_file_path}")


def compute_windowed_bandwidth(transactions_by_channel, stage):
    """
    Compute per-window bandwidth for each pseudo_channel.

    Args:
        transactions_by_channel: dict mapping channel -> list of transactions.
        stage: 'dfi' or 'dq'.

    Returns:
        dict mapping channel -> dict with 'time_labels', 'rd_bw', 'wr_bw', 'cum_avg'.
    """
    begin_key = f"{stage}_begin"
    end_key = f"{stage}_end"
    windowed_data = {}

    for pc, transactions in transactions_by_channel.items():
        valid = [t for t in transactions if t[begin_key] is not None and t[end_key] is not None]
        if not valid:
            continue

        durations = [t[end_key] - t[begin_key] for t in valid]
        avg_duration = sum(durations) / len(durations)
        window_size = avg_duration * WINDOW_MULTIPLIER

        last_end = max(t[end_key] for t in valid)
        num_windows = max(1, int(last_end / window_size) + 1)

        rd_bytes = [0.0] * num_windows
        wr_bytes = [0.0] * num_windows

        for t in valid:
            idx = int(t[begin_key] / window_size)
            idx = min(idx, num_windows - 1)
            if t["rd_wr_type"] == "RD":
                rd_bytes[idx] += t["size"]
            elif t["rd_wr_type"] == "WR":
                wr_bytes[idx] += t["size"]

        window_size_sec = window_size * 1e-12
        rd_bw = [b / window_size_sec / (2 ** 30) for b in rd_bytes]
        wr_bw = [b / window_size_sec / (2 ** 30) for b in wr_bytes]

        cum_rd_bytes = 0.0
        cum_wr_bytes = 0.0
        cum_avg = []
        for i in range(num_windows):
            cum_rd_bytes += rd_bytes[i]
            cum_wr_bytes += wr_bytes[i]
            elapsed = min((i + 1) * window_size, last_end)
            if elapsed > 0:
                avg = (cum_rd_bytes + cum_wr_bytes) / (elapsed * 1e-12) / (2 ** 30)
            else:
                avg = 0.0
            cum_avg.append(avg)

        time_labels = [
            ((i + 0.5) * window_size) / 1e6 for i in range(num_windows)
        ]

        windowed_data[pc] = {
            "time_labels": time_labels,
            "rd_bw": rd_bw,
            "wr_bw": wr_bw,
            "cum_avg": cum_avg,
        }

    return windowed_data


def write_bandwidth_html(windowed_data, output_path, title):
    """
    Write an interactive Plotly HTML with stacked bar charts (per-window BW)
    and dashed line charts (cumulative average BW).

    Bars are stacked in the order: PC0_RD -> PC0_WR -> PC1_RD -> PC1_WR.
    """
    if not windowed_data:
        print(f"No data for {title}, skipping HTML generation.")
        return

    fig = make_subplots(specs=[[{"secondary_y": True}]])

    # Per-PC color palette: each PC shares a hue family (RD=dark, WR=light)
    pc_colors = [
        {"rd": "#1f77b4", "wr": "#aec7e8"},  # PC0: blue family
        {"rd": "#2ca02c", "wr": "#98df8a"},  # PC1: green family
        {"rd": "#9467bd", "wr": "#c5b0d5"},  # PC2: purple family
        {"rd": "#d62728", "wr": "#ff9896"},  # PC3: red family
    ]

    # High-contrast colors for cumulative average lines
    line_colors = ["#d62728", "#000000", "#9467bd", "#ff7f0e"]

    channels = sorted(windowed_data.keys())
    bar_trace_count = 0
    line_trace_count = 0

    # Layer 1: all RD bars stacked at the bottom
    for i, pc in enumerate(channels):
        data = windowed_data[pc]
        time_labels = data["time_labels"]
        rd_bw = data["rd_bw"]
        rd_color = pc_colors[i % len(pc_colors)]["rd"]

        fig.add_trace(
            go.Bar(
                name=f"PC{pc} RD",
                x=time_labels,
                y=rd_bw,
                marker_color=rd_color,
                opacity=0.85,
                legendgroup=f"PC{pc}",
                hovertemplate=f"Time: %{{x:.3f}} us<br>PC{pc} RD BW: %{{y:.3f}} GB/s<extra></extra>",
            ),
            secondary_y=False,
        )
        bar_trace_count += 1

    # Layer 2: all WR bars stacked on top of RD bars
    for i, pc in enumerate(channels):
        data = windowed_data[pc]
        time_labels = data["time_labels"]
        wr_bw = data["wr_bw"]
        wr_color = pc_colors[i % len(pc_colors)]["wr"]

        fig.add_trace(
            go.Bar(
                name=f"PC{pc} WR",
                x=time_labels,
                y=wr_bw,
                marker_color=wr_color,
                opacity=0.85,
                legendgroup=f"PC{pc}",
                hovertemplate=f"Time: %{{x:.3f}} us<br>PC{pc} WR BW: %{{y:.3f}} GB/s<extra></extra>",
            ),
            secondary_y=False,
        )
        bar_trace_count += 1

    # Layer 3: cumulative average lines on secondary y-axis
    for i, pc in enumerate(channels):
        data = windowed_data[pc]
        time_labels = data["time_labels"]
        cum_avg = data["cum_avg"]
        line_color = line_colors[i % len(line_colors)]

        fig.add_trace(
            go.Scatter(
                name=f"PC{pc} Cum Avg",
                x=time_labels,
                y=cum_avg,
                mode="lines",
                line=dict(color=line_color, width=2, dash="dash", shape="spline"),
                legendgroup=f"PC{pc}",
                hovertemplate=f"Time: %{{x:.3f}} us<br>PC{pc} Cum Avg: %{{y:.3f}} GB/s<extra></extra>",
            ),
            secondary_y=True,
        )
        line_trace_count += 1

    num_bar = bar_trace_count
    num_line = line_trace_count
    dropdown_buttons = []

    # Show All Lines
    dropdown_buttons.append(
        dict(
            label="Show All Lines",
            method="update",
            args=[{"visible": [True] * (num_bar + num_line)}],
        )
    )

    # Per-channel options
    for target_pc in channels:
        visible = [True] * num_bar
        for pc in channels:
            visible.append(pc == target_pc)
        dropdown_buttons.append(
            dict(
                label=f"PC{target_pc} Only",
                method="update",
                args=[{"visible": visible}],
            )
        )

    # Hide All Lines
    dropdown_buttons.append(
        dict(
            label="Hide All Lines",
            method="update",
            args=[{"visible": [True] * num_bar + [False] * num_line}],
        )
    )

    fig.update_layout(
        title=title,
        xaxis_title="Time (us)",
        yaxis_title="Per-Window Bandwidth (GB/s)",
        yaxis2_title="Cumulative Average Bandwidth (GB/s)",
        barmode="stack",
        bargap=0.25,
        legend=dict(
            orientation="h",
            yanchor="bottom",
            y=1.02,
            xanchor="right",
            x=1,
        ),
        updatemenus=[
            dict(
                type="dropdown",
                direction="down",
                showactive=True,
                buttons=dropdown_buttons,
                x=0.1,
                xanchor="left",
                y=1.15,
                yanchor="top",
            )
        ],
        hovermode="x unified",
        width=1600,
        height=800,
        margin=dict(l=80, r=80, t=120, b=80),
    )

    # Ensure axes are non-negative
    fig.update_xaxes(rangemode="nonnegative", rangeslider=dict(visible=True))
    fig.update_yaxes(rangemode="nonnegative", secondary_y=False)
    fig.update_yaxes(rangemode="nonnegative", secondary_y=True)

    fig.write_html(output_path, include_plotlyjs="cdn")
    print(f"Interactive bandwidth HTML written to: {output_path}")


def main():
    """
    Main entry point for the script.

    Parses a log file and generates three CSV reports:
    1. A summary report with per-channel statistics.
    2. A main transaction report sorted by CAM Leave.
    3. A UIF transaction report sorted by UIF Begin with read/write separation.
    """
    if len(sys.argv) < 2:
        log_file_path = "Statisic.log"
    else:
        log_file_path = sys.argv[1]

    # Determine output file paths based on the input log file name
    log_directory = os.path.dirname(log_file_path)
    base_file_name = os.path.splitext(os.path.basename(log_file_path))[0]
    summary_output_path = os.path.join(
        log_directory, base_file_name + "_report.csv"
    )
    main_transaction_output_path = os.path.join(
        log_directory, base_file_name + "_transactions.csv"
    )
    uif_transaction_output_path = os.path.join(
        log_directory, base_file_name + "_uif_transactions.csv"
    )

    # Parse the log file into transaction data
    transactions_by_channel = parse_log(log_file_path)

    # Compute summary statistics
    statistics = compute_statistics(transactions_by_channel)

    # Write the three CSV reports
    write_summary_csv(statistics, summary_output_path)
    write_main_transaction_csv(transactions_by_channel, main_transaction_output_path)
    write_uif_transaction_csv(transactions_by_channel, uif_transaction_output_path)

    # Generate interactive bandwidth HTML if plotly is available
    if PLOTLY_AVAILABLE:
        dfi_windowed = compute_windowed_bandwidth(transactions_by_channel, "dfi")
        dq_windowed = compute_windowed_bandwidth(transactions_by_channel, "dq")
        dfi_html_path = os.path.join(
            log_directory, base_file_name + "_bandwidth_dfi.html"
        )
        dq_html_path = os.path.join(
            log_directory, base_file_name + "_bandwidth_dq.html"
        )
        write_bandwidth_html(
            dfi_windowed, dfi_html_path, f"{base_file_name} - DFI Bandwidth"
        )
        write_bandwidth_html(
            dq_windowed, dq_html_path, f"{base_file_name} - DQ Bandwidth"
        )
    else:
        print(
            "Note: plotly not installed. Skipping interactive bandwidth HTML generation."
        )
        print("      Install with: pip install plotly")


if __name__ == "__main__":
    main()