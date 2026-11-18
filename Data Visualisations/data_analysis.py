
import csv
import random
import statistics
from collections import defaultdict

import matplotlib.pyplot as plt


# ============================================================
# 1. SETTINGS
# ============================================================

CSV_FILE = "events.csv"

# Set to a Match_ID to analyse one match only.
# Leave as None to use all available matches.
MATCH_ID_FILTER = None
TEAM_ID_FILTER = "41103X"

# Minimum number of observations needed to display an event type.
MIN_OBSERVATIONS = 2

# Ignore effectively instantaneous events.
MIN_DURATION = 0.05

# Reproducible horizontal jitter for individual observations.
random.seed(42)


# ============================================================
# 2. LOAD AND PROCESS DATA
# ============================================================

event_durations = defaultdict(list)

invalid_rows = 0
excluded_zero_duration = 0

with open(CSV_FILE, newline="", encoding="utf-8-sig") as file:
    reader = csv.DictReader(file)

    required_columns = {
        "Match_ID",
        "Event_start_sec",
        "Event_end_sec",
        "Event_Type"
    }

    missing_columns = required_columns - set(reader.fieldnames or [])

    if missing_columns:
        raise ValueError(f"Missing CSV columns: {missing_columns}")

    for event in reader:

        # Optional match filter.
        if MATCH_ID_FILTER is not None:
            if event["Match_ID"] != MATCH_ID_FILTER:
                continue

        if TEAM_ID_FILTER is not None:
            if event["Actor_ID"] != TEAM_ID_FILTER:
                continue

        try:
            start = float(event["Event_start_sec"])
            end = float(event["Event_end_sec"])

            duration = end - start

        except (ValueError, TypeError):
            print(event["Event_ID"])
            invalid_rows += 1
            continue

        # Ignore invalid or effectively instantaneous events.
        if duration < 0:
            print(event["Event_ID"])
            invalid_rows += 1
            continue

        if duration <= MIN_DURATION:
            excluded_zero_duration += 1
            continue

        event_type = event["Event_Type"].strip()

        if event_type:
            event_durations[event_type].append(duration)


# ============================================================
# 3. FILTER AND SUMMARISE EVENT TYPES
# ============================================================

plot_data = {}
summary_data = []

for event_type, durations in event_durations.items():

    if len(durations) < MIN_OBSERVATIONS:
        continue

    plot_data[event_type] = durations

    summary_data.append({
        "event_type": event_type,
        "count": len(durations),
        "mean": statistics.mean(durations),
        "median": statistics.median(durations),
        "p90": sorted(durations)[
            min(int(0.90 * len(durations)), len(durations) - 1)
        ],
        "minimum": min(durations),
        "maximum": max(durations)
    })


if not plot_data:
    raise ValueError(
        "No event types have enough valid duration observations."
    )


# Sort by median duration, longest first.
summary_data.sort(
    key=lambda item: item["median"],
    reverse=True
)

ordered_types = [
    item["event_type"] for item in summary_data
]

ordered_durations = [
    plot_data[event_type] for event_type in ordered_types
]


# ============================================================
# 4. PRINT SUMMARY STATISTICS
# ============================================================

print("\nEVENT DURATION SUMMARY")
print("-" * 75)

print(
    f"{'Event Type':<25}"
    f"{'Count':>8}"
    f"{'Mean':>10}"
    f"{'Median':>10}"
    f"{'90th %':>10}"
)

for item in summary_data:

    print(
        f"{item['event_type']:<25}"
        f"{item['count']:>8}"
        f"{item['mean']:>10.2f}"
        f"{item['median']:>10.2f}"
        f"{item['p90']:>10.2f}"
    )

print("\nData quality:")
print(f"Invalid rows: {invalid_rows}")
print(f"Zero/near-zero duration events excluded: {excluded_zero_duration}")


# ============================================================
# 5. CREATE GRAPH
# ============================================================

fig, ax = plt.subplots(figsize=(12, 7))

positions = range(1, len(ordered_types) + 1)

# Box plots show the median, quartiles and spread.
ax.boxplot(
    ordered_durations,
    positions=list(positions),
    widths=0.55,
    showfliers=False
)

# Overlay every individual observation, including outliers.
for position, durations in zip(positions, ordered_durations):

    jitter = [
        position + random.uniform(-0.09, 0.09)
        for _ in durations
    ]

    ax.scatter(
        jitter,
        durations,
        alpha=0.65,
        s=25
    )

# Label event types and sample sizes.
labels = [
    f"{event_type}\n(n={len(plot_data[event_type])})"
    for event_type in ordered_types
]

ax.set_xticks(list(positions))
ax.set_xticklabels(labels, rotation=25, ha="right")

ax.set_title("Event Duration Distributions")
ax.set_xlabel("Event Type")
ax.set_ylabel("Duration (seconds)")

ax.grid(axis="y", alpha=0.3)

plt.tight_layout()

plt.savefig("event_duration_distributions.png", dpi=200)

plt.show()

with open(CSV_FILE, newline="", encoding="utf-8-sig") as file:
    read = csv.DictReader(file)
    AUTONOMOUS_DURATION = 15.0

    # ------------------------------------------------------------
    # 1. CLASSIFY EVENTS
    # ------------------------------------------------------------

    period_data = {
        "Autonomous": [],
        "Driver Control": []
    }

    crossing_events = 0

    for event in read:
        start = float(event["Event_start_sec"])
        end = float(event["Event_end_sec"])
        duration = end - start

        if duration <= 0:
            continue

        # Classify according to when the event began.
        if start < AUTONOMOUS_DURATION:
            period = "Autonomous"
        else:
            period = "Driver Control"

        # Identify events that cross the 15-second boundary.
        if start < AUTONOMOUS_DURATION < end:
            crossing_events += 1

        period_data[period].append({
            "Event_Type": event["Event_Type"].strip(),
            "Duration": duration,
            "Outcome": event.get("Outcome", "").strip().upper(),
            "Actor_ID": event.get("Actor_ID", ""),
            "Match_ID": event.get("Match_ID", "")
        })


    # ------------------------------------------------------------
    # 2. OVERALL PERIOD STATISTICS
    # ------------------------------------------------------------

    print("\nAUTONOMOUS VS DRIVER-CONTROL ANALYSIS")
    print("-" * 75)

    for period, observations in period_data.items():

        durations = [item["Duration"] for item in observations]

        print(f"\n{period}")
        print(f"  Total events: {len(observations)}")

        if durations:
            print(f"  Mean event duration:   {statistics.mean(durations):.2f}s")
            print(f"  Median event duration: {statistics.median(durations):.2f}s")
            print(f"  90th percentile:       {statistics.quantiles(durations, n=10)[8]:.2f}s")

    print(f"\nEvents crossing the 15-second boundary: {crossing_events}")


    # ------------------------------------------------------------
    # 3. STATISTICS BY EVENT TYPE
    # ------------------------------------------------------------

    event_types = sorted(set(
        item["Event_Type"]
        for observations in period_data.values()
        for item in observations
    ))

    print("\nEVENT TYPE COMPARISON")
    print("-" * 95)
    print(
        f"{'Event Type':<22}"
        f"{'Period':<18}"
        f"{'Count':>7}"
        f"{'Mean':>10}"
        f"{'Median':>10}"
        f"{'Success':>10}"
        f"{'Partial':>10}"
        f"{'Failure':>10}"
    )

    for event_type in event_types:

        for period in ["Autonomous", "Driver Control"]:

            observations = [
                item for item in period_data[period]
                if item["Event_Type"] == event_type
            ]

            if not observations:
                continue

            durations = [item["Duration"] for item in observations]

            outcomes = [item["Outcome"] for item in observations]

            success = outcomes.count("SUCCESS")
            partial = outcomes.count("PARTIAL")
            failure = outcomes.count("FAILURE") + outcomes.count("FAIL")

            print(
                f"{event_type:<22}"
                f"{period:<18}"
                f"{len(observations):>7}"
                f"{statistics.mean(durations):>10.2f}"
                f"{statistics.median(durations):>10.2f}"
                f"{success:>10}"
                f"{partial:>10}"
                f"{failure:>10}"
            )


    # ------------------------------------------------------------
    # 4. GRAPH 1: EVENT COUNTS BY PERIOD
    # ------------------------------------------------------------

    auto_counts = []
    driver_counts = []

    for event_type in event_types:

        auto_counts.append(sum(
            1 for item in period_data["Autonomous"]
            if item["Event_Type"] == event_type
        ))

        driver_counts.append(sum(
            1 for item in period_data["Driver Control"]
            if item["Event_Type"] == event_type
        ))

    x = list(range(len(event_types)))
    width = 0.4

    fig, ax = plt.subplots(figsize=(12, 6))

    ax.bar(
        [i - width / 2 for i in x],
        auto_counts,
        width,
        label="Autonomous"
    )

    ax.bar(
        [i + width / 2 for i in x],
        driver_counts,
        width,
        label="Driver Control"
    )

    ax.set_title("Event Counts: Autonomous vs Driver Control")
    ax.set_xlabel("Event Type")
    ax.set_ylabel("Number of Events")
    ax.set_xticks(x)
    ax.set_xticklabels(event_types, rotation=40, ha="right")
    ax.legend()
    ax.grid(axis="y", alpha=0.3)

    plt.tight_layout()
    plt.savefig("autonomous_vs_driver_counts.png", dpi=200)
    plt.show()


    # ------------------------------------------------------------
    # 5. GRAPH 2: EVENT DURATIONS BY PERIOD
    # ------------------------------------------------------------

    plot_data = []
    positions = []
    labels = []

    for i, event_type in enumerate(event_types):

        for offset, period in [
            (-0.2, "Autonomous"),
            (0.2, "Driver Control")
        ]:

            durations = [
                item["Duration"]
                for item in period_data[period]
                if item["Event_Type"] == event_type
            ]

            if durations:
                plot_data.append(durations)
                positions.append(i + offset)
                labels.append((i, period))

    fig, ax = plt.subplots(figsize=(13, 7))

    ax.boxplot(
        plot_data,
        positions=positions,
        widths=0.3,
        showfliers=True
    )

    ax.set_title("Event Duration: Autonomous vs Driver Control")
    ax.set_xlabel("Event Type")
    ax.set_ylabel("Duration (seconds)")
    ax.set_xticks(x)
    ax.set_xticklabels(event_types, rotation=40, ha="right")

    # Legend
    ax.plot([], [], label="Autonomous", marker="s", linestyle="none")
    ax.plot([], [], label="Driver Control", marker="o", linestyle="none")
    ax.legend()

    ax.grid(axis="y", alpha=0.3)

    plt.tight_layout()
    plt.savefig("autonomous_vs_driver_durations.png", dpi=200)
    plt.show()