#!/usr/bin/env python3
"""Reject tracked text files that are not clean UTF-8.

Nazm source, examples, fixtures and documentation carry Arabic text, so a file
saved in another encoding is a broken file even when it still looks plausible
in one editor. Every tracked text file must decode as strict UTF-8, must not
contain U+FFFD (the mark an editor leaves after a lossy conversion), and must
not start with a byte-order mark. PowerShell and Inno Setup scripts are the
exception to the last rule: Windows PowerShell 5.1 and the Inno Setup compiler
both read a script without a mark in the system code page.

Usage: python tools/check_utf8.py [repository root]
"""

import os
import subprocess
import sys

TEXT_SUFFIXES = (
    ".c", ".h", ".cmake", ".txt", ".md", ".py", ".ps1", ".sh", ".yml",
    ".yaml", ".json", ".s", ".iss", ".نظم", ".baa", ".in", ".cmd",
)
TEXT_NAMES = ("CMakeLists.txt", ".gitignore", ".gitattributes", "LICENSE")
BOM = b"\xef\xbb\xbf"
BOM_ALLOWED_SUFFIXES = (".ps1", ".iss")
REPLACEMENT_CHARACTER = chr(0xFFFD)


def tracked_files(root):
    listing = subprocess.run(
        ["git", "-C", root, "ls-files", "-z"],
        check=True, stdout=subprocess.PIPE,
    ).stdout
    return [name.decode("utf-8") for name in listing.split(b"\0") if name]


def is_text(name):
    return name.endswith(TEXT_SUFFIXES) or os.path.basename(name) in TEXT_NAMES


def problem(path, name):
    with open(path, "rb") as handle:
        data = handle.read()
    if data.startswith(BOM):
        if not name.endswith(BOM_ALLOWED_SUFFIXES):
            return "starts with a UTF-8 byte-order mark"
        data = data[len(BOM):]
    try:
        text = data.decode("utf-8", errors="strict")
    except UnicodeDecodeError as error:
        line = data.count(b"\n", 0, error.start) + 1
        return "invalid UTF-8 at byte %d (line %d)" % (error.start, line)
    position = text.find(REPLACEMENT_CHARACTER)
    if position >= 0:
        return "contains U+FFFD on line %d" % (text.count("\n", 0, position) + 1)
    return None


def main(argv):
    root = os.path.abspath(argv[1]) if len(argv) > 1 else os.getcwd()
    checked = 0
    failures = []
    for name in tracked_files(root):
        if not is_text(name):
            continue
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            continue
        checked += 1
        found = problem(path, name)
        if found:
            failures.append("%s: %s" % (name, found))

    for failure in failures:
        print(failure, file=sys.stderr)
    if failures:
        print("%d of %d text files are not clean UTF-8" % (len(failures), checked),
              file=sys.stderr)
        return 1
    print("%d text files are clean UTF-8" % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
