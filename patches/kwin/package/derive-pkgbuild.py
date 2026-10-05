#!/usr/bin/env python3
"""Derive the local patched KWin recipe from Arch Linux's official one.

Usage: derive-pkgbuild.py OFFICIAL_PKGBUILD PATCH > PKGBUILD

The local recipe differs from the official one only as patches/kwin/README.md
describes: a pkgrel above the official and CachyOS rebuilds, the installed
x86_64_v4 architecture, the checksum-pinned patch as a source, and a prepare()
step that applies it without fuzz. Everything else, dependencies and signature
keys included, comes from the official recipe unchanged.
"""

import hashlib
import re
import sys


def array_span(text, name):
    """Return the (start, end) of a top-level `name=( ... )` assignment."""
    match = re.search(rf"^{name}=\(", text, re.M)
    if not match:
        return None
    depth = 0
    for index in range(match.end() - 1, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return match.start(), index + 1
    raise SystemExit(f"unterminated {name} array")


def append_to_array(text, name, item):
    span = array_span(text, name)
    if span is None:
        raise SystemExit(f"official recipe has no {name} array")
    start, end = span
    return text[:end - 1] + f"\n{' ' * (len(name) + 2)}{item}" + text[end - 1:]


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    official_path, patch_path = sys.argv[1:]
    text = open(official_path, encoding="utf-8").read()
    patch_name = patch_path.rsplit("/", 1)[-1]
    patch = open(patch_path, "rb").read()

    pkgrel = re.search(r"^pkgrel=(\S+)$", text, re.M)
    if not pkgrel:
        raise SystemExit("official recipe has no pkgrel")
    # Official N, CachyOS rebuild N.1, local correction N.2.
    base = pkgrel.group(1).split(".")[0]
    text = text[:pkgrel.start()] + f"pkgrel={base}.2" + text[pkgrel.end():]

    arch = array_span(text, "arch")
    if arch is None:
        raise SystemExit("official recipe has no arch array")
    if "x86_64_v4" not in text[arch[0]:arch[1]]:
        text = append_to_array(text, "arch", "x86_64_v4")

    text = append_to_array(text, "source", patch_name)
    sums = 0
    for name, digest in (("sha256sums", hashlib.sha256), ("b2sums", hashlib.blake2b)):
        if array_span(text, name) is not None:
            text = append_to_array(text, name, f"'{digest(patch).hexdigest()}'")
            sums += 1
    if not sums:
        raise SystemExit("official recipe has no sha256sums or b2sums array")

    apply = (
        '  patch --directory="$pkgname-$pkgver" -p1 --fuzz=0 '
        f'< "$srcdir/{patch_name}"\n'
    )
    prepare = re.search(r"^prepare\(\)\s*\{\n", text, re.M)
    if prepare:
        text = text[:prepare.end()] + apply + text[prepare.end():]
    else:
        build = re.search(r"^build\(\)\s*\{", text, re.M)
        if not build:
            raise SystemExit("official recipe has no build()")
        text = text[:build.start()] + "prepare() {\n" + apply + "}\n\n" + text[build.start():]

    sys.stdout.write(text)


if __name__ == "__main__":
    main()
