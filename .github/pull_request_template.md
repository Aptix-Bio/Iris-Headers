<!--
Iris-Headers pull request. Fill each section from development/: WBS.md (the work), RTM.md (the
requirements) and ARCHITECTURE.md (the decisions, §4); README.md gives the sections' order.
Write "None" in a section that does not apply; never delete one. The review bot reads this
description as data and checks it against the diff (.github/review/rubric.toml).

"Agent review" at the bottom is not yours to tick: the review bot rewrites it on every run. A pull
request from a fork gets no model review; a maintainer reviews it.
Branch name: <kind>/<WorkItemName>, e.g. feature/StrongIndices_4.2.
-->

## Summary

<!-- Two or three sentences: what this delivers, and why. -->

## WBS

<!-- WBS.md work items completed or advanced, nested as there. Tick only steps whose
evidence is on the same line, with a link to the commit that did the work. -->

- **_WorkPackage_n.n_**
  - [x] _step_ — _evidence_ — [_1234567_](https://github.com/Aptix-Bio/Iris-Headers/commit/_full-hash_)

## RTM

<!-- RTM.md rows added or moved, with the evidence; a row moved to Implemented or Verified links
the commit that did the work. The development/ERRATA.md row this change adds or whose guard it
changes. "None" if none. -->

| Row | Change | Evidence |
|---|---|---|
| _Area_nn_ | _Planned → Implemented → Verified_ | _test tagged `[Area_nn]` or `// RTM Area_nn`, file_ |

## Decisions

<!-- Decisions made or changed, one line each, dated, each stated as a plain requirement, with
the consequence; recorded in ARCHITECTURE.md §4 (e.g. ResultShapeQuestion_3). "None" if none. -->

## Measurements

<!-- Numbers this change was decided on (throughput, latency, size), with the machine, the
compiler and the date; anything unmeasured marked (to measure). "None" if none. -->

## Test results

<!-- Per CI cell from the final run of this branch, with links. -->

| Cell | Result | Run |
|---|---|---|
| _Type tests (Linux)_ | _n / n_ | _link_ |

## Red first

<!-- The run that failed before the change made it pass: the command, its failing output, and the
commit it ran at. A test that records existing behaviour says so. -->

## Failures fixed

<!-- Every test or CI failure met while doing this work that needed a change: symptom, root cause,
fix, and why the fix is right. "None" if none. -->

| Cell / step | Symptom | Root cause | Fix | Commit |
|---|---|---|---|---|

## Consumer impact

<!-- What Iris-File-Extension and Iris-Slide must change, and the downstream cells that show each
builds against this change. A coordinated breaking change names each consumer's pull request and
the order of the merges. "None: no consumer compiles anything this changes" if so. -->

- Iris-File-Extension: _none / what changes, its pull request_
- Iris-Slide: _none / what changes, its pull request_
- Merge order: _none / e.g. this, then Iris-File-Extension#n, then Iris-Slide#n, together_

## Security impact

- New input parsed from a file or the network? _no / yes: where, and how every length is bounded_
- New or upgraded dependency? _no / yes: name, version, licence, pinned hash_
- New permission, secret, variable, environment or token? _no / yes: which, and why_
- Patient data, slide metadata, secrets or local paths anywhere in the diff or logs? _no_

## AI authorship

<!-- Required. Which parts an AI agent wrote, with which tool and model, and what the human did
(directed, reviewed line by line, rewrote). -->

- Written by: _the maintainer / an AI agent (tool, model) / both — which parts_

## Agent review

<!-- Owned by the review bot. Hand edits are overwritten on its next run. -->
<!-- review-bot:begin -->
- [ ] **Traceability** — work items named; ticks carry evidence and a link to the work commit
- [ ] **StyleGuide** — `development/style_guide.md` followed
- [ ] **MinimumSurface** — no near-duplicates, no second queue or buffer, nothing outside the vocabulary
- [ ] **TestsFirst** — new behaviour tested, shown red first; per-cell results; no test weakened
- [ ] **AbiAndConsumers** — consumer impact stated; downstream cells build; IFE's enum values kept
- [ ] **HeaderHygiene** — headers compile alone; no macros for constants; no reserved identifiers
- [ ] **PublicRepository** — nothing private named, no quotations, no local paths, no patient data
- [ ] **SecurityImpact** — section above filled in and matches the diff
- [ ] **Pipeline** — SHA pins, `permissions: {}`, untrusted input via `env:`, CMake scripts, bounded jobs
- [ ] **DataProtection** — no slides, patient data, secrets, keys or binaries
- [ ] **Documentation** — Mermaid diagrams match the code; decisions recorded (advisory)
- [ ] **AgentDisclosure** — AI authorship disclosed and specific

_Not yet reviewed._
<!-- review-bot:end -->
