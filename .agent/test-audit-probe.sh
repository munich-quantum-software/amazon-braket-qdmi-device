#!/usr/bin/env bash
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

script_directory=$(
  cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd
)
readonly script_directory
readonly probe_source="${script_directory}/audit-probe.sh"

fail() {
  echo "test-audit-probe: $*" >&2
  exit 1
}

[[ -x "${probe_source}" ]] || fail "probe is not executable: ${probe_source}"

scratch_directory=$(mktemp -d)
readonly scratch_directory
trap 'rm -rf -- "${scratch_directory}"' EXIT

readonly primary_repository="${scratch_directory}/primary"
readonly audit_worktree="${scratch_directory}/audit-worktree"
readonly attached_worktree="${scratch_directory}/attached-worktree"
readonly stub_directory="${scratch_directory}/stubs"
readonly probe_tmp_directory="${scratch_directory}/probe-tmp"
readonly runner_log="${scratch_directory}/runner.log"
readonly command_output="${scratch_directory}/command.out"

mkdir -p "${primary_repository}/.agent" "${primary_repository}/src" \
  "${primary_repository}/spank" \
  "${primary_repository}/test/python" "${stub_directory}" \
  "${probe_tmp_directory}"
cp -- "${probe_source}" "${primary_repository}/.agent/audit-probe.sh"
chmod +x "${primary_repository}/.agent/audit-probe.sh"

cat >"${primary_repository}/src/sample.py" <<'EOF'
def answer():
    return 1
EOF
cat >"${primary_repository}/test/python/test_sample.py" <<'EOF'
def test_answer():
    assert True
EOF
cat >"${primary_repository}/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(audit_probe_fixture LANGUAGES CXX)
EOF
cat >"${primary_repository}/spank/Dockerfile" <<'EOF'
FROM scratch
EOF
cat >"${primary_repository}/spank/spank.cpp" <<'EOF'
int spank_probe() {
  return 1;
}
EOF
cat >"${primary_repository}/.gitignore" <<'EOF'
.env-private
build-private/
EOF

git -C "${primary_repository}" init --quiet --initial-branch=main
git -C "${primary_repository}" config user.email audit-probe@example.invalid
git -C "${primary_repository}" config user.name "Audit Probe Self-Test"
git -C "${primary_repository}" add .
fixture_tree=$(git -C "${primary_repository}" write-tree)
fixture_commit=$(
  printf 'Create probe fixture\n' |
    git -C "${primary_repository}" commit-tree "${fixture_tree}"
)
git -C "${primary_repository}" update-ref HEAD "${fixture_commit}"
git -C "${primary_repository}" worktree add --quiet --detach \
  "${audit_worktree}" HEAD
git -C "${primary_repository}" branch audit-attached HEAD
git -C "${primary_repository}" worktree add --quiet \
  "${attached_worktree}" audit-attached
echo "private-value-must-not-enter-context" >"${audit_worktree}/.env-private"
mkdir -p "${audit_worktree}/build-private"
echo "private-build-cache" >"${audit_worktree}/build-private/cache"

baseline=$(git -C "${audit_worktree}" rev-parse HEAD)
readonly baseline
readonly source_file="${audit_worktree}/src/sample.py"
readonly expected_source=$'def answer():\n    return 1'
readonly expected_cmake=$'cmake_minimum_required(VERSION 3.20)\nproject(audit_probe_fixture LANGUAGES CXX)'
readonly expected_spank_source=$'int spank_probe() {\n  return 1;\n}'

cat >"${stub_directory}/verify-offline-aws" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${AWS_ACCESS_KEY_ID-}" ]]; then
  exit 0
fi
[[ "${AWS_EC2_METADATA_DISABLED-}" == "true" ]]
[[ -z "${AWS_PROFILE+x}" ]]
[[ -z "${AWS_DEFAULT_PROFILE+x}" ]]
[[ "${AWS_CONFIG_FILE-}" == */amazon-braket-audit-probe.*/empty-aws-config ]]
[[ "${AWS_SHARED_CREDENTIALS_FILE-}" == \
  */amazon-braket-audit-probe.*/empty-aws-credentials ]]
[[ -f "${AWS_CONFIG_FILE}" && ! -s "${AWS_CONFIG_FILE}" ]]
[[ -f "${AWS_SHARED_CREDENTIALS_FILE}" && ! -s "${AWS_SHARED_CREDENTIALS_FILE}" ]]
echo "aws-offline-isolation verified" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
EOF
chmod +x "${stub_directory}/verify-offline-aws"

cat >"${stub_directory}/uvx" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

verify-offline-aws

printf 'uvx' >>"${AUDIT_PROBE_SELF_TEST_LOG}"
printf ' <%s>' "$@" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
printf '\n' >>"${AUDIT_PROBE_SELF_TEST_LOG}"

[[ "${1-}" == "nox" ]] || {
  echo "uvx stub expected nox" >&2
  exit 90
}

saw_tests_session=false
for ((index = 1; index <= $#; ++index)); do
  if [[ "${!index}" == "-s" ]]; then
    next_index=$((index + 1))
    if ((next_index <= $#)) && [[ "${!next_index}" == "tests" ]]; then
      saw_tests_session=true
    fi
  fi
done
${saw_tests_session} || {
  echo "uvx stub expected the supported nox tests session" >&2
  exit 91
}

coverage_json=""
deselected=false
for argument in "$@"; do
  case "${argument}" in
    --cov-report=json:*) coverage_json=${argument#--cov-report=json:} ;;
    --deselect) deselected=true ;;
  esac
done
if [[ -n "${coverage_json}" ]]; then
  if ${deselected}; then
    echo "uvx-phase coverage-modified" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
    printf '%s\n' \
      '{"totals": {"covered_lines": 8, "num_statements": 10, "missing_lines": 2, "num_branches": 4, "covered_branches": 2, "missing_branches": 2}}' \
      >"${coverage_json}"
  else
    echo "uvx-phase coverage-baseline" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
    printf '%s\n' \
      '{"totals": {"covered_lines": 9, "num_statements": 10, "missing_lines": 1, "num_branches": 4, "covered_branches": 3, "missing_branches": 1}}' \
      >"${coverage_json}"
  fi
  exit 0
fi

if grep -Fxq "    return 1" \
  "${AUDIT_PROBE_SELF_TEST_REPOSITORY}/src/sample.py"; then
  echo "uvx-phase baseline" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
  if [[ "${AUDIT_PROBE_SELF_TEST_COLLATERAL_MUTATION-}" == "tracked" ]]; then
    echo "# collateral mutation" >>"${AUDIT_PROBE_SELF_TEST_REPOSITORY}/CMakeLists.txt"
  fi
  if [[ "${AUDIT_PROBE_SELF_TEST_COLLATERAL_MUTATION-}" == "untracked" ]]; then
    echo "collateral mutation" \
      >"${AUDIT_PROBE_SELF_TEST_REPOSITORY}/generated-collateral.txt"
  fi
  if [[ "${AUDIT_PROBE_SELF_TEST_FAIL_BASELINE-}" == "python" ]]; then
    exit 92
  fi
  exit 0
fi

grep -Fxq "    return 0" \
  "${AUDIT_PROBE_SELF_TEST_REPOSITORY}/src/sample.py" || {
  echo "uvx stub observed neither the baseline nor the injected Python source" >&2
  exit 93
}
echo "uvx-phase fault" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
if [[ "${AUDIT_PROBE_SELF_TEST_WAIT_AFTER_FAULT-}" == "yes" ]]; then
  : >"${AUDIT_PROBE_SELF_TEST_FAULT_MARKER}"
  while true; do
    sleep 1
  done
fi
if [[ "${AUDIT_PROBE_SELF_TEST_FAULT_RUNNER_ERROR-}" == "python" ]]; then
  exit 97
fi
echo "raw-output <AssertionError>" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
echo "FAILED test/python/test_sample.py::test_answer - AssertionError"
exit 1
EOF
chmod +x "${stub_directory}/uvx"

cat >"${stub_directory}/cmake" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

verify-offline-aws

printf 'cmake' >>"${AUDIT_PROBE_SELF_TEST_LOG}"
printf ' <%s>' "$@" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
printf '\n' >>"${AUDIT_PROBE_SELF_TEST_LOG}"

if [[ " $* " != *" --build "* ]]; then
  grep -Fxq "    return 1" \
    "${AUDIT_PROBE_SELF_TEST_REPOSITORY}/src/sample.py" || {
    echo "cmake configure stub did not observe the baseline source" >&2
    exit 94
  }
  echo "cmake-phase configure" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
  exit 0
fi

if grep -Fxq "    return 1" \
  "${AUDIT_PROBE_SELF_TEST_REPOSITORY}/src/sample.py"; then
  echo "cmake-phase baseline-build" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
  exit 0
fi

if grep -Fxq "    return 0" \
  "${AUDIT_PROBE_SELF_TEST_REPOSITORY}/src/sample.py"; then
  echo "cmake-phase fault-build" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
  exit 42
fi
echo "cmake build stub observed neither the baseline nor the injected source" >&2
exit 95
EOF
chmod +x "${stub_directory}/cmake"

cat >"${stub_directory}/ctest" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
verify-offline-aws
[[ " $* " == *" --no-tests=error "* ]] || {
  echo "ctest stub requires --no-tests=error" >&2
  exit 96
}
if grep -Fxq "    return 1" \
  "${AUDIT_PROBE_SELF_TEST_REPOSITORY}/src/sample.py"; then
  echo "ctest-phase baseline-suite" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
  if [[ "${AUDIT_PROBE_SELF_TEST_CTEST_NO_MATCH-}" == "yes" ]]; then
    echo "No tests were found!!!" >&2
    exit 8
  fi
  exit 0
fi
echo "ctest stub ran after the simulated fault build failure" >&2
exit 98
EOF
chmod +x "${stub_directory}/ctest"

cat >"${stub_directory}/docker" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

verify-offline-aws

printf 'docker' >>"${AUDIT_PROBE_SELF_TEST_LOG}"
printf ' <%s>' "$@" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
printf '\n' >>"${AUDIT_PROBE_SELF_TEST_LOG}"

case "${1-}" in
  image)
    [[ "${2-}" == "rm" ]] || exit 99
    exit 0
    ;;
  build)
    iid_file=""
    previous=""
    for argument in "$@"; do
      if [[ "${previous}" == "--iidfile" ]]; then
        iid_file=${argument}
      fi
      previous=${argument}
    done
    context=${!#}
    [[ -n "${iid_file}" ]] || exit 100
    [[ ! -e "${context}/.git" ]] || {
      echo "Docker context leaked Git metadata" >&2
      exit 101
    }
    [[ ! -e "${context}/.env-private" ]] || {
      echo "Docker context leaked an ignored private environment file" >&2
      exit 102
    }
    [[ ! -e "${context}/build-private" ]] || {
      echo "Docker context leaked an ignored build cache" >&2
      exit 103
    }
    if grep -Fxq "  return 1;" "${context}/spank/spank.cpp"; then
      echo "docker-phase baseline-build" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
      echo "sha256:baseline" >"${iid_file}"
      exit 0
    fi
    grep -Fxq "  return 0;" "${context}/spank/spank.cpp" || exit 104
    echo "docker-phase fault-build" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
    if [[ "${AUDIT_PROBE_SELF_TEST_DOCKER_BUILD_ERROR-}" == "yes" ]]; then
      exit 1
    fi
    if [[ "${AUDIT_PROBE_SELF_TEST_DOCKER_MISSING_IID-}" == "yes" ]]; then
      exit 0
    fi
    echo "sha256:fault" >"${iid_file}"
    exit 0
    ;;
  run)
    image_id=${!#}
    case "${image_id}" in
      sha256:baseline)
        echo "docker-phase baseline-suite" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
        exit 0
        ;;
      sha256:fault)
        echo "docker-phase fault-suite" >>"${AUDIT_PROBE_SELF_TEST_LOG}"
        if [[ -n "${AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_STATUS-}" ]]; then
          exit "${AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_STATUS}"
        fi
        if [[ "${AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_ERROR-}" == "yes" ]]; then
          exit 125
        fi
        if [[ "${AUDIT_PROBE_SELF_TEST_DOCKER_MUTANT_KILLED-}" == "yes" ]]; then
          echo "=== Verifying configuration references stay out of Slurm daemons ==="
          exit 1
        fi
        exit 0
        ;;
      *) exit 105 ;;
    esac
    ;;
  *) exit 106 ;;
esac
EOF
chmod +x "${stub_directory}/docker"

clear_live_environment() {
  unset AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY AWS_SESSION_TOKEN
  unset AWS_SECURITY_TOKEN AWS_WEB_IDENTITY_TOKEN_FILE AWS_ROLE_ARN
  unset AWS_ROLE_SESSION_NAME AWS_CONTAINER_CREDENTIALS_FULL_URI
  unset AWS_CONTAINER_CREDENTIALS_RELATIVE_URI AWS_CONTAINER_AUTHORIZATION_TOKEN
  unset AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE
  unset AWS_PROFILE AWS_DEFAULT_PROFILE AWS_SHARED_CREDENTIALS_FILE
  unset AWS_CONFIG_FILE AWS_REGION AWS_DEFAULT_REGION AWS_S3_BUCKET
  unset AWS_ENDPOINT_URL AWS_ENDPOINT_URL_BRAKET AWS_ENDPOINT_URL_S3
  unset AWS_ENDPOINT_URL_STS
  unset AMZN_BRAKET_TASK_RESULTS_S3_URI AMAZON_BRAKET_RESERVATION_ARN
  unset AMAZON_BRAKET_DEVICE_ARN AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG
  unset AMAZON_BRAKET_QDMI_TEST_ALLOW_BUCKET_CREATION
  unset AMAZON_BRAKET_PENNYLANE_LIVE IQM_DEVICE_ARN
}

run_probe_from() {
  target_worktree=$1
  shift
  readonly live_environment=${AUDIT_PROBE_SELF_TEST_LIVE_ENV-}
  readonly baseline_failure=${AUDIT_PROBE_SELF_TEST_FAIL_BASELINE-}
  readonly fault_runner_error=${AUDIT_PROBE_SELF_TEST_FAULT_RUNNER_ERROR-}
  readonly collateral_mutation=${AUDIT_PROBE_SELF_TEST_COLLATERAL_MUTATION-}
  readonly ctest_no_match=${AUDIT_PROBE_SELF_TEST_CTEST_NO_MATCH-}
  readonly docker_fault_error=${AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_ERROR-}
  readonly docker_mutant_killed=${AUDIT_PROBE_SELF_TEST_DOCKER_MUTANT_KILLED-}
  readonly docker_build_error=${AUDIT_PROBE_SELF_TEST_DOCKER_BUILD_ERROR-}
  readonly docker_missing_iid=${AUDIT_PROBE_SELF_TEST_DOCKER_MISSING_IID-}
  readonly docker_fault_status=${AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_STATUS-}
  readonly wait_after_fault=${AUDIT_PROBE_SELF_TEST_WAIT_AFTER_FAULT-}
  readonly start_new_session=${AUDIT_PROBE_SELF_TEST_NEW_SESSION-}
  clear_live_environment
  if [[ "${live_environment}" == "access-key" ]]; then
    export AWS_ACCESS_KEY_ID="probe-placeholder-not-a-credential"
  elif [[ -n "${live_environment}" ]]; then
    export "${live_environment}=probe-placeholder-for-${live_environment}"
  fi
  export PATH="${stub_directory}:${PATH}"
  export TMPDIR="${probe_tmp_directory}"
  export AUDIT_PROBE_SELF_TEST_LOG="${runner_log}"
  export AUDIT_PROBE_SELF_TEST_REPOSITORY="${target_worktree}"
  export AUDIT_PROBE_SELF_TEST_FAIL_BASELINE="${baseline_failure}"
  export AUDIT_PROBE_SELF_TEST_FAULT_RUNNER_ERROR="${fault_runner_error}"
  export AUDIT_PROBE_SELF_TEST_COLLATERAL_MUTATION="${collateral_mutation}"
  export AUDIT_PROBE_SELF_TEST_CTEST_NO_MATCH="${ctest_no_match}"
  export AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_ERROR="${docker_fault_error}"
  export AUDIT_PROBE_SELF_TEST_DOCKER_MUTANT_KILLED="${docker_mutant_killed}"
  export AUDIT_PROBE_SELF_TEST_DOCKER_BUILD_ERROR="${docker_build_error}"
  export AUDIT_PROBE_SELF_TEST_DOCKER_MISSING_IID="${docker_missing_iid}"
  export AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_STATUS="${docker_fault_status}"
  export AUDIT_PROBE_SELF_TEST_WAIT_AFTER_FAULT="${wait_after_fault}"
  export AUDIT_PROBE_SELF_TEST_FAULT_MARKER="${scratch_directory}/fault-started"
  cd -- "${target_worktree}"
  if [[ "${start_new_session}" == "yes" ]]; then
    exec setsid "${target_worktree}/.agent/audit-probe.sh" "$@"
  fi
  exec "${target_worktree}/.agent/audit-probe.sh" "$@"
}

run_probe() (
  run_probe_from "${audit_worktree}" "$@"
)

run_attached_probe() (
  run_probe_from "${attached_worktree}" "$@"
)

assert_clean_and_restored() {
  [[ "$(<"${source_file}")" == "${expected_source}" ]] ||
    fail "probe did not restore the injected source file"
  [[ "$(<"${audit_worktree}/CMakeLists.txt")" == "${expected_cmake}" ]] ||
    fail "probe did not restore collateral tracked-file changes"
  [[ "$(<"${audit_worktree}/spank/spank.cpp")" == "${expected_spank_source}" ]] ||
    fail "probe did not restore the SPANK production source"
  [[ ! -e "${audit_worktree}/generated-collateral.txt" ]] ||
    fail "probe did not remove a command-created untracked file"
  grep -Fxq 'private-value-must-not-enter-context' "${audit_worktree}/.env-private" ||
    fail "probe changed or removed a private ignored worktree file"
  [[ -z "$(git -C "${audit_worktree}" status --porcelain)" ]] ||
    fail "probe left the audit worktree dirty"
  [[ -z "$(find "${probe_tmp_directory}" -mindepth 1 -print -quit)" ]] ||
    fail "probe left an isolated build or environment behind"
}

expect_failure() {
  local description=$1
  local expected_pattern=$2
  shift 2
  : >"${command_output}"
  if "$@" >"${command_output}" 2>&1; then
    fail "${description}: command unexpectedly succeeded"
  fi
  grep -Eiq "${expected_pattern}" "${command_output}" ||
    fail "${description}: diagnostic did not match ${expected_pattern}"
}

common_python_arguments=(
  t2
  --expected-baseline "${baseline}"
  --lang python
  --tests test/python
  --inject src/sample.py:2
  --with "    return 0"
)

echo "=== Rejecting a dirty audit worktree ==="
printf '\n# dirty\n' >>"${source_file}"
: >"${runner_log}"
expect_failure "dirty-tree check" 'clean|dirty|working tree' \
  run_probe "${common_python_arguments[@]}"
[[ ! -s "${runner_log}" ]] || fail "runner executed in a dirty worktree"
git -C "${audit_worktree}" checkout -- src/sample.py

echo "=== Rejecting an unexpected baseline ==="
: >"${runner_log}"
wrong_baseline=0000000000000000000000000000000000000000
expect_failure "baseline check" 'baseline|HEAD|commit' \
  run_probe t2 --expected-baseline "${wrong_baseline}" --lang python \
  --tests test/python --inject src/sample.py:2 --with "    return 0"
[[ ! -s "${runner_log}" ]] || fail "runner executed at the wrong baseline"
assert_clean_and_restored

echo "=== Rejecting an attached audit worktree at the exact baseline ==="
: >"${runner_log}"
expect_failure "attached worktree" 'detached|branch|HEAD|worktree' \
  run_attached_probe "${common_python_arguments[@]}"
[[ ! -s "${runner_log}" ]] || fail "attached worktree rejection still ran a test tool"
[[ -z "$(git -C "${attached_worktree}" status --porcelain)" ]] ||
  fail "attached worktree rejection changed its worktree"
assert_clean_and_restored

echo "=== Rejecting unsupported nox sessions ==="
: >"${runner_log}"
expect_failure "unsupported nox session" 'nox|session|tests' \
  run_probe "${common_python_arguments[@]}" --nox-session lint
[[ ! -s "${runner_log}" ]] || fail "rejected nox session still ran a test tool"
assert_clean_and_restored

echo "=== Rejecting invalid edit specs before runner commands ==="
: >"${runner_log}"
expect_failure "invalid injection range" 'inject|line|range|beyond' \
  run_probe t2 --expected-baseline "${baseline}" --lang python \
  --tests test/python --inject src/sample.py:999 --with "    return 0"
[[ ! -s "${runner_log}" ]] || fail "invalid injection still ran a test tool"
expect_failure "invalid omission range" 'omit|line|range|beyond' \
  run_probe t1 --expected-baseline "${baseline}" --lang python \
  --source src --tests test/python --omit src/sample.py:999
[[ ! -s "${runner_log}" ]] || fail "invalid omission still ran a test tool"
expect_failure "unsafe SPANK injection target" 'SPANK|production|source|spank' \
  run_probe t2 --expected-baseline "${baseline}" --lang spank \
  --inject spank/Dockerfile:1 --with 'FROM scratch'
[[ ! -s "${runner_log}" ]] || fail "unsafe SPANK injection target still ran Docker"
assert_clean_and_restored

echo "=== Reporting exact source-scoped T1 totals and deltas ==="
: >"${runner_log}"
run_probe t1 --expected-baseline "${baseline}" --lang python \
  --source src --tests test/python \
  --drop test/python/test_sample.py::test_answer >"${command_output}" 2>&1
grep -Fxq 'with statements : total=10 covered=9 missing=1' "${command_output}" ||
  fail "T1 omitted exact baseline statement totals"
grep -Fxq 'without stmts   : total=10 covered=8 missing=2' "${command_output}" ||
  fail "T1 omitted exact modified statement totals"
grep -Fxq 'statement delta : total=0 covered=-1 missing=1' "${command_output}" ||
  fail "T1 statement delta was not exact"
grep -Fxq 'with branches   : total=4 covered=3 missing=1' "${command_output}" ||
  fail "T1 omitted exact baseline branch totals"
grep -Fxq 'without branches: total=4 covered=2 missing=2' "${command_output}" ||
  fail "T1 omitted exact modified branch totals"
grep -Fxq 'branch delta    : total=0 covered=-1 missing=1' "${command_output}" ||
  fail "T1 branch delta was not exact"
grep -Fq '<--cov=src>' "${runner_log}" ||
  fail "T1 did not scope Python coverage to the selected source"
grep -Fq '<--cov-branch>' "${runner_log}" ||
  fail "T1 did not collect Python branch coverage"
assert_clean_and_restored

echo "=== Restoring a successful Python fault injection ==="
: >"${runner_log}"
run_probe "${common_python_arguments[@]}" >"${command_output}" 2>&1
grep -Fq 'uvx <nox>' "${runner_log}" ||
  fail "Python probe did not use uvx nox"
grep -Fq '<-s> <tests>' "${runner_log}" ||
  fail "Python probe did not use the supported nox tests session"
grep -Fq 'aws-offline-isolation verified' "${runner_log}" ||
  fail "Python runner did not receive isolated offline AWS configuration"
[[ "$(grep -c '^uvx-phase baseline$' "${runner_log}")" -eq 1 ]] ||
  fail "Python probe did not run exactly one unedited baseline"
[[ "$(grep -c '^uvx-phase fault$' "${runner_log}")" -eq 1 ]] ||
  fail "Python probe did not run exactly one injected fault suite"
baseline_line=$(grep -n '^uvx-phase baseline$' "${runner_log}" | cut -d: -f1)
fault_line=$(grep -n '^uvx-phase fault$' "${runner_log}" | cut -d: -f1)
((baseline_line < fault_line)) || fail "Python fault suite ran before its baseline"
grep -Fxq 'baseline suite  : pass' "${command_output}" ||
  fail "Python evidence did not record its passing baseline"
grep -Fxq 'failing tests   : 1' "${command_output}" ||
  fail "Python evidence did not count the failing test"
grep -Fxq '  - test/python/test_sample.py::test_answer' "${command_output}" ||
  fail "Python evidence did not include the sanitized failing node ID"
grep -Fq 'AssertionError' "${runner_log}" ||
  fail "Python stub did not exercise raw failure-detail suppression"
! grep -Fq 'AssertionError' "${command_output}" ||
  fail "Python evidence included raw failure details"
assert_clean_and_restored

echo "=== Restoring an injected edit after external termination ==="
: >"${runner_log}"
rm -f -- "${scratch_directory}/fault-started"
AUDIT_PROBE_SELF_TEST_WAIT_AFTER_FAULT=yes \
  AUDIT_PROBE_SELF_TEST_NEW_SESSION=yes \
  run_probe "${common_python_arguments[@]}" >"${command_output}" 2>&1 &
terminated_probe=$!
fault_started=no
for _ in {1..100}; do
  if [[ -e "${scratch_directory}/fault-started" ]]; then
    fault_started=yes
    break
  fi
  sleep 0.05
done
if [[ "${fault_started}" != "yes" ]]; then
  kill -TERM "${terminated_probe}" >/dev/null 2>&1 || true
  wait "${terminated_probe}" >/dev/null 2>&1 || true
  fail "termination probe never reached the injected fault"
fi
probe_group=$(ps -o pgid= -p "${terminated_probe}" | tr -d ' ')
if [[ "${probe_group}" != "${terminated_probe}" ]]; then
  session_probe=$(ps -o pid= --ppid "${terminated_probe}" | awk 'NR == 1 { print $1 }')
  [[ -n "${session_probe}" ]] || {
    kill -TERM "${terminated_probe}" >/dev/null 2>&1 || true
    wait "${terminated_probe}" >/dev/null 2>&1 || true
    fail "termination probe did not expose its isolated session process"
  }
  probe_group=$(ps -o pgid= -p "${session_probe}" | tr -d ' ')
  if [[ "${probe_group}" != "${session_probe}" ]]; then
    kill -TERM "${terminated_probe}" "${session_probe}" >/dev/null 2>&1 || true
    wait "${terminated_probe}" >/dev/null 2>&1 || true
    fail "termination probe did not start in an isolated process group"
  fi
fi
kill -TERM -- "-${probe_group}"
if wait "${terminated_probe}"; then
  fail "externally terminated probe unexpectedly exited successfully"
fi
grep -Fxq 'uvx-phase fault' "${runner_log}" ||
  fail "termination test stopped before the fault command observed the edit"
assert_clean_and_restored

echo "=== Restoring and rejecting collateral tracked-file mutations ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_COLLATERAL_MUTATION=tracked \
  expect_failure "collateral tracked mutation" 'tracked|mutat|state|adjudic' \
    run_probe "${common_python_arguments[@]}"
grep -Fxq 'uvx-phase baseline' "${runner_log}" ||
  fail "collateral mutation case did not run the baseline command"
! grep -Fq 'uvx-phase fault' "${runner_log}" ||
  fail "collateral mutation case proceeded to fault injection"
! grep -Fq '=== SpecAudit probe:' "${command_output}" ||
  fail "collateral mutation produced a verdict evidence block"
assert_clean_and_restored

echo "=== Removing and rejecting command-created untracked files ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_COLLATERAL_MUTATION=untracked \
  expect_failure "collateral untracked mutation" 'created|worktree|state|adjudic' \
    run_probe "${common_python_arguments[@]}"
grep -Fxq 'uvx-phase baseline' "${runner_log}" ||
  fail "untracked collateral case did not run its baseline command"
! grep -Fq 'uvx-phase fault' "${runner_log}" ||
  fail "untracked collateral case proceeded to fault injection"
assert_clean_and_restored

echo "=== Aborting before injection when the Python baseline fails ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_FAIL_BASELINE=python \
  expect_failure "failed Python baseline" 'baseline|suite|failed' \
    run_probe "${common_python_arguments[@]}"
grep -Fxq 'uvx-phase baseline' "${runner_log}" ||
  fail "Python baseline-failure case did not run the baseline"
! grep -Fq 'uvx-phase fault' "${runner_log}" ||
  fail "Python baseline-failure case still injected and ran the fault"
! grep -Fq '=== SpecAudit probe:' "${command_output}" ||
  fail "Python baseline failure produced a verdict evidence block"
assert_clean_and_restored

echo "=== Rejecting an unattributed Python fault-runner error ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_FAULT_RUNNER_ERROR=python \
  expect_failure "Python fault runner error" 'runner|failed|error' \
    run_probe "${common_python_arguments[@]}"
grep -Fxq 'fault suite     : runner-error' "${command_output}" ||
  fail "Python runner error was not distinguished from a test failure"
grep -Fxq 'failing tests   : 0' "${command_output}" ||
  fail "Python runner error invented a failing test"
! grep -Fq 'does not detect' "${command_output}" ||
  fail "Python runner error incorrectly supported a no-detection verdict"
assert_clean_and_restored

echo "=== Restoring a fault after a failed C++ build ==="
: >"${runner_log}"
expect_failure "non-compiling C++ fault" 'compile|build|adjudic|evaluated' \
  run_probe t2 --expected-baseline "${baseline}" --lang cpp \
  --target audit-probe-fixture --inject src/sample.py:2 \
  --with "    return 0"
grep -Fq 'cmake' "${runner_log}" || fail "C++ probe did not invoke CMake"
grep -Fq '<--build>' "${runner_log}" ||
  fail "C++ probe did not attempt the simulated failing build"
grep -Fxq 'cmake-phase configure' "${runner_log}" ||
  fail "C++ probe did not configure before the baseline build"
grep -Fxq 'cmake-phase baseline-build' "${runner_log}" ||
  fail "C++ probe did not build the unedited baseline"
grep -Fxq 'ctest-phase baseline-suite' "${runner_log}" ||
  fail "C++ probe did not run the unedited baseline suite"
grep -Fxq 'cmake-phase fault-build' "${runner_log}" ||
  fail "C++ probe did not build the injected fault"
grep -Fxq 'fault build     : fail' "${command_output}" ||
  fail "C++ compile failure omitted sanitized fault-build status"
grep -Fq 'non-adjudicable' "${command_output}" ||
  fail "C++ compile failure did not invalidate the experiment"
assert_clean_and_restored

echo "=== Rejecting an empty CTest selection ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_CTEST_NO_MATCH=yes \
  expect_failure "empty CTest selection" 'baseline|suite|failed|tests' \
    run_probe t2 --expected-baseline "${baseline}" --lang cpp \
    --target audit-probe-fixture --ctest DoesNotExist \
    --inject src/sample.py:2 --with "    return 0"
grep -Fxq 'ctest-phase baseline-suite' "${runner_log}" ||
  fail "empty CTest selection did not reach CTest"
! grep -Fq 'cmake-phase fault-build' "${runner_log}" ||
  fail "empty CTest selection proceeded to fault injection"
! grep -Fq '=== SpecAudit probe:' "${command_output}" ||
  fail "empty CTest selection produced a verdict evidence block"
assert_clean_and_restored

echo "=== Building SPANK from a tracked-only Docker context ==="
: >"${runner_log}"
run_probe t2 --expected-baseline "${baseline}" --lang spank \
  --inject spank/spank.cpp:2 --with "  return 0;" \
  >"${command_output}" 2>&1
for phase in baseline-build baseline-suite fault-build fault-suite; do
  grep -Fxq "docker-phase ${phase}" "${runner_log}" ||
    fail "SPANK probe missed Docker phase ${phase}"
done
grep -Fxq 'fault suite     : pass' "${command_output}" ||
  fail "SPANK probe did not record the surviving mutant"
assert_clean_and_restored

echo "=== Treating a SPANK container failure as infrastructure failure ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_ERROR=yes \
  expect_failure "SPANK infrastructure failure" 'Docker|infrastructure|complete|valid' \
    run_probe t2 --expected-baseline "${baseline}" --lang spank \
    --inject spank/spank.cpp:2 --with "  return 0;"
grep -Fxq 'fault suite     : infrastructure-error' "${command_output}" ||
  fail "SPANK container failure was not classified as infrastructure failure"
grep -Fxq 'failing tests   : 0' "${command_output}" ||
  fail "SPANK infrastructure failure invented a failing test"
! grep -Fq 'detects the injected fault' "${command_output}" ||
  fail "SPANK infrastructure failure incorrectly supported a detection verdict"
assert_clean_and_restored

echo "=== Treating a signalled SPANK container as infrastructure failure ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_DOCKER_FAULT_STATUS=137 \
  expect_failure "SPANK signalled-container failure" 'Docker|infrastructure|complete|valid' \
    run_probe t2 --expected-baseline "${baseline}" --lang spank \
    --inject spank/spank.cpp:2 --with "  return 0;"
grep -Fxq 'fault suite     : infrastructure-error' "${command_output}" ||
  fail "SPANK signal exit was not classified as infrastructure failure"
grep -Fxq 'failing tests   : 0' "${command_output}" ||
  fail "SPANK signal exit invented a failing test"
! grep -Fq 'detects the injected fault' "${command_output}" ||
  fail "SPANK signal exit incorrectly supported a detection verdict"
assert_clean_and_restored

echo "=== Treating a SPANK Docker build failure as infrastructure ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_DOCKER_BUILD_ERROR=yes \
  expect_failure "SPANK build infrastructure failure" 'Docker|build|valid|infrastructure' \
    run_probe t2 --expected-baseline "${baseline}" --lang spank \
    --inject spank/spank.cpp:2 --with "  return 0;"
grep -Fxq 'fault build     : infrastructure-error' "${command_output}" ||
  fail "SPANK build failure was not classified as infrastructure"
grep -Fxq 'fault suite     : not-run' "${command_output}" ||
  fail "SPANK build failure incorrectly ran or classified the suite"
! grep -Fq 'detects the injected fault' "${command_output}" ||
  fail "SPANK build failure incorrectly supported a detection verdict"
assert_clean_and_restored

echo "=== Treating a missing SPANK image ID as build infrastructure ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_DOCKER_MISSING_IID=yes \
  expect_failure "SPANK missing image ID" 'Docker|build|valid|infrastructure' \
    run_probe t2 --expected-baseline "${baseline}" --lang spank \
    --inject spank/spank.cpp:2 --with "  return 0;"
grep -Fxq 'fault build     : infrastructure-error' "${command_output}" ||
  fail "missing SPANK image ID was not classified as build infrastructure"
grep -Fxq 'fault suite     : not-run' "${command_output}" ||
  fail "missing SPANK image ID incorrectly ran or classified the suite"
! grep -Fq 'docker-phase fault-suite' "${runner_log}" ||
  fail "missing SPANK image ID still ran the container suite"
! grep -Fq 'detects the injected fault' "${command_output}" ||
  fail "missing SPANK image ID incorrectly supported a detection verdict"
assert_clean_and_restored

echo "=== Treating an ordinary SPANK test exit as a killed mutant ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_DOCKER_MUTANT_KILLED=yes \
  run_probe t2 --expected-baseline "${baseline}" --lang spank \
  --inject spank/spank.cpp:2 --with "  return 0;" \
  >"${command_output}" 2>&1
grep -Fxq 'fault suite     : fail' "${command_output}" ||
  fail "ordinary SPANK test exit was not classified as a killed mutant"
grep -Fxq 'failing tests   : 1' "${command_output}" ||
  fail "SPANK killed-mutant evidence omitted its failing script count"
grep -Fxq '  - spank: Verifying configuration references stay out of Slurm daemons' \
  "${command_output}" || fail "SPANK killed-mutant evidence omitted its sanitized phase"
grep -Fq 'detects the injected fault' "${command_output}" ||
  fail "SPANK killed mutant did not produce a detection reading"
assert_clean_and_restored

echo "=== Documenting Docker cache cleanup limits ==="
run_probe --help >"${command_output}" 2>&1
grep -Fq 'BuildKit layers and cache' "${command_output}" ||
  fail "probe help omitted Docker cache cleanup limitations"

echo "=== Keeping offline probes separated from live AWS state ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_LIVE_ENV=access-key \
  expect_failure "offline AWS check" 'AWS|credential|live' \
    run_probe "${common_python_arguments[@]}"
[[ ! -s "${runner_log}" ]] || fail "offline rejection still ran a test tool"
! grep -Fq 'probe-placeholder-not-a-credential' "${command_output}" ||
  fail "offline diagnostic printed an AWS environment value"
assert_clean_and_restored

for live_variable in \
  AMAZON_BRAKET_QDMI_TEST_ALLOW_BUCKET_CREATION \
  AMAZON_BRAKET_DEVICE_ARN \
  IQM_DEVICE_ARN \
  AWS_ENDPOINT_URL_S3 \
  AWS_ENDPOINT_URL_STS; do
  echo "=== Refusing offline environment variable ${live_variable} ==="
  : >"${runner_log}"
  AUDIT_PROBE_SELF_TEST_LIVE_ENV=${live_variable} \
    expect_failure "offline ${live_variable} check" 'AWS|credential|live' \
      run_probe "${common_python_arguments[@]}"
  [[ ! -s "${runner_log}" ]] ||
    fail "offline ${live_variable} rejection still ran a test tool"
  ! grep -Fq "probe-placeholder-for-${live_variable}" "${command_output}" ||
    fail "offline diagnostic printed the value of ${live_variable}"
  assert_clean_and_restored
done

echo "=== Requiring an identifier for each live evidence batch ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_LIVE_ENV=access-key \
  expect_failure "missing live batch identifier" 'batch|identifier' \
    run_probe "${common_python_arguments[@]}" --live
[[ ! -s "${runner_log}" ]] || fail "live probe ran without a batch identifier"
assert_clean_and_restored

echo "=== Rejecting a live batch identifier in offline mode ==="
: >"${runner_log}"
expect_failure "offline batch identifier" 'batch|live' \
  run_probe "${common_python_arguments[@]}" --batch-id probe-self-test
[[ ! -s "${runner_log}" ]] || fail "offline probe accepted a batch identifier"
assert_clean_and_restored

echo "=== Allowing an explicitly identified, stubbed live probe ==="
: >"${runner_log}"
AUDIT_PROBE_SELF_TEST_LIVE_ENV=access-key \
  run_probe "${common_python_arguments[@]}" --live \
  --batch-id probe-self-test >"${command_output}" 2>&1
[[ -s "${runner_log}" ]] || fail "identified live probe did not run its stub"
grep -Fxq 'live batch      : probe-self-test' "${command_output}" ||
  fail "live evidence omitted its non-secret batch identifier"
! grep -Fq 'probe-placeholder-not-a-credential' "${command_output}" ||
  fail "live probe output printed an AWS environment value"
assert_clean_and_restored

echo "=== audit-probe self-tests passed ==="
