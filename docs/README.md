# Documentation index

Each live document owns one subject and states what is true now. `../AGENTS.md`
owns the startup set and the writing rules; this index routes every other
subject to its owner. When two documents disagree, the owner governs.

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
| `ROADMAP.md` | what is planned, and what Kadunce does not do yet |
| `TESTING.md` | what each check proves, running the private harness, promotion, failure classification and confirming the running build |
| `TERMINOLOGY.md` | the suite's approved and retired language |
| `ITASCA-VISUAL-LANGUAGE.md` | the shared visual and motion grammar, and Kadunce's geometry |
| `TETTEGOUCHE-CONTEXT.md` | the versioned context and guest D-Bus interface Kadunce offers Tettegouche, and the stuck notes it reads from Gooseberry |
| `REQUESTS.md` | placement requests: how another program asks Kadunce to open an application at a point |
| `TABLE.md` | Table: what it does to workspaces and their windows, how it looks, and its invariants |
| `UPSTREAM.md` | problems in KDE software Shuffle could report or patch |
| `../patches/kwin/README.md` | the version-bound KWin touch correction, its package provenance and rollback |

## Keeping documentation small

`tests/verify-docs.py`, run by `../verify.sh`, enforces a word budget for the
largest documents and for all live documents together. When a budget fails,
trim: removed text lives in Git history. A budget is raised only with the
maintainer's agreement.
