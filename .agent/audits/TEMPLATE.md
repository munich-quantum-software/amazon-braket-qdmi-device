# SpecAudit: `<scope-slug>`

<!--
Copy this file to .agent/audits/<scope-slug>.md. Remove all instructional
comments before review. Wrap prose at 80 columns. Do not record local paths,
agent IDs, credentials, account IDs, bucket names, task ARNs, reservation ARNs,
or private device details.
-->

## Audit identity

- **Scope:** `<one of the seven scopes in .agent/AUDITS.md>`
- **Scope code:** `<ABI|META|AWS|TASK|DIST|PL|SPANK>`
- **Campaign baseline B:** `<full 40-character main commit SHA>`
- **Evidence SHA E:** `<full 40-character detached main commit SHA>`
- **Audit date:** `<YYYY-MM-DD>`
- **Source boundary:** `<production paths and public surfaces>`
- **Test boundary:** `<test paths and exact filters>`
- **Promise boundary:** `<external, published, and requested sources searched>`

Every `file:line` citation below was read in the detached evidence worktree at
`E`. The PR-authoring worktree changes only this audit file.

## Scope and ownership

<!--
Describe what this audit owns and what adjacent audits own. Mixed tests are
divided by assertion. Resolve every ownership conflict before prosecution.
-->

## Spec ledger

| ID   | Promise               | Rung      | Evidence citation                       | Affected surface    |
| :--- | :-------------------- | :-------- | :-------------------------------------- | :------------------ |
| `S1` | `<who promises what>` | `<1|2|3>` | `<source:line or linked human request>` | `<API or behavior>` |

<!--
Build this ledger before reading tests. Rung 4 rationale may be recorded in
notes, but it cannot anchor an assertion without a rung 1, 2, or 3 source.
State which expected sources were searched when no promise was found.
-->

## Assertion census

| Assertion ID                | Assertion and evidence citation    | Owner     | Class     | Ledger IDs     | Verdict or anchor |
| :-------------------------- | :--------------------------------- | :-------- | :-------- | :------------- | :---------------- |
| `<CODE>-<baseline12>-A0001` | `<file:line and observable claim>` | `<scope>` | `<class>` | `<S1,S2|None>` | `<V1|Anchor 1>`   |

<!--
Build this only after the test-blind ledger. List every assertion in the test
boundary, including anchors. An ID never changes during reconciliation. Give
newly discovered assertions an ID from the baseline where they were first
audited. At completion, every row has exactly one owner and class, ledger IDs or
None, and a verdict or anchor record. No cell may say Pending.
-->

## Summary

<!-- Rank by complexity removed per unit of risk, not by assertion count. -->

| Verdict | Assertion IDs | Class     | Remedy                             | Tier      | Unlock                      | Risk                     | Decision  | Resolution    |
| :------ | :------------ | :-------- | :--------------------------------- | :-------- | :-------------------------- | :----------------------- | :-------- | :------------ |
| `V1`    | `<IDs>`       | `<class>` | `<keep, narrow, merge, or delete>` | `<T1-T3>` | `<specific change or none>` | `<low, medium, or high>` | `Pending` | `Not started` |

## Verdicts

### V1. `<one-line claim>` - `<verdict class>`

**Assertions.** `<stable IDs and exact evidence-SHA file:line citations>`

**Promise.** `<ledger IDs, or "No rung 1, 2, or 3 promise found">`

**Provenance.** `<commits and history facts; do not use them as the verdict>`

**Experiment.** `<temporary assertion change or injected fault>`

```text
command             : <rerunnable sanitized command>
campaign baseline B : <full SHA>
evidence SHA E      : <full SHA>
test selection       : <scope selection>
expected observation : <what would settle the question>
observed failures    : <test IDs or none>
accused assertion    : <failed or passed>
same-class oracles   : <assertion IDs or none>
coverage before      : <line and branch data or not measured>
coverage after       : <line and branch data or not measured>
probe-owned edit     : restored
post-command status  : clean, detached at E
```

**T3 comparison.** `<Paired comparison evidence, or Not run.>`

<!-- For each selected mutant, cite the paired disposable worktrees at E, the
identical fault, both assertion commands and exit statuses, and the killed or
surviving tests. Write Not run for a T1 or T2-only verdict. -->

**Adjudication.** `<Oracle result and evidence-backed conclusion.>`

<!-- Record sole oracle, equivalent surviving oracle, or non-adjudicable. If no
assertion failed, state that the mutant proved no verdict. A contract-free
result also requires no rung 1, 2, or 3 promise. -->

**Remedy.** `<later test change; no implementation in this audit>`

**Unlock.**
`<assertion constraint lifted and production change enabled, or none>`

**Risk and release impact.**
`<consumer risk, confidence, and whether it can block release>`

**Maintainer decision.** `Pending`

**Decision reason.** `Pending`

**Resolution state.** `Not started`

**Closure evidence.** `Open; no resolving change.`

<!--
After this audit merges, a maintainer changes the decision to Accepted,
Rejected, or Deferred and writes a reason. Decision and resolution are separate.
Only an accepted finding may later become Applied, Narrowed, or Superseded.
Preserve the original evidence and append the resolving change, revalidation,
and closure result.
-->

## Anchors confirmed

### `<assertion IDs>` - `<promise defended>`

- **Promise:** `<ledger IDs>`
- **Fault:** `<temporary fault injected>`
- **Evidence:** `<rerunnable command and observed failing assertions>`
- **Why it stays:** `<regression, external contract, safety, or sole oracle>`

## Deliberately not touched

### `<item>`

`<What it is, which scope owns it if any, and why this audit left it alone.>`

## Evidence record

### Environment

- **Campaign baseline B:** `<full SHA>`
- **Evidence SHA E:** `<full exact main SHA>`
- **Evidence worktree:** `detached at E; no branch or commits`
- **Authoring worktree:** `<audit branch; audit file only>`
- **Platform:** `<portable platform facts; no local paths or account data>`
- **Build mode:** `<C++ Debug coverage, nox session, or SPANK Docker>`
- **AWS mode:** `offline`

### Commands

```sh
<all rerunnable commands, including the exact probe and test filters>
```

### Live evidence

`Not run.`

<!--
If live evidence was approved, replace "Not run" with the batch identifier,
approval reference, Region, exact test selection, task and shot caps, sanitized
task count, elapsed time, outcome, and external state created. Never include
credentials or resource identifiers. One approval covers one batch only.
-->

### Restoration

The probe-owned edit was restored. After every command, these checks passed:

```sh
test -z "$(git status --porcelain=v1 --untracked-files=all)"
test "$(git rev-parse HEAD)" = "${E}"
test -z "$(git symbolic-ref --quiet HEAD)"
```

`<Record discarded worktrees or command-created state that invalidated a run.>`

### Drift and revalidation

- **Exact current main SHA M:** `<full SHA>`
- **Diff:** `<exact B..M command and scope paths>`
- **Relevant drift:** `<none, or affected sources, tests, and promises>`
- **Revalidation:** `<not required, or appended evidence at E=M>`

<!-- Preserve every original experiment. Append new citations and experiments
at M; never rewrite evidence from B. -->

## Residual risk

- `<Remaining risk.>`

<!-- Include unexecuted experiments, uncertain consumers, and untested live
behavior. Distinguish severity from confidence. -->

## Found along the way, not blocked by an assertion

- `<Ordinary cleanup or architecture observation.>`

<!-- Name no unlock unless an assertion must change first. -->

## Reconciliation

| Date           | Exact main SHA | Finding | Decision                       | Decision reason       | Resolution                                                 | Resolving change, revalidation, and closure |
| :------------- | :------------- | :------ | :----------------------------- | :-------------------- | :--------------------------------------------------------- | :------------------------------------------ |
| `<YYYY-MM-DD>` | `<full SHA>`   | `<V1>`  | `<Accepted|Rejected|Deferred>` | `<maintainer reason>` | `<Not started|Applied|Narrowed|Superseded|Not applicable>` | `<link, evidence, and open or closed>`      |

## Progress

- [ ] (`<YYYY-MM-DD>`) Scope and ownership fixed at the pinned baseline.
- [ ] (`<YYYY-MM-DD>`) Test-blind spec ledger completed by four source classes.
- [ ] (`<YYYY-MM-DD>`) Assertion census completed with stable IDs.
- [ ] (`<YYYY-MM-DD>`) Prosecution and provenance completed.
- [ ] (`<YYYY-MM-DD>`) Fresh defenders reviewed every accused assertion.
- [ ] (`<YYYY-MM-DD>`) T1 and T2 experiments completed.
- [ ] (`<YYYY-MM-DD>`) Selected T3 mutants compared through paired T2 runs in
      separate disposable worktrees at E.
- [ ] (`<YYYY-MM-DD>`) Unlock and architecture-altitude analyses completed.
- [ ] (`<YYYY-MM-DD>`) Two fresh red-team reviews completed.
- [ ] (`<YYYY-MM-DD>`) Scope lead adjudicated the evidence.
- [ ] (`<YYYY-MM-DD>`) Census has one owner, class, ledger result, and verdict
      or anchor for every assertion, with no pending entries.
- [ ] (`<YYYY-MM-DD>`) Drift checked against an exact current `main` SHA and
      original evidence preserved.
- [ ] (`<YYYY-MM-DD>`) Post-command clean, detached-at-E checks passed.
- [ ] (`<YYYY-MM-DD>`) Audit evidence and `uvx nox -s lint` passed.
