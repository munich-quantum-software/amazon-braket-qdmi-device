# Spec audits (SpecAudits)

This document defines how to audit this repository for *spec debt*: tests that
assert behavior nobody promised, and the production complexity those tests hold
in place. A SpecAudit looks backward after implementation. It separates
intentional contracts from choices that code and tests introduced together.

Treat the reader as a complete beginner to this repository. They have only the
current working tree, this file, and the audit being reviewed.

## What spec debt is

Requirements do not state every implementation detail. A contributor fills the
gaps, writes code, and tests the result. A passing test may then turn an
incidental choice into a permanent constraint. Later improvements appear to be
regressions even when no user or external specification required that choice.

Spec debt costs twice. The test suite grows without defending more intent, and
the implementation cannot be simplified because every accidental detail looks
like a contract.

Examples in this repository may include exact AWS request shapes that no public
contract exposes, ordering where QDMI promises only a set, mock call counts that
freeze an internal client boundary, and exhaustive error text where only the
QDMI status code is stable. These examples are search hints, not verdicts.

## How to use this framework

Read this file in full before starting an audit. Store one living audit per
scope at `.agent/audits/<scope-slug>.md`, based on `.agent/audits/TEMPLATE.md`.
Reconcile an existing audit instead of replacing its history.

A SpecAudit produces an assertion census, a spec ledger, and ranked verdicts
with reproducible evidence. It never applies its own verdicts. A maintainer
records a decision and reason first; production and test changes happen in later
pull requests.

The pre-release campaign proceeds in four stages:

1. Merge this framework without changing production sources or tests. Once the
   framework and pull request `#183` are on `main`, pin one exact `main` commit
   as the campaign baseline. It must descend from
   `8c2d17eaba1ee4dc87a8da7a751527c451da0086` and contain both changes.
2. Audit each of the seven scopes below in an independent pull request from the
   same baseline.
3. After all audits merge, run a fresh cross-scope architecture and release
   review.
4. After a maintainer decides every finding and records the reasons, use fresh
   agents to resolve accepted findings and reconcile the living audits.

## Non-negotiable requirements

- Audit assertions, not files or test functions. Give each assertion exactly one
  owning scope and one stable, baseline-specific ID.
- Every verdict must cite evidence from an executed experiment. An argument,
  code reading, search result, or provenance fact is not experiment evidence.
- Every verdict must name the promise it tested the assertion against, or state
  that no promise exists.
- Read every cited `file:line` at the audit's pinned full evidence SHA. Do not
  cite memory, a stale branch, or a search snippet.
- Never remove or weaken a test that reproduces a reported defect unless an
  experiment and contract analysis prove that the proposed replacement keeps the
  regression covered.
- Preserve assertions that defend an external contract, a safety property, or a
  resource-lifetime property unless equally strong evidence proves a safe
  replacement.
- Keep audit and remediation separate. An audit branch changes only its audit
  file. It does not change production code, tests, dependencies, or generated
  files.
- Make every audit self-contained. A reader with the audit file and a clean
  checkout must be able to reproduce each experiment.
- Abort on a dirty worktree or an evidence-SHA mismatch. Give each scope an
  immutable detached evidence worktree and a separate PR-authoring worktree. Use
  isolated build directories.
- The probe restores only edits and isolated state that it owns. After every
  command, verify that the evidence worktree is clean and still detached at the
  recorded evidence SHA. Discard and recreate it if either check fails.
- Default to offline evidence. Live AWS evidence requires a separate, explicit
  human approval for each batch and must follow the safeguards below.

## The unit of audit is the assertion

One test may contain several assertions with different purposes. A null-handle
check may defend the QDMI C ABI, an exact diagnostic may exceed that promise,
and a mock call count may only describe the current implementation. Judge them
separately, then group remedies by test because that is how a later patch is
written.

Use an ID of the following form:

```text
<scope-code>-<first-12-baseline-hex>-A<four-digit-sequence>
```

For example, the first distribution assertion at baseline `0123456789ab...` is
`DIST-0123456789ab-A0001`. Use these scope codes:

- `ABI` for `native-abi-and-session-lifecycle`;
- `META` for `device-capabilities-and-metadata`;
- `AWS` for `aws-auth-region-and-storage`;
- `TASK` for `quantum-task-jobs-and-results`;
- `DIST` for `distribution-catalog-and-python-shell`;
- `PL` for `pennylane-adapter`;
- `SPANK` for `spank-integration`.

An ID never changes during reconciliation. New assertions found at a later
baseline receive IDs with that baseline. If a mixed test belongs to several
scopes, divide it by assertion and record the other owners in each audit's
census.

Build the ledger before the census. The completed census has separate columns
for assertion ID, assertion and citation, owner, class, ledger IDs, and verdict
or anchor record. It contains no `Pending` values. Use `None` when no rung 1, 2,
or 3 ledger entry exists, and name the verdict or anchor section that carries
the evidence for every assertion.

## Audit scopes

Every assertion belongs to exactly one of these scopes. Resolve an ownership
conflict before prosecution begins.

### `native-abi-and-session-lifecycle`

Owns QDMI C entry points, initialization and finalization, handle ownership,
session state, null and size-query behavior, exception containment, concurrency,
and status-code mapping.

### `device-capabilities-and-metadata`

Owns capability parsing, catalogue semantics, sites, operations, connectivity,
calibration, native and supported gates, device status, queue metadata, and QPU
or simulator caching.

### `aws-auth-region-and-storage`

Owns credential-provider lifetime, ARN and Region handling, Braket, S3, and STS
clients, IAM-safe errors, reservation propagation, S3 precedence, and standard
regional bucket behavior.

### `quantum-task-jobs-and-results`

Owns job creation and retrieval, job parameters, submission immutability,
QuantumTask identity and states, cancellation, checking, waiting, queue
position, result validation, histograms, and shot ordering.

### `distribution-catalog-and-python-shell`

Owns CMake install and export behavior, generated headers and catalogue, library
and wheel layout, relocation, Python artifact discovery, the CLI, MQT Core
registration, entry points, and package metadata.

### `pennylane-adapter`

Owns lazy and concrete PennyLane entry points, catalogue and session mapping,
override precedence, QDMI translation, shots and wires, QAOA behavior, and
deferred QPU-compilation claims.

### `spank-integration`

Owns the Slurm ABI, licence applicability, environment precedence, credential
references, failure behavior, container integration, installation, and the
GPL-3.0-or-later boundary.

Documentation is normally a published promise source for these scopes. CI,
documentation consistency, and release configuration are examined in the later
cross-scope review unless they directly assert behavior owned above.

Use `distribution-catalog-and-python-shell` as the pilot. Run the other scopes
from the same baseline. Do not replace these bounded audits with one
whole-repository prosecution.

## The spec ladder

Ask one question of every assertion:
**which promise does this defend, and who made it?** Rank sources as follows,
strongest first.

1. **External and machine-checked.** A specification this repository does not
   own, or one a tool verifies. Examples include the QDMI revision pinned by
   `cmake/ExternalDependencies.cmake`, the QDMI C ABI, AWS service API
   contracts, Slurm's SPANK ABI, package metadata schemas, CMake package rules,
   and the OpenQASM formats advertised through QDMI.
2. **Published.** A promise this project made to users in `README.md`,
   `CHANGELOG.md`, `docs/`, public headers, installed CMake metadata, the device
   catalogue, Python exports, CLI help, or registered entry points.
3. **Requested.** A human-authored issue, discussion, or review comment that
   caused the behavior. Record the author's actual request, not a later summary
   of the implementation.
4. **Recorded rationale.** A commit message, pull-request description, audit, or
   retrospective that explains what was implemented. This is useful context, but
   a detail recorded after implementation is not automatically a promise. An
   AI-authored summary never becomes rung 3 without an underlying human-authored
   request.
5. **Implemented.** The code behaves this way.
6. **Asserted.** A test says so.

Rungs 5 and 6 are not promises. Rung 4 may identify intent but cannot anchor a
verdict by itself. An anchored assertion must trace to rung 1, 2, or 3.

Build the **spec ledger** before reading tests. Number promises `S1`, `S2`, and
so on, and record the promise, rung, exact citation, and affected surface. The
test-blind rule matters: reading tests first lets their assumptions contaminate
the ledger.

For the initial campaign, requested-behavior cartographers should inspect the
human-authored issues and review discussions around pull requests `#147`,
`#148`, `#150`, `#156` through `#160`, `#166` through `#168`, and `#171` through
`#176`. Verify the live source and author before treating text as rung
3. Pull-request summaries and retrospective text are not substitutes.

## Verdict classes

Give every audited assertion exactly one class.

**Anchored.** It defends a rung 1, 2, or 3 ledger entry without pinning more
than the promise states. Keep it and record why. A mostly anchored scope is a
successful result.

**Over-specified.** It defends a real promise but constrains more than the
promise states. Narrow it later; do not delete it. For example, keep a QDMI
status code but relax incidental wording, or compare documented result fields
instead of an entire serialized AWS request.

**Redundant.** Another assertion covers the same equivalence class and catches
the same contract-breaking fault. Merge or parametrize it later. Similar source
coverage alone does not prove redundancy.

**Contract-free.** No rung 1, 2, or 3 source promises the behavior. Delete the
assertion only after maintainer acceptance, then examine code that exists solely
to produce that behavior as a removal candidate.

**Coverage-driven.** The assertion reaches code that the public interface cannot
reach, only to satisfy a coverage gate. The later remedy is usually to remove
both the unreachable branch and its manufactured test, or to use a justified
coverage exclusion.

## Provenance signals

History points to candidates but never decides a verdict. Keep provenance and
contract analysis separate. Co-introduction of implementation and tests is
normal and weak evidence on its own.

Use these signals in decreasing order of value:

1. The assertion changed in the same commit as the behavior it asserts, and the
   new assertion became tighter than the previous one.
2. `git log -S '<assertion text>'` finds only its introduction; it was born
   green and never changed after a failure.
3. The commit records AI assistance, while no human-authored request states the
   exact behavior.
4. Test and code arrived together without a linked request for the asserted
   detail.

A provenance agent reports commits, authorship facts, and source text. It does
not assign verdicts.

## Smell catalogue

These patterns start the census. They do not establish guilt.

| Smell                   | Search or symptom                                        |
| :---------------------- | :------------------------------------------------------- |
| Exact C++ diagnostic    | `EXPECT_STREQ`, exact `message`, or full AWS text        |
| Exact Python diagnostic | `pytest.raises` with a long `match=`                     |
| Internal surface        | private Python names or internal C++ helpers in tests    |
| Mock mechanism          | exact calls, call counts, or call ordering               |
| Incidental order        | exact sites, operations, histogram, or map order         |
| Golden object           | equality on a complete AWS request or JSON document      |
| Vacuous assertion       | non-null or non-empty as the only observable result      |
| Impossible input        | forged handle, enum, or state the public API cannot make |
| Duplicated matrix       | cases indistinguishable to the code under test           |
| Live-only oracle        | a paid task used where an offline boundary can prove it  |

Exact error wording is rarely a contract. The error domain, QDMI status code,
exception type within Python, and safe handling at the C boundary may be.

Tests of private symbols or mock calls often freeze implementation seams. Prefer
observable results through the supported surface unless the seam itself is
published or required for safety.

Ordering is a contract only when a rung 1, 2, or 3 source says so. Pay special
attention to operation lists, site pairs, queues, histograms, and filesystem or
catalogue discovery.

Whole AWS request objects and JSON documents can turn irrelevant defaults into
contracts. Assert only fields whose service or project contract matters.

Parameter matrices need one case per meaningful equivalence class. Region,
device ARN, client Region, S3 result location, and reservation ARN are distinct
inputs and must not be collapsed merely because current fixtures share values.

## Anchors that must survive

The failure mode of an aggressive audit is over-deletion. Preserve and record an
assertion when any of these applies:

- It reproduces a reported defect. Find and cite the issue or fix commit.
- It enforces the QDMI C ABI, including size queries, handle validation,
  status-code mapping, null handling, and the rule that C++ exceptions do not
  cross the C boundary.
- It protects a contract with AWS, Amazon Braket, S3, STS, OpenQASM, MQT Core,
  PennyLane, CMake, Python packaging, Slurm, or another external consumer.
- It guards credential secrecy, session isolation, credential-provider refresh,
  mutex protection, process-wide AWS SDK lifetime, cleanup, cancellation, a
  timeout, or another safety and resource property.
- It is the only assertion that fails under a fault that violates a rung 1, 2,
  or 3 promise.

The stable `AMAZON_BRAKET_` symbol prefix and `amazon.braket.default` device ID
are published compatibility boundaries. Changing either is remediation with a
separate compatibility decision, never incidental audit cleanup.

## Evidence protocol

**No verdict without an executed experiment.** Escalate through the tiers and
stop when the evidence settles the question.

For the first audit, the exact evidence SHA `E` equals campaign baseline `B`.
Keep one evidence worktree detached at `E`; never commit, rebase, or change its
base. "Immutable" means its recorded base and tracked state never persistently
change. The probe may make a temporary edit that it owns, but it must restore
that edit before returning. Write the audit only in the separate authoring
worktree.

After every evidence command, including a failed or interrupted command, verify
all three conditions:

```sh
test -z "$(git status --porcelain=v1 --untracked-files=all)"
test "$(git rev-parse HEAD)" = "${E}"
test -z "$(git symbolic-ref --quiet HEAD)"
```

If a supplied build or test command changes anything outside the probe-owned
edit or isolated output, the probe cannot restore it. Treat the evidence as
invalid, discard the evidence worktree, and recreate it detached at `E`.

### T1: coverage delta

Measure coverage for the source in scope. Temporarily remove or narrow the
candidate assertion, run the same scope command, and measure again.

The standard C++ path is an isolated Debug build with `ENABLE_COVERAGE=ON`,
`BUILD_AMAZON_BRAKET_TESTS=ON`, and `BUILD_AMAZON_BRAKET_LIVE_TESTS=OFF`. Run
the relevant offline GoogleTest or CTest selection.

Run Python through the supported nox installation path:

```sh
uvx nox -s tests -- <pytest-path-or--k-expression>
```

Do not use a bare `uv run pytest` against an environment where the compiled
package was not installed. Run SPANK evidence through its isolated Docker test
suite.

No coverage change is evidence of possible redundancy, not proof. An assertion
may duplicate line coverage while remaining the only useful oracle.

### T2: fault injection

Restore the assertion, inject one fault into the behavior it claims to guard,
and run the complete offline suite for the scope.

- If only the accused assertion fails, it is the sole oracle for that mutant. If
  a rung 1, 2, or 3 promise covers the behavior, keep it as a sole anchor. If no
  such promise exists, the result may support a contract-free verdict.
- If another assertion fails for the same equivalence class, the candidate may
  be redundant. Name every covering assertion, prove the equivalence, and repeat
  the fault with the accused assertion removed or narrowed to show that the
  surviving oracle still catches it.
- If no assertion fails, the mutant did not exercise or prove the accused
  assertion. The experiment is non-adjudicable. Choose a fault that the accused
  assertion detects or record that no verdict was established.

A contract-free verdict needs both parts: no rung 1, 2, or 3 promise and an
executed experiment showing that the accused assertion is the only oracle for
the unpromised behavior, or equally strong evidence with the same conclusion. A
surviving mutant that no assertion detects proves neither contract freedom nor
redundancy.

Restore the fault and perform the post-command clean-tree checks before
continuing. A passing suite with a mutation still present invalidates later
evidence.

### T3: scoped mutation

T3 is a selected set of repeated T2 fault-injection experiments, not a separate
mutation engine. Use it only for high-value findings. Select several independent
faults in the one translation unit, Python module, or SPANK unit under audit.

For each fault, create two disposable detached worktrees at the same immutable
evidence SHA `E`. In the first, apply only the fault and run the fixed scope
command with the current assertion. In the second, apply the identical fault and
the proposed assertion change, then run the same command. Do not reuse a
worktree for another mutant.

Record the exact mutant, commands, exit statuses, and complete killed or
surviving-test sets for both sides. Compare those sets in the audit. Verify each
worktree clean after restoration, then remove it. Do not add a mutation-testing
dependency, and do not represent a single T2 run as T3 evidence.

### The probe

`.agent/audit-probe.sh` standardizes individual T1 and T2 experiments. It
requires an expected full evidence SHA and a clean, detached evidence worktree.
It uses isolated state and restores the temporary edit it owns on success,
failure, signal, and timeout. Read `./.agent/audit-probe.sh --help` before use.

The probe has no T3 mode and no option to retain mutations. Build T3 evidence
from the paired, isolated T2 runs above. Manual experiments must provide the
same SHA check, isolation, command consistency, and post-command clean-tree
proof.

## AWS evidence safeguards

Offline evidence is the default. The presence of credentials never authorizes
live access. The probe must refuse AWS credentials and live-test variables
unless the operator selected explicit live mode and supplied a non-secret batch
identifier.

Every live batch requires fresh human approval. Before it runs, record its
purpose, Region, exact test filter, maximum task count, maximum shots per task,
expected external state, and stop conditions. Run under `set +x`. Obtain
temporary credentials through the AWS SDK provider chain or a process-local
export. Never print or commit credentials, account IDs, bucket names, task ARNs,
reservation ARNs, private device data, or other identifiers.

Use explicit Regions. Treat device ARN Region, client Region, S3 result
location, and reservation ARN as distinct inputs. Use only the standard regional
Amazon Braket bucket selected by the human. Do not delete a bucket or other
external state after a test unless that separate destructive action was
explicitly authorized.

The campaign divides live evidence into these approval gates:

1. Identity and permission preflight. STS and required permission checks only;
   no printed identity and no state change.
2. Read-only catalogue and metadata. Query the nine catalogue devices and the
   SV1 and IQM metadata fixtures in their declared Regions. Submit no tasks.
3. Standard-bucket behavior. Unset the result URI and run only
   `AmazonBraketQDMILiveTest.UsesAutomaticDefaultS3Destination` in `us-east-1`.
   Submit at most one one-shot SV1 task.
4. C++ task and results. Use a campaign-specific prefix in the selected bucket.
   Submit at most three SV1 tasks with no more than 100 shots each.
5. PennyLane. Use the existing protected, x86-64 CI lane after confirming its
   bucket configuration. Submit at most twenty SV1 tasks. Never bypass the
   architecture guard from an ARM64 workstation.

Never submit a QPU task during the audit campaign. Stop immediately when a task
or shot cap may be exceeded, the configured Region differs from the approved
one, a requested device is not SV1, or output may expose a protected identifier.
Record only task counts, elapsed time, sanitized outcomes, and restoration
status in the audit.

## Unlock analysis

For every assertion proposed for narrowing or removal, ask:
**what does this now permit?** An unlock must name the lifted constraint and the
production change that becomes legal.

Look for unreachable defensive branches, parameters with one real value,
interfaces with one implementation, test-only seams, duplicated validation,
unnecessary ordering, defensive copies, repeated AWS calls, and cache or
bookkeeping work observable only through mocks.

Run exactly one architecture-altitude pass per scope. Ignore individual
assertions and ask whether a module, layer, cache, or client boundary exists
only because tests imposed it.

Do not claim ordinary cleanup as an unlock. Name the assertion that must change
first. If there is none, record the observation under "Found along the way, not
blocked by an assertion."

## Agent roster and isolation

A SpecAudit is adversarial. Role isolation is part of the method; parallelism
only reduces elapsed time. The scope lead coordinates and adjudicates but never
acts as a prosecutor or defender.

Maintain an uncommitted orchestrator registry containing the scope, baseline,
source symbols, tests and assertion IDs, dependencies, branch, worktree, lead
agent, pull request, head SHA, review cursor, CI status, approved live batches,
and current state. Never put local paths, agent IDs, credentials, or private AWS
identifiers in repository files.

With four total agent slots, keep the root as orchestrator. Create persistent
scope leads in batches of three and leave inactive leads idle. Audit one scope
intensively with the root, its lead, and two isolated role slots. Keep a lead
available until its audit pull request merges so review feedback returns to the
same context.

Run these waves in order:

1. **Spec cartographers.** Four read-only, test-blind roles cover external and
   machine-checked sources, published promises, requested behavior, and the
   declared public surface. Run them in two batches when slots are limited.
2. **Census.** One neutral role enumerates every assertion in scope and assigns
   stable IDs without judging them.
3. **Prosecutors.** Give independent roles clusters of roughly twenty tests, the
   ledger, and production code. They cite a promise for each assertion or accuse
   it as unanchored. Prosecutors do not see one another's work.
4. **Provenance.** Read-only roles report commits and history facts for suspect
   assertions. They do not recommend verdicts.
5. **Defenders.** Fresh, blind roles receive the accused assertions but not the
   prosecution reasoning. They seek missed promises, regressions, consumers, and
   safety properties.
6. **Executor.** One serialized role runs T1 and T2, then selected paired T2
   worktree comparisons for T3. Never run concurrent probes in one worktree.
7. **Unlock analysts.** Fresh roles analyze confirmed findings, including one
   scope-level architecture-altitude pass.
8. **Red team.** Two fresh roles identify the verdict most likely to break a
   real consumer and rank residual risk.
9. **Adjudication.** The scope lead weighs the ledger and executed evidence and
   writes the audit. Argument alone cannot become a verdict.

If subagents are unavailable, use separate fresh sessions and pass only the
inputs for that wave. Never let one context prosecute, defend, and adjudicate
the same assertion.

## Guardrails

1. Never delete a test that reproduces a reported defect.
2. Narrow before you merge; merge before you delete. First remove constraints
   that exceed the promise. Then combine genuinely equivalent coverage. Delete
   only after the surviving oracle has been proved.
3. No verdict is valid without an executed experiment.
4. No role both prosecutes and adjudicates an assertion.
5. An audit records findings and stops. It does not apply them.
6. A dirty worktree, attached evidence branch, or unpinned evidence SHA aborts
   the run.
7. Assertion changes and the production changes they unlock use separate commits
   during remediation.
8. Audit one bounded scope at a time.
9. State every coverage consequence plainly. Record line and branch deltas and
   any effect on project or patch coverage; never hide a decrease behind an
   exclusion.

## Audit pull-request lifecycle

Once the foundation and pull request `#183` are both on `main`, record their
full merge SHAs. Pin `B` to one exact full `main` SHA that contains both and for
which this command succeeds:

```sh
git merge-base --is-ancestor \
  8c2d17eaba1ee4dc87a8da7a751527c451da0086 "${B}"
```

For every scope, create two worktrees from `B`:

- an evidence worktree detached at exact evidence SHA `E=B`, with no branch,
  commits, or persistent tracked edits; and
- a PR-authoring worktree on `codex/audit-<scope-slug>`, used only to edit
  `.agent/audits/<scope-slug>.md`.

Before every PR update, fetch `origin/main` and record its exact full SHA as
`M`. Compare `B..M` across the scope's production sources, tests, and promise
sources. If relevant content drifted, keep the original evidence unchanged,
create a fresh detached evidence worktree at `E=M`, re-read affected citations
there, rerun affected experiments, and append a dated revalidation record. If no
relevant content drifted, record `M` and the exact diff paths checked. A rebase
and evidence from a PR head do not satisfy this rule.

Run the scope evidence and `uvx nox -s lint`. Inspect the diff and worktree.
Sign each commit and run `git verify-commit HEAD`. After explicit authorization,
open an independent draft pull request targeting `main`; do not stack audit pull
requests. Include the required AI disclosure, assign `@burgholzer`, and apply
`documentation` plus `c++` or `python` where applicable. Use the existing
pull-request template without adding checklist items. Follow `AGENTS.md` for
remote-state authorization.

Do not rewrite a remote audit branch by default. If a rewrite becomes necessary,
fetch that branch immediately before the push and record its full remote SHA as
`REMOTE_HEAD`. Create and verify a recoverable local backup ref at that SHA.
Then obtain fresh human authorization for that exact branch and rewrite. Use an
exact lease, never a branch-only or implicit lease:

```sh
REMOTE_REF=refs/heads/codex/audit-<scope-slug>
REMOTE_HEAD=<full-SHA-from-the-fresh-fetch>
BACKUP_REF=refs/codex-backups/audit-<scope-slug>/<timestamp>
git update-ref "${BACKUP_REF}" "${REMOTE_HEAD}"
git show-ref --verify "${BACKUP_REF}"
git push \
  --force-with-lease="${REMOTE_REF}:${REMOTE_HEAD}" \
  origin "HEAD:${REMOTE_REF}"
```

If the fetched SHA changes before authorization or push, stop, refresh the
backup and lease, and request authorization again.

Route review feedback to the original scope lead. A defender, red-team role, or
fresh agent adjudicates substantive objections. The prosecutor does not defend
its own finding. The maintainer resolves review threads and accepts the audit
record before merge. Merging an audit record does not accept its findings.

## Cross-scope architecture review

After all seven audits merge, use entirely fresh agents to trace:

- QDMI ABI through device, session, and job implementation to AWS services;
- catalogue and install layout through Python discovery and MQT Core to
  PennyLane;
- Slurm and SPANK configuration through environment transport to the native
  provider; and
- documentation and CI promises through executable configuration to release
  artifacts.

Store the review at `.agent/reviews/pre-release-architecture.md`. Architecture
findings are not assertion verdicts. Give each one a stable ID of the form
`ARCH-<first-12-B-hex>-F<four-digit-sequence>`. For each finding, record
severity, producer, consumers, broken or duplicated contract, evidence, proposed
ownership boundary, dependencies, release impact, maintainer decision, decision
reason, resolution state, and closure evidence.

Ask whether each proposed fix is a solution at the correct ownership boundary or
a workaround for a design problem elsewhere in the stack. Produce the dependency
graph that later orders remediation.

## Reconciliation and remediation

Every verdict and architecture finding starts with maintainer decision `Pending`
and resolution state `Not started`. After the audit and architecture records
merge, a maintainer chooses exactly one decision and records a reason:

- `Accepted`: remediation is authorized. Its resolution may become `Applied`,
  `Narrowed`, or `Superseded` only after evidence is merged and reconciled.
- `Rejected`: no remediation follows. Set resolution to `Not applicable` and
  preserve the reason.
- `Deferred`: remediation is postponed. Keep resolution `Not started`, record
  the remaining risk and release effect, and preserve the reason.

Decision and resolution are distinct. Never encode `Deferred` as a resolution
state or treat a merged audit as an `Accepted` decision.

A rejected finding closes when the decision and reason are recorded. An accepted
finding closes when its resolving change is merged, its resolution state and
link are recorded, and its original experiment is rerun or superseded by equally
strong evidence at an exact `main` SHA. `Narrowed` closes only when the
maintainer records that the narrowed result fully satisfies the accepted part;
otherwise the remainder stays open. `Superseded` closes only when an executed
experiment proves another merged change removed the concern. A deferred finding
remains open in the living record, even when its documented low risk permits
this release.

Do not remediate until all audit and architecture pull requests merge and every
finding has an `Accepted`, `Rejected`, or `Deferred` decision with a reason.
Fresh resolution agents receive only current `main`, the merged records,
accepted verdict and architecture IDs, and the repository instructions.

Group accepted findings by coherent ownership and file overlap. Serialize the
overlapping native sequence: ABI and session, capabilities, AWS and storage,
then jobs and results. Order distribution, PennyLane, and SPANK work from the
architecture dependency graph.

Keep separate signed commits for assertion narrowing or removal, production
simplification, and audit, documentation, or changelog reconciliation. Preserve
those commits with merge commits. Regenerate `uv.lock` from declarations when a
dependency changes.

Re-derive each living audit against an exact current `main` SHA. Preserve its
original IDs, evidence, decision, and reason. Append the resolution state,
resolving pull request or commit, revalidation evidence, and closure result. Do
not rewrite old experiments. A partial application narrows a finding; it does
not close the remainder without the maintainer decision described above.

Run targeted offline tests first, then every affected offline C++, Python,
documentation, SPANK, and lint suite. Run live evidence only when an accepted
finding requires it and its batch has fresh approval.

No audit assumes a public API change. A change to the QDMI C ABI, stable device
ID, catalogue entries, custom parameters, Python exports, or PennyLane entry
points requires separate compatibility review, changelog treatment, and a
migration plan.

The v1.1.0 release gate closes only when all of these are true:

- Every assertion ID has exactly one owning scope, every census has no pending
  entries, and all seven audit records plus the architecture review are merged.
- Every verdict and architecture finding has an `Accepted`, `Rejected`, or
  `Deferred` decision and a maintainer-authored reason.
- Every accepted correctness, contract, ABI, safety, security,
  resource-lifetime, and architecture-blocking finding is closed by the rules
  above. No accepted release blocker remains unresolved.
- Every deferred item is low risk for this release and records its rationale,
  residual risk, owner, and explicit non-blocking release decision.
- The MQT Core VCS dependency is replaced by the released minimum version,
  `uv.lock` is regenerated from the declarations, and neither declaration nor
  lockfile retains the temporary Git source.
- One exact final release-candidate SHA is recorded. The complete offline C++
  suite, supported Python and minimum-dependency sessions, SPANK Docker suite,
  documentation build, and `uvx nox -s lint` pass at that SHA.
- Every required live-AWS batch has fresh approval and passes at the same final
  SHA within its Region, task, and shot caps. Batches that are not required are
  recorded as not run, never implied to have passed.
- All required CI checks pass for the final SHA, and the release artifacts they
  validate correspond to that SHA.
- Version metadata, release documentation, and both changelog surfaces agree on
  v1.1.0 and contain the accepted user-facing and compatibility changes.

## Repository requirements

- Use the build and test entry points in `AGENTS.md` and the probe. Apply the
  same command to every candidate within a scope.
- Run `uvx nox -s lint` after each completed audit-file batch.
- Wrap prose at 80 columns. Keep pull-request references in backticks so a
  wrapped `#123` cannot become a Markdown heading.
- Use short, direct sentences and established project terms.
- Do not record local paths, account names, branch-specific machine details,
  credentials, or private AWS identifiers in an audit.
- Do not edit generated or externally templated files.
