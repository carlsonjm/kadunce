# Documentation index

Each live document covers one subject and describes what is true now.
`../AGENTS.md` holds the startup set and the writing rules; this index says
where every other subject lives. When two documents disagree, the one listed
for the subject wins.

## Routing

| Document | Owns |
| --- | --- |
| `../README.md` | what Kadunce is and how to install it, for users |
| `../AGENTS.md` | the startup set, holds, writing rules, safety control and handover: the only rulebook for agents |
| `../CLAUDE.md` | the Claude Code entry point; routes to `AGENTS.md` |
| `../TRADEMARKS.md` | the project's names and marks, which the code licence does not cover |
| `ARCHITECTURE.md` | subsystem ownership, the transfer transaction, input ownership, teardown and the invariants |
| `CARD-LIFECYCLE.md` | who owns each window, how it is presented, and what every transition does |
| `INPUT.md` | every input Kadunce answers and what it does |
| `DECISIONS.md` | why each rule holds and what it rejected |
| `LIMITATIONS.md` | what Kadunce does not do yet, and what it will not do |
| `TESTING.md` | what each check proves, running the private harness, promotion, failure classification and confirming the running build |
| `TERMINOLOGY.md` | Kadunce's terms and naming rules |
| `ITASCA-VISUAL-LANGUAGE.md` | the visual and motion language Kadunce shares with the components it works with, and Kadunce's geometry |
| `TETTEGOUCHE-CONTEXT.md` | the versioned context and guest D-Bus interface Kadunce offers Tettegouche, and the stuck notes it reads from Gooseberry |
| `REQUESTS.md` | placement requests: how another program asks Kadunce to open an application at a point, bringing applications back after a sign-in, and saved layouts |
| `TABLE.md` | Table: what it does to workspaces and their windows, how it looks, and its invariants |
| `UPSTREAM.md` | problems in KDE software that could be reported or patched |

## Keeping documentation small

`tests/verify-docs.py`, run by `../verify.sh`, sets a word budget for the
largest documents and for all live documents together. When a budget fails,
trim; removed text stays in Git history. Only the maintainer can agree to raise
a budget.
