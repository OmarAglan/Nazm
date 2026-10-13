#!/usr/bin/env python3
"""Line coverage of src/ from a Coverage build, as one published number.

Usage: coverage_summary.py BUILD_DIR [--minimum PERCENT]

Runs gcov over every .gcda file under BUILD_DIR, keeps the files under src/,
and prints a Markdown table with the total. Exits 1 when the total is below
the minimum, so a change that leaves code untested fails the job.
"""

import argparse
import json
import pathlib
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir")
    parser.add_argument("--minimum", type=float, default=0.0)
    parser.add_argument("--unexecuted", action="store_true",
                        help="also list the line numbers no test executed")
    args = parser.parse_args()

    build_dir = pathlib.Path(args.build_dir).resolve()
    source_root = pathlib.Path(__file__).resolve().parent.parent / "src"
    data_files = sorted(str(p) for p in build_dir.rglob("*.gcda"))
    if not data_files:
        sys.exit("no .gcda files under %s; build with CMAKE_BUILD_TYPE=Coverage "
                 "and run the tests first" % build_dir)

    # One source file is compiled into several targets. A line counts as
    # executed if any of them executed it.
    lines = {}
    with tempfile.TemporaryDirectory() as scratch:
        for data_file in data_files:
            report = subprocess.run(
                ["gcov", "--json-format", "--stdout", data_file],
                cwd=scratch, check=True, capture_output=True, text=True,
            ).stdout
            for document in report.splitlines():
                if not document.strip():
                    continue
                for entry in json.loads(document).get("files", []):
                    path = pathlib.Path(entry["file"])
                    if not path.is_absolute():
                        path = (build_dir / path).resolve()
                    try:
                        relative = path.resolve().relative_to(source_root)
                    except ValueError:
                        continue
                    executed = lines.setdefault(relative.as_posix(), {})
                    for line in entry["lines"]:
                        number = line["line_number"]
                        executed[number] = executed.get(number, 0) + line["count"]

    if not lines:
        sys.exit("gcov reported no file under %s" % source_root)

    total = covered = 0
    print("| File | Lines | Executed | Coverage |")
    print("|---|---:|---:|---:|")
    for name in sorted(lines):
        file_total = len(lines[name])
        file_covered = sum(1 for count in lines[name].values() if count > 0)
        total += file_total
        covered += file_covered
        print("| `src/%s` | %d | %d | %.1f%% |" % (
            name, file_total, file_covered,
            100.0 * file_covered / file_total if file_total else 100.0))
    percent = 100.0 * covered / total
    print("| **Total** | **%d** | **%d** | **%.1f%%** |" % (total, covered, percent))

    if args.unexecuted:
        print()
        for name in sorted(lines):
            missed = sorted(n for n, count in lines[name].items() if count == 0)
            if missed:
                print("- `src/%s`: %s" % (name, " ".join(str(n) for n in missed)))

    if percent < args.minimum:
        print("\nLine coverage %.1f%% is below the required %.1f%%."
              % (percent, args.minimum))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
