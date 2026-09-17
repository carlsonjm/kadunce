#!/usr/bin/env python3
"""Guard the small canonical read set without interpreting archived evidence."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOC_INDEX = ROOT / "docs" / "README.md"
ARCHIVE_INDEX = ROOT / "docs" / "archive" / "README.md"
errors: list[str] = []


def tracked_documents() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "--", "*.md", "*.txt"],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    documents = []
    root_docs = {
        "AGENTS.md",
        "CHANGELOG.md",
        "README.md",
        "SWARM.md",
        "THIRD_PARTY_NOTICES.md",
    }
    for path in result.stdout.splitlines():
        if Path(path).name == "CMakeLists.txt" or path.startswith("LICENSES/"):
            continue
        if path in root_docs or path.startswith("docs/"):
            documents.append(path)
        elif path.endswith("/README.md") and path.split("/", 1)[0] in {
            "packaging",
            "patches",
            "tests",
        }:
            documents.append(path)
        elif path.endswith("/THIRD_PARTY.md"):
            documents.append(path)
    return documents


def index_mentions(path: str, text: str) -> bool:
    candidates = {path, Path(path).name}
    if path.startswith("docs/"):
        candidates.add(path.removeprefix("docs/"))
    else:
        candidates.add(f"../{path}")
    if Path(path).name == "README.md" and "/" in path and not path.startswith("docs/"):
        candidates.discard("README.md")
    return any(candidate in text for candidate in candidates)


def check_index_coverage() -> None:
    main_index = DOC_INDEX.read_text()
    archive_index = ARCHIVE_INDEX.read_text()
    for path in tracked_documents():
        if path.startswith("docs/archive/") and path != "docs/archive/README.md":
            if not index_mentions(path, archive_index):
                errors.append(f"archive index does not cover {path}")
        elif not index_mentions(path, main_index):
            errors.append(f"documentation index does not cover {path}")


def check_archive_authority() -> None:
    evidence_link = re.compile(
        r"(?:docs/)?archive/(?!README\.md\b)[^\s)`\]]+\.(?:md|txt)", re.IGNORECASE
    )
    for path in tracked_documents():
        if path.startswith("docs/archive/") or path == "docs/README.md":
            continue
        text = (ROOT / path).read_text()
        for match in evidence_link.finditer(text):
            errors.append(
                f"{path} treats archived evidence as a live reference: {match.group(0)}"
            )


def check_swarm() -> None:
    text = (ROOT / "SWARM.md").read_text()
    marker = "## Active handoffs"
    if marker not in text:
        errors.append("SWARM.md is missing the Active handoffs section")
        return
    active = text.split(marker, 1)[1].strip()
    if active in {"None", "None."}:
        return
    lines = [line.strip() for line in active.splitlines() if line.strip()]
    if any(not line.startswith("- ") for line in lines):
        errors.append("each SWARM handoff must be one bullet on one line")
        return
    if len(lines) > 3:
        errors.append(f"SWARM.md has {len(lines)} live handoffs; maximum is 3")
    for number, line in enumerate(lines, 1):
        words = re.findall(r"[\w][\w'’-]*", line[2:], flags=re.UNICODE)
        if len(words) > 50:
            errors.append(
                f"SWARM.md handoff {number} has {len(words)} words; maximum is 50"
            )


def check_current_state() -> None:
    path = ROOT / "docs" / "CURRENT_STATE.md"
    text = path.read_text()
    if re.search(r"\b[0-9a-f]{7,40}\b", text, flags=re.IGNORECASE):
        errors.append("CURRENT_STATE.md contains a commit hash")
    if re.search(r"\b20\d{2}-\d{2}-\d{2}\b", text):
        errors.append("CURRENT_STATE.md contains dated progress")
    if re.search(
        r"\b(?:today|yesterday|tomorrow|this morning|this afternoon|this evening)\b",
        text,
        flags=re.IGNORECASE,
    ):
        errors.append("CURRENT_STATE.md contains relative progress narration")


check_index_coverage()
check_archive_authority()
check_swarm()
check_current_state()

if errors:
    for error in errors:
        print(f"documentation guard: {error}", file=sys.stderr)
    raise SystemExit(1)

print("Documentation hygiene checks passed.")
