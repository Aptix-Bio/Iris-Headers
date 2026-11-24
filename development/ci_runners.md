# CI Runners

How Iris-Headers uses CI runners (GitHub Actions on Blacksmith's runners).
Read it before writing or changing a workflow. Adopted 2026-11-24 from the
runner rules the maintainer keeps for the Iris repositories, so that the
review bot judges a workflow change against rules this repository carries. Runner minutes are billed and
have run out; a runner does only work whose result is wanted, and never the
same work twice. Modular reuse and frequent cache hits are the two levers.

## When a runner runs

- **Only where a result is wanted.** Pull requests that are not drafts,
  release-candidate tags, release tags, and by hand. Never on a branch push;
  never for docs-only changes. A new commit cancels its PR's running run.
- **Cheapest first.** Gate the costly OS (macOS bills 10x, Windows 2x) on the
  cheap ones passing (`needs:`), and gate a job on the jobs whose output it
  uses.
- **Prove it locally first.** A build or script runs locally, in a container
  for other OSes and architectures, before it spends a runner. Check that the
  local run can fail the way the runner would (an arm64 binary runs inside an
  "amd64" container on an Apple Silicon host); if it can't, assert the
  property directly. The container starts from what the runner image already
  has installed, not a bare image: a preinstalled host library a cross build
  can pick up by mistake fails only on the runner.

## Modular reuse

- **Workflows are modules, as code is.** One reusable workflow
  (`workflow_call`) per build, called once per OS; shared steps (fetching
  assets, signing in to a cloud) are a composite action every workflow calls.
  Never a second copy of steps that already exist.
- **Identical work runs once.** Work that doesn't depend on the platform (the
  assets, a toolchain archive covering every platform, generated data) is done
  by one job and handed on as an artifact, not repeated per platform.
- **Prebuilt over built.** Dependencies come prebuilt (apt, Homebrew, vcpkg
  binary archives); building one from source on a runner is a cache miss.

## Workflows are simple; CMake does the work

- **One workflow per purpose, few steps.** A `run:` step calls CMake: a
  configure, build or test command, or `cmake -P cmake/<name>.cmake`. Its
  logic lives in the script, which runs the same on every operating system's
  shell and locally, and is tested (`cmake/tests/`). Composite actions are for
  what needs an action (checkout, cache restore and save), with no shell logic
  beyond one call.
- **Every job on a pinned Blacksmith label.** `cmake/check_runner_labels.cmake`
  fails on any other label (BlacksmithRunners_1.4).
- **Settings are checked first.** A job that needs a repository variable or
  secret names each missing one in its first step
  (`cmake/require_settings.cmake`), before an action fails without saying
  which setting to create.

## Caches

- **Cache everything restorable**: package archives, compiler cache, fetched
  sources, toolchain and engine downloads, assets.
- **Keys come from content** (a manifest's hash, the build files' hash, the
  package manager's commit and triplet), with a `restore-keys` prefix so a
  partial hit beats a miss. A key names anything the tool's own hash can't see
  (a deployment target passed through the environment).
- **Restore first, save as soon as filled.** Restore every cache at the top
  of the job; save each one in the step right after the one that fills it,
  before anything that can fail (a compile, a test). Caches of partial work
  (the compiler cache, fetched sources) save with `always()`. A cache saved
  only at job end is lost by the first failing test.
- **Same keys everywhere.** Every workflow uses the same key for the same
  content, so a release reads what CI saved.
- **Mind the scopes.** A cache belongs to the ref that saved it: seed the
  default branch by hand so pull requests and tags can read it, and delete a
  closed pull request's caches (unreadable by anything else, they crowd the
  seeded ones out of the repository's limit).
- **Keep the environment stable.** A variable a compiler cache hashes changes
  every key: set it only on the step that needs it.

## Robustness

- **Pin what can move.** Runner images (`ubuntu-24.04`, not `-latest`), tool
  and engine versions, action versions.
- **Bound everything.** Every job has a timeout; parallel builds have a job
  count that fits the runner's memory.
- **Fail loudly.** Check that each output exists and is within limits; an
  exit code of 0 with nothing written is a failure.
