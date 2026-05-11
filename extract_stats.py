#!/usr/bin/env python3
"""Extract the branch-prediction metrics used by the lab."""

import argparse
import re
from pathlib import Path


STAT_NAMES = [
    "simTicks",
    "simInsts",
    "system.cpu.cpi",
    "system.cpu.ipc",
    "system.cpu.branchPred.condPredicted",
    "system.cpu.branchPred.condIncorrect",
]


def read_stats(path):
    text = Path(path).read_text()
    values = {}
    for name in STAT_NAMES:
        match = re.search(rf"^{re.escape(name)}\s+([-+0-9.eE]+)", text, re.M)
        values[name] = float(match.group(1)) if match else None
    predicted = values.get("system.cpu.branchPred.condPredicted")
    incorrect = values.get("system.cpu.branchPred.condIncorrect")
    if predicted and predicted > 0 and incorrect is not None:
        values["condIncorrectRate"] = incorrect / predicted
        values["condAccuracy"] = 1.0 - values["condIncorrectRate"]
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stats", help="Path to gem5 stats.txt")
    args = parser.parse_args()

    values = read_stats(args.stats)
    for key, value in values.items():
        if value is None:
            print(f"{key}: NA")
        elif "Rate" in key or "Accuracy" in key:
            print(f"{key}: {value:.6f}")
        else:
            print(f"{key}: {value}")


if __name__ == "__main__":
    main()
