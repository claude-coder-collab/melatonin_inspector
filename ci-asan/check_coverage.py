#!/usr/bin/env python3
"""Fail unless every executable line changed since a base ref was run by the test."""

import argparse
import re
import subprocess
import sys
from pathlib import Path


def changed_lines(repo: Path, base: str, source: str) -> set[int]:
    diff = subprocess.run(
        ["git", "-C", str(repo), "diff", "-U0", base, "--", source],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    lines: set[int] = set()
    for match in re.finditer(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", diff, re.MULTILINE):
        start, count = int(match[1]), int(match[2] or 1)
        lines.update(range(start, start + count))
    return lines


def line_counts(llvm_cov: str, binary: str, profdata: str, source: Path) -> dict[int, tuple[int, str]]:
    shown = subprocess.run(
        [llvm_cov, "show", binary, f"-instr-profile={profdata}", str(source), "-show-instantiations=false"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    counts: dict[int, tuple[int, str]] = {}
    for row in shown.splitlines():
        match = re.match(r"^\s*(\d+)\|\s*([\d.]+[kMG]?)\|(.*)$", row)
        if match:
            value = match[2]
            scale = {"k": 1e3, "M": 1e6, "G": 1e9}.get(value[-1], 1)
            number = float(value.rstrip("kMG")) * scale
            counts[int(match[1])] = (int(number), match[3])
    return counts


def uncovered(changed: set[int], counts: dict[int, tuple[int, str]]) -> list[tuple[int, str]]:
    return sorted((line, counts[line][1]) for line in changed if line in counts and counts[line][0] == 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--base", required=True)
    parser.add_argument("--source", required=True, help="path relative to the repo")
    parser.add_argument("--binary", required=True)
    parser.add_argument("--profdata", required=True)
    parser.add_argument("--llvm-cov", default="llvm-cov")
    args = parser.parse_args()

    changed = changed_lines(args.repo, args.base, args.source)
    counts = line_counts(args.llvm_cov, args.binary, args.profdata, args.repo / args.source)
    executable = sorted(line for line in changed if line in counts)
    missed = uncovered(changed, counts)

    print(f"{len(executable) - len(missed)}/{len(executable)} executable changed lines covered")
    for line, text in missed:
        print(f"  not covered: {args.source}:{line}: {text.strip()}")
    return 1 if missed or not executable else 0


if __name__ == "__main__":
    sys.exit(main())
