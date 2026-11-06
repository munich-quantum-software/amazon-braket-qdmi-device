# Contract audits

Audit a bounded behavior or change set against its callers, external contracts,
and failure modes. A passing test is evidence of implementation, not proof that
its asserted behavior is a public promise.

## Scope and evidence

Record the compared revisions and affected entry points. Trace each input from
its producer through validation, submission, and result handling. Include direct
QDMI callers that bypass the Python adapters. Distinguish requirements from
implementation choices and inferred intent.

Use these sources, in descending authority:

1. QDMI and relevant AWS API, Braket, and OpenQASM specifications.
2. Published project documentation and supported public interfaces.
3. Explicit maintainer requirements in linked issues or reviews.
4. Implementation history and recorded design rationale.
5. Current implementation and tests.

A missing explicit requirement does not make a check redundant. Examine error
boundaries, callers, resource limits, and compatibility before deleting it.
Challenge a proposed simplification with a concrete counterexample.

## Contracts to inspect

- QDMI ABI, symbol prefix, installed catalogue IDs, size queries, handle
  validation, error codes, and exception boundaries.
- Session credentials and Region selection; task, reservation, and S3 resource
  lifetimes; concurrency, cancellation, and partial results.
- Device capabilities, sparse site IDs, real couplers, native gate parameters,
  and supported program formats.
- Submitted OpenQASM, ProgramSets, result ordering, reopened tasks, and paths
  from both Python adapters and independent QDMI clients.
- Installed CMake and Python artifacts, relocation, discovery, and optional
  dependencies.

Assign each invariant to its owning layer. Similar code in two layers can
protect different entry points; prove that every supported path reaches the
surviving check before removing one.

## Validation

Start with the smallest probe that can disprove a finding. Prefer direct
semantic tests to exact serialized text unless formatting is itself a contract.
When weakening an assertion, identify the surviving oracle and the failures it
must detect. Equal coverage, a mock call count, or one shared failure does not
prove redundancy.

Use the repository's supported build and test commands. Keep offline selections
explicit. AWS metadata requests and task submissions require the authorization
specified in `AGENTS.md`; an audit does not grant it. Do not retry a paid test
without approval for that additional execution.

Tie results to the tested revision. A failed build, empty test selection,
missing dependency, or unavailable service is not a passing check. Separate
local validation, hosted CI, and live service evidence. Preserve historical
results as historical when they help explain a compatibility constraint.

## Findings and implementation

For each actionable finding, record:

- the affected behavior and owning source;
- the violated contract or demonstrated maintenance cost;
- the smallest sufficient change and its strongest counterexample;
- compatibility implications, validation, and remaining uncertainty.

Separate blockers, optional improvements, and design decisions requiring human
input. Do not invent findings or redesign sound boundaries. An audit is
read-only unless implementation is authorized. Follow `AGENTS.md` for commits,
publication, and review responsibilities.

Use `.agent/audits/TEMPLATE.md` when a durable record is useful. Keep records
concise and current; remove obsolete implementation recipes, campaign rosters,
and release instructions. Do not copy conversation history into them. Recheck a
finding against the final code before calling it resolved.
