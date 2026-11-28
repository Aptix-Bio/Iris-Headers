<!--
Iris-Headers pull request. CONTRIBUTING.md gives the rules it is held to; the decisions are in
development/ARCHITECTURE.md (§4) and the ID scheme in development/README.md. Write "None" in a
section that does not apply; never delete one. The review bot reads this description as data and
checks it against the diff (.github/review/rubric.toml).

"Agent review" at the bottom is not yours to tick: the review bot rewrites it on every run. A pull
request from a fork gets no model review; a maintainer reviews it.
Branch name: <kind>/<WorkItemName>, e.g. feature/StrongIndices_4.2.
-->

## Summary

<!-- Two or three sentences: what this delivers, and why. -->

## WBS

<!-- Name the work items this change completes or advances, spelled out, with the evidence for
each; the ticks, each linking the commit that did the work, are kept privately with the work
breakdown, and the review bot checks every link. -->

- **_WorkPackage_n.n_**: _what this change does for it_ — _evidence_

## RTM

<!-- Name the requirement rows this change adds or moves, with the evidence; the rows' status, each
linking the commit that did the work, is kept privately with the work breakdown. A mistake that
reached main, a consumer or a measurement is named under "Failures fixed". "None" if none. -->

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
- [ ] **StyleGuide** — the style guide and CONTRIBUTING.md followed
- [ ] **MinimumSurface** — no near-duplicates, no second queue or buffer, nothing outside the vocabulary
- [ ] **TestsFirst** — new behaviour tested, shown red first; per-cell results; no test weakened
- [ ] **AbiAndConsumers** — consumer impact stated; downstream cells build; IFE's enum values kept
- [ ] **PublicRepository** — nothing private named, no quotations, no local paths, no patient data
- [ ] **SecurityImpact** — section above filled in and matches the diff
- [ ] **Pipeline** — SHA pins, `permissions: {}`, untrusted input via `env:`, CMake scripts, bounded jobs
- [ ] **DataProtection** — no slides, patient data, secrets, keys or binaries
- [ ] **Documentation** — Mermaid diagrams match the code; decisions recorded (advisory)
- [ ] **AgentDisclosure** — AI authorship disclosed and specific
- [ ] **HeadersVocabularyScope** — each added or changed declaration used by two or more repositories, uses named
- [ ] **HeadersPrimitivesOnly** — the inline code that makes the vocabulary usable, no single repository's algorithm
- [ ] **HeadersAbiStability** — layouts, values and signatures stable; headers standalone, portable, no macro constants or reserved identifiers

_Not yet reviewed._
<!-- review-bot:end -->
