"""
verify.py — MCP2518FD integration test runner.

Uploads firmware and verifies assertions on real hardware.

Usage:
  # Run one suite (upload + test):
  python tests/integration/verify.py --suite single_node --port COM4
  python tests/integration/verify.py --suite id_filter   --port COM4
  python tests/integration/verify.py --suite two_node    --port COM4 --port-b COM3

  # Run all three suites in sequence (upload + test each):
  python tests/integration/verify.py --suite all --port COM4 --port-b COM3

  # Skip upload — test only, firmware already on board:
  python tests/integration/verify.py --suite single_node --port COM4 --no-upload

Options:
  --suite       Suite to run: single_node | id_filter | two_node | all
  --port        Single-board port, and node A for two_node  (default: COM4)
  --port-b      Node B port for two_node / all              (default: COM3)
  --baud        Baud rate                                   (default: 115200)
  --no-upload   Skip firmware upload, run tests immediately
  --build-only  Build only, no upload or serial verification (for CI)
"""

import sys
import argparse
import logging
import os

sys.path.insert(0, os.path.dirname(__file__))

from mcp_test.runner import run_suite, run_all, SUITES


def configure_logging():
    handler = logging.StreamHandler(sys.stdout)
    handler.stream = open(sys.stdout.fileno(), mode='w',
                          encoding='utf-8', errors='replace', closefd=False)
    handler.setFormatter(logging.Formatter(
        fmt="%(asctime)s  %(levelname)-5s  %(message)s",
        datefmt="%H:%M:%S"
    ))
    logging.getLogger("mcp_test").setLevel(logging.DEBUG)
    logging.getLogger("mcp_test").addHandler(handler)


def main():
    parser = argparse.ArgumentParser(description="MCP2518FD integration test runner")
    parser.add_argument("--suite",     required=True, choices=SUITES + ["all"])
    parser.add_argument("--port",      default="COM4")
    parser.add_argument("--port-b",    default="COM3")
    parser.add_argument("--baud",      type=int, default=115200)
    parser.add_argument("--no-upload", action="store_true")
    parser.add_argument("--build-only", action="store_true",
                        help="Build only, no upload or serial verification")
    args = parser.parse_args()

    configure_logging()

    if args.suite == "all":
        ok = run_all(args.port, args.port_b, args.baud, args.no_upload, args.build_only)
    else:
        ok = run_suite(args.suite, args.port, args.port_b, args.baud, args.no_upload, args.build_only)

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
