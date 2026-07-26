#!/usr/bin/env python3
"""
Run from tests/unit/:
    python coverage.py

Requires: pio test -e native has already been run (produces .gcda files).
Produces: coverage/html/index.html  +  prints a line/branch summary.
"""
import subprocess, os, sys

BUILD_DIR  = os.path.join(".pio", "build", "native")
REPORT_DIR = "coverage"
INFO_RAW   = os.path.join(REPORT_DIR, "coverage.info")
INFO_FILT  = os.path.join(REPORT_DIR, "coverage_filtered.info")
HTML_DIR   = os.path.join(REPORT_DIR, "html")

def run(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        sys.exit(result.returncode)
    return result.stdout

os.makedirs(REPORT_DIR, exist_ok=True)

print("Collecting coverage data...")
run(["lcov", "--capture",
     "--directory", BUILD_DIR,
     "--output-file", INFO_RAW,
     "--ignore-errors", "mismatch,gcov",
     "--rc", "branch_coverage=1"])

print("Filtering out system and test framework files...")
run(["lcov", "--remove", INFO_RAW,
     "*/unity/*", "*/Unity/*", "*/test/*", "*/unity_config*", "/usr/*",
     "--output-file", INFO_FILT,
     "--ignore-errors", "unused",
     "--rc", "branch_coverage=1"])

print("Generating HTML report...")
run(["genhtml", INFO_FILT,
     "--output-directory", HTML_DIR,
     "--branch-coverage",
     "--title", "mcp2518fd unit tests"])

print("\n--- Coverage summary ---")
summary = run(["lcov", "--summary", INFO_FILT, "--rc", "branch_coverage=1"])
print(summary)
print(f"HTML report: tests/unit/{HTML_DIR}/index.html")
