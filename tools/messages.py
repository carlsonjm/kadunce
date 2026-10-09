#!/usr/bin/env python3
"""Keeps every on-screen word translatable.

  python3 tools/messages.py                  check (what CI runs)
  python3 tools/messages.py --templates DIR  write each catalog's .pot to DIR

The check fails when shipped QML or C++ shows a quoted word without asking
KI18n for it, when a file asks for words but belongs to no catalog, when Qt's own
translation calls (never loaded here) appear, or when a template can't be made.
A translation is control/po/<language>/<catalog>.po, made from the template;
the control's ki18n_install(po) builds and installs whichever are present.

A line that shows a word on purpose untranslated (a product name, a sample in a
preview) carries the comment `// not translated: <reason>`.

Needs xgettext (gettext).
"""
import argparse
import fnmatch
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

REPO = "kadunce"

# Each catalog, and the sources that ask it for words: the Cards switch in
# the tray, its menu, its messages and its settings window.
DOMAINS = {
    "kadunce-control": ["control/src/*.cpp", "control/src/*.h"],
}

# QML a person sees. The compositor plugin's Table is not yet listed (below).
SHIPPED_QML = []

# C++ a person reads: a QStringLiteral or QLatin1String with words in it.
SHIPPED_CPP = ["control/src/*.cpp", "control/src/*.h"]

# Tests and probes, which no one reads on screen, and the compositor plugin,
# whose words still use Qt's tr(), which KWin never loads for a plugin. It
# moves to a catalog of its own with a build that loads the plugin in KWin.
NOT_SHIPPED = ["tests/*", "tools/*", "control/tests/*", "native/*"]

KEYWORDS = [
    "i18n:1", "i18nc:1c,2", "i18np:1,2", "i18ncp:1c,2,3",
    "i18nd:2", "i18ndc:2c,3", "i18ndp:2,3", "i18ndcp:2c,3,4",
    "xi18n:1", "xi18nc:1c,2", "xi18np:1,2", "xi18ncp:1c,2,3",
    "xi18nd:2", "xi18ndc:2c,3", "xi18ndp:2,3", "xi18ndcp:2c,3,4",
    "ki18n:1", "ki18nc:1c,2", "ki18np:1,2", "ki18ncp:1c,2,3",
    "ki18nd:2", "ki18ndc:2c,3", "ki18ndp:2,3", "ki18ndcp:2c,3,4",
    "kxi18n:1", "kxi18nc:1c,2", "kxi18np:1,2", "kxi18ncp:1c,2,3",
    "kli18n:1", "kli18nc:1c,2", "kli18np:1,2", "kli18ncp:1c,2,3",
    "I18N_NOOP:1", "I18NC_NOOP:1c,2",
]

# Properties whose value a person reads or hears.
SHOWN = (r"text|title|subtitle|label|description|placeholder|placeholderText|"
         r"toolTip|tooltip|ToolTip\.text|toolTipMainText|toolTipSubText|heading|"
         r"explanation|caption|hint|message|question|summary|accessibleName|"
         r"accessibleDescription|Accessible\.name|Accessible\.description|"
         r"displayName|confirmText|cancelText|actionText|emptyText|note")
STRING = r'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\''
ASSIGN = re.compile(r"(?:^|[{,;]|\s)(?:" + SHOWN + r")\s*[:=](?!=)\s*(.*)")
WORD = re.compile(r"[A-Za-z]{2}")


def files(globs):
    found = set()
    tracked = subprocess.run(["git", "ls-files"], cwd=ROOT, check=True,
                             capture_output=True, text=True).stdout.split()
    for pattern in globs:
        found.update(p for p in tracked if fnmatch.fnmatchcase(p, pattern))
    return sorted(found)


def extract(domain, globs, out):
    sources = files(globs)
    if not sources:
        sys.exit(f"messages: no sources match {domain}")
    common = ["xgettext", "--from-code=UTF-8", "--add-comments=i18n",
              "--add-location=file", "--sort-by-file", "--package-name=" + domain,
              "--msgid-bugs-address=https://github.com/carlsonjm/" + REPO + "/issues"]
    common += ["-k"] + ["-k" + k for k in KEYWORDS]
    script = [s for s in sources if s.endswith((".qml", ".js"))]
    native = [s for s in sources if not s.endswith((".qml", ".js"))]
    out.unlink(missing_ok=True)
    for language, group in (("JavaScript", script), ("C++", native)):
        if group:
            subprocess.run(common + ["-L", language, "--join-existing" if out.exists() else "--force-po",
                                     "-o", str(out)] + group,
                           cwd=ROOT, check=True)


def entries(path):
    """The set of (context, word, plural) a template asks translators for."""
    found, current, field = set(), {}, None
    for line in path.read_text(encoding="utf-8").splitlines() + [""]:
        match = re.match(r"(msgctxt|msgid_plural|msgid|msgstr(?:\[\d\])?) (\".*\")$", line)
        if match:
            field = match.group(1)
            current[field] = eval(match.group(2))  # a C string literal
        elif line.startswith('"') and field:
            current[field] += eval(line)
        elif not line.strip():
            if current.get("msgid"):
                found.add((current.get("msgctxt"), current["msgid"], current.get("msgid_plural")))
            current, field = {}, None
    return found


def shown_literals(code):
    """Quoted text the value of one property can show. The value ends at the
    first ; , or } outside brackets. Arguments to other calls (a date pattern,
    a role name), comparisons and colours are data rather than words."""
    calls, found = [], []
    for match in re.finditer(STRING + r"|[()\[\]{};,]", code):
        token = match.group(0)
        if token in "([{":
            calls.append(token == "(" and bool(re.search(r"[\w\]]\s*$", code[:match.start()])))
        elif token in ")]}":
            if not calls:
                break
            calls.pop()
        elif token in ";,":
            if not calls:
                break
        elif not any(calls):
            before = code[:match.start()].rstrip()
            after = code[match.end():].lstrip()
            compared = before.endswith(("==", "!=")) or after.startswith(("==", "!="))
            if not compared and not token[1:].startswith("#"):
                found.append(token[1:-1])
    return found


def loose_words():
    problems = []
    for name in files(SHIPPED_QML):
        lines = (ROOT / name).read_text(encoding="utf-8").splitlines()
        for number, line in enumerate(lines, 1):
            stripped = line.strip()
            if "not translated:" in line or stripped.startswith(("//", "*", "import ")):
                continue
            code = re.split(r"\s//", stripped)[0]
            for match in ASSIGN.finditer(code):
                rest = match.group(1)
                if any(WORD.search(s) for s in shown_literals(rest)):
                    problems.append(f"{name}:{number}: {stripped}")
                    break
    return problems


CPP_LITERAL = re.compile(r'\b(?:QStringLiteral|QLatin1String|QLatin1StringView)\(\s*((?:"(?:[^"\\\n]|\\.)*"\s*)+)\)')


def shown_in_cpp(text):
    """Looks like words: a space between them, or one capitalised word. Paths,
    identifiers, D-Bus names and patterns carry no space and no capital word."""
    if "\\" in text or not WORD.search(text):
        return False
    return " " in text.strip() or bool(re.fullmatch(r"[A-Z][a-z]+…?", text))


def loose_literals():
    problems = []
    for name in files(globals().get("SHIPPED_CPP", [])):
        text = (ROOT / name).read_text(encoding="utf-8")
        lines = text.splitlines()
        for match in CPP_LITERAL.finditer(text):
            number = text.count("\n", 0, match.start()) + 1
            line = lines[number - 1]
            if "not translated:" in line or line.strip().startswith(("//", "*")):
                continue
            words = "".join(re.findall(r'"((?:[^"\\\n]|\\.)*)"', match.group(1)))
            if shown_in_cpp(words):
                problems.append(f"{name}:{number}: {line.strip()}")
    return problems


def unlisted():
    """Files that ask for words but sit in no catalog, and Qt translation calls."""
    listed = {name for globs in DOMAINS.values() for name in files(globs)}
    problems = []
    for name in files(["*.qml", "*.js", "*.cpp", "*.h"]):
        if any(fnmatch.fnmatchcase(name, p) for p in NOT_SHIPPED):
            continue
        text = (ROOT / name).read_text(encoding="utf-8", errors="replace")
        code = re.sub(r"//.*", "", text)
        if re.search(r"\bk?x?i18n[a-z]*\(", code) and name not in listed:
            problems.append(f"{name}: asks for words but is in no catalog")
        if re.search(r"\b(qsTr|qsTranslate|QT_TR_NOOP)\(|(?<![\w.:>])tr\(|QObject::tr\(", code):
            problems.append(f"{name}: uses Qt's translation call, which is never loaded; use i18n")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--templates", metavar="DIR", type=Path,
                        help="write <catalog>.pot for each catalog to DIR")
    args = parser.parse_args()
    if not shutil.which("xgettext"):
        sys.exit("messages: xgettext is missing (install gettext)")

    problems = unlisted()
    loose = loose_words() + loose_literals()
    if loose:
        problems.append("these show words without asking KI18n; wrap them in i18n(), "
                        "or mark the line `// not translated: <reason>`:")
        problems += ["  " + p for p in loose]
    with tempfile.TemporaryDirectory() as scratch:
        out = args.templates or Path(scratch)
        out.mkdir(parents=True, exist_ok=True)
        counts = {}
        for domain, globs in DOMAINS.items():
            pot = out / f"{domain}.pot"
            extract(domain, globs, pot)
            counts[domain] = len(entries(pot))
    if problems:
        print("\n".join("messages: " + p if not p.startswith("  ") else p for p in problems),
              file=sys.stderr)
        return 1
    summary = ", ".join(f"{d} ({n} phrases)" for d, n in counts.items())
    print(f"messages: every shown word asks a catalog: {summary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
