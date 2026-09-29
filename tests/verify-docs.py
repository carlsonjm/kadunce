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
        "CLAUDE.md",
        "README.md",
        "THIRD_PARTY_NOTICES.md",
        "TRADEMARKS.md",
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


# Each subject has one owner (docs/README.md § Routing). These are the owners,
# and the documents whose subjects moved elsewhere must not come back.
REQUIRED_DOCUMENTS = [
    "AGENTS.md",
    "CLAUDE.md",
    "README.md",
    "TRADEMARKS.md",
    "docs/README.md",
    "docs/ARCHITECTURE.md",
    "docs/CARD-LIFECYCLE.md",
    "docs/INPUT.md",
    "docs/DECISIONS.md",
    "docs/ROADMAP.md",
    "docs/TESTING.md",
    "docs/TERMINOLOGY.md",
    "docs/ITASCA-VISUAL-LANGUAGE.md",
    "docs/TETTEGOUCHE-CONTEXT.md",
    "docs/UPSTREAM.md",
    "patches/kwin/README.md",
]
PRIVATE_RECORD = "the private suite record (../shuffle/docs/suite)"
RETIRED_DOCUMENTS = {
    "SWARM.md": PRIVATE_RECORD,
    "docs/ROADMAP-CC.md": PRIVATE_RECORD,
    "docs/ROADMAP-CONTEXT.md": PRIVATE_RECORD,
    "docs/CURRENT_STATE.md": PRIVATE_RECORD,
    "docs/archive": PRIVATE_RECORD,
    "docs/PRODUCT-CONTRACT.md": "docs/INPUT.md and CARD-LIFECYCLE.md",
    "docs/EXPERIENCE-AUDIT.md": "Git history",
    "docs/KNOWN-ISSUES.md": "docs/ROADMAP.md",
    "docs/SHUFFLE-KEYBOARD-1.0-CONCEPT.md": "the shuffle-keyboard repository",
    "tests/unload-probe/README.md": "docs/TESTING.md",
    "patches/kwin/package/README.md": "patches/kwin/README.md",
}


def check_required_documents() -> None:
    for path in REQUIRED_DOCUMENTS:
        if not (ROOT / path).is_file():
            errors.append(f"required document {path} is missing")
    for path, owner in RETIRED_DOCUMENTS.items():
        if (ROOT / path).exists():
            errors.append(f"{path} was retired; its subject belongs to {owner}")


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
    # Archived evidence is kept privately; a checkout may carry none.
    archive_index = ARCHIVE_INDEX.read_text() if ARCHIVE_INDEX.exists() else ""
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




def check_retired_vocabulary() -> None:
    """Catch the retired workspace term across a line break.

    tests/verify-source.sh greps line by line, so a wrapped "Card\nLine" reads
    as two innocent words. This check joins wrapped lines before matching.
    Layer 3 keeps three installed spellings that Block 10b retires together.
    """
    frozen = re.compile(r"showCardLine|cardLine")
    retired = re.compile(r"card\s+line", re.IGNORECASE)
    for path in tracked_documents():
        if path.startswith("docs/archive/") or path == "docs/TERMINOLOGY.md":
            continue
        text = (ROOT / path).read_text()
        for match in retired.finditer(text):
            if "\n" not in match.group(0):
                continue  # already covered, case-sensitively, by verify-source.sh
            line = text.count("\n", 0, match.start()) + 1
            context = text[max(0, match.start() - 40):match.end() + 40]
            if frozen.search(context):
                continue
            errors.append(
                f"{path}:{line} carries the retired workspace term across a line break"
            )




# Live documents regrow when history is appended instead of filed. A document
# over its budget is trimmed, and the removed text lives in Git history. The
# budgets are each document's size after the 27 September cleanup plus about a
# tenth; a budget is raised only with the maintainer's agreement.
WORD_BUDGETS = {
    "AGENTS.md": 975,
    "CLAUDE.md": 80,
    "docs/README.md": 475,
    "docs/DECISIONS.md": 5450,
    "docs/CARD-LIFECYCLE.md": 3650,
    "docs/INPUT.md": 2050,
    "docs/TESTING.md": 3300,
}
LIVE_WORD_BUDGET = 32500


def word_count(text: str) -> int:
    return len(text.split())


def check_word_budgets() -> None:
    total = 0
    for path in tracked_documents():
        if path.startswith("docs/archive/") or not (ROOT / path).exists():
            continue
        words = word_count((ROOT / path).read_text())
        total += words
        budget = WORD_BUDGETS.get(path)
        if budget is not None and words > budget:
            errors.append(
                f"{path} has {words} words; budget is {budget}. Trim it; "
                "removed text lives in Git history"
            )
    if total > LIVE_WORD_BUDGET:
        errors.append(
            f"live documents total {total} words; budget is {LIVE_WORD_BUDGET}"
        )




check_required_documents()
check_index_coverage()
check_archive_authority()
check_word_budgets()
check_retired_vocabulary()

if errors:
    for error in errors:
        print(f"documentation guard: {error}", file=sys.stderr)
    raise SystemExit(1)

print("Documentation hygiene checks passed.")
