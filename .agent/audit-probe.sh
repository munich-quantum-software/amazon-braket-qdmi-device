#!/usr/bin/env sh
# Copyright (c) 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License
# Measure how much a single test assertion actually protects.

set -eu

script_directory=$(
  CDPATH=''
  cd -- "$(dirname -- "$0")"
  pwd -P
)
repository_root=$(
  CDPATH=''
  cd -- "${script_directory}/.."
  pwd -P
)

probe_directory=""
edited_file=""
edit_backup=""
docker_images_file=""
baseline_tree_directory=""
baseline_backup_ready=no
spank_context=""

usage() {
  cat <<'EOF'
Usage:
  .agent/audit-probe.sh t1 --expected-baseline SHA --lang python \
    --source SRC --tests TESTS (--drop NODE | --omit FILE:LINE[-END])
  .agent/audit-probe.sh t1 --expected-baseline SHA --lang cpp \
    --source SRC --target TARGET --omit FILE:LINE[-END] [--ctest REGEX]
  .agent/audit-probe.sh t2 --expected-baseline SHA --lang python \
    --tests TESTS --inject FILE:LINE[-END] --with TEXT
  .agent/audit-probe.sh t2 --expected-baseline SHA --lang cpp \
    --target TARGET --inject FILE:LINE[-END] --with TEXT [--ctest REGEX]
  .agent/audit-probe.sh t2 --expected-baseline SHA --lang spank \
    --inject FILE:LINE[-END] --with TEXT

Tiers:
  t1  Coverage delta. Run the selected suite with and without an assertion.
      Report exact source-scoped statement/line and branch totals, covered
      counts, missing counts, and deltas. Equal coverage is evidence of
      redundancy, never proof on its own.
  t2  Fault injection. Replace the selected source lines and report whether the
      suite notices the fault. Start from a baseline where the suite passes.

Required safety option:
  --expected-baseline SHA
      Require this exact, full 40-character commit at HEAD. The probe also
      requires a clean, detached linked Git worktree dedicated to the audit.

Selection options:
  --lang LANG       python, cpp, or spank (SPANK supports t2 only).
  --source SRC      Repository-relative source file or directory for t1.
  --tests TESTS     Repository-relative pytest path.
  --target TARGET   CMake target for the C++ suite.
  --ctest REGEX     Optional regular expression passed to ctest -R. Required
                    for live C++ probes.
  --nox-session ID  Supported nox test session: tests or tests-3.10 through
                    tests-3.14 (default: tests).

Temporary-edit options:
  --drop NODE       Python pytest node ID to deselect during the second t1 run.
  --omit SPEC       Repository-relative FILE:LINE or FILE:LINE-END to delete
                    during the second t1 run.
  --inject SPEC     Repository-relative FILE:LINE or FILE:LINE-END to replace
                    during a t2 run. SPANK probes accept only production
                    .cpp/.h/.hpp files directly under spank/.
  --with TEXT       Replacement text for --inject. An empty value deletes the
                    selected lines. Literal newlines are supported.

Live evidence:
  --live --batch-id ID
      Acknowledge one bounded live-evidence batch. Both options are required
      together. Offline probes refuse exported AWS credential, Region, result,
      reservation, endpoint, and live-test variables before running anything.
      SPANK probes always use the repository's local Docker fixture and cannot
      be live. Its real Slurm/cgroup-v2 suite requires Docker --privileged.

The probe never has a keep mode. It restores its one temporary edit and removes
all isolated probe state on normal exit, failure, signal, or external timeout.
It suppresses raw command output so credentials and private AWS identifiers are
never copied into an audit evidence block.

SPANK builds receive a probe-owned context made exclusively from tracked files
at the expected baseline, with only the selected fault overlaid. The probe asks
Docker to remove each resulting image, but shared BuildKit layers and cache are
daemon-managed and may remain after the probe exits. No worktree-private,
ignored, Git-metadata, AWS, environment, or build-cache files enter the context.
Offline child processes also receive probe-owned empty AWS config and credential
files, no selected profile, and disabled EC2 metadata discovery, so an
implicit profile in the invoking user's home directory cannot authorize work.
EOF
}

fail() {
  echo "audit-probe: $1" >&2
  exit 2
}

cleanup() {
  status=$?
  trap - 0 HUP INT TERM

  if [ -n "${edited_file}" ] && [ -n "${edit_backup}" ] && [ -f "${edit_backup}" ]; then
    cp -p -- "${edit_backup}" "${repository_root}/${edited_file}" >/dev/null 2>&1 || true
  fi

  restore_all_tracked_state >/dev/null 2>&1 || true
  remove_untracked_additions >/dev/null 2>&1 || true

  if [ -n "${docker_images_file}" ] && [ -f "${docker_images_file}" ] &&
    command -v docker >/dev/null 2>&1; then
    while IFS= read -r image_id; do
      [ -n "${image_id}" ] || continue
      docker image rm "${image_id}" >/dev/null 2>&1 || true
    done <"${docker_images_file}"
  fi

  if [ -n "${probe_directory}" ] && [ -d "${probe_directory}" ]; then
    rm -rf -- "${probe_directory}"
  fi
  exit "${status}"
}

trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

need_value() {
  [ "$2" -ge 2 ] || fail "$1 requires a value"
}

validate_relative_path() {
  path=$1
  label=$2
  [ -n "${path}" ] || fail "${label} cannot be empty"
  case "${path}" in
    /* | -* | *:*) fail "${label} must be a repository-relative path without ':'" ;;
  esac
  case "/${path}/" in
    */../* | */./*) fail "${label} must not contain '.' or '..' path components" ;;
  esac
  [ -e "${repository_root}/${path}" ] || fail "${label} does not exist"
}

validate_tracked_file() {
  file=$1
  label=$2
  validate_relative_path "${file}" "${label}"
  [ -f "${repository_root}/${file}" ] || fail "${label} must name a regular file"
  [ ! -L "${repository_root}/${file}" ] || fail "${label} must not name a symbolic link"
  git -C "${repository_root}" ls-files --error-unmatch -- "${file}" >/dev/null 2>&1 ||
    fail "${label} must name a tracked file"
}

parse_edit_spec() {
  spec=$1
  label=$2
  case "${spec}" in
    *:*) ;;
    *) fail "${label} expects FILE:LINE or FILE:LINE-END" ;;
  esac

  parsed_file=${spec%:*}
  parsed_range=${spec##*:}
  validate_tracked_file "${parsed_file}" "${label} file"

  case "${parsed_range}" in
    *-*)
      parsed_first=${parsed_range%-*}
      parsed_last=${parsed_range#*-}
      ;;
    *)
      parsed_first=${parsed_range}
      parsed_last=${parsed_range}
      ;;
  esac
  case "${parsed_first}:${parsed_last}" in
    *[!0-9:]* | :* | *:) fail "${label} line range must contain positive integers" ;;
  esac
  [ "${parsed_first}" -ge 1 ] 2>/dev/null || fail "${label} line range must start at 1 or later"
  [ "${parsed_last}" -ge "${parsed_first}" ] 2>/dev/null ||
    fail "${label} line range must be ordered"
  total_lines=$(awk 'END { print NR }' "${repository_root}/${parsed_file}")
  [ "${parsed_last}" -le "${total_lines}" ] ||
    fail "${label} line range extends beyond the selected file"
}

apply_edit() {
  spec=$1
  replacement=$2
  parse_edit_spec "${spec}" "temporary edit"

  edited_file=${parsed_file}
  edit_backup=${probe_directory}/original
  edit_output=${probe_directory}/edited
  replacement_file=${probe_directory}/replacement
  cp -p -- "${repository_root}/${edited_file}" "${edit_backup}"
  printf '%s' "${replacement}" >"${replacement_file}"

  awk -v first="${parsed_first}" -v last="${parsed_last}" \
    -v replacement_file="${replacement_file}" '
      NR == first {
        while ((getline replacement_line < replacement_file) > 0) {
          print replacement_line
        }
        close(replacement_file)
      }
      NR < first || NR > last { print }
    ' "${edit_backup}" >"${edit_output}"
  cp -- "${edit_output}" "${repository_root}/${edited_file}"
  verify_tracked_state "${edited_file}"
}

restore_path_from_baseline() {
  path=$1
  [ "${baseline_backup_ready}" = yes ] || return 1
  source_path=${baseline_tree_directory}/${path}
  destination_path=${repository_root}/${path}
  if [ ! -e "${source_path}" ] && [ ! -L "${source_path}" ]; then
    git -C "${repository_root}" restore --staged --source="${expected_baseline}" -- "${path}" \
      >/dev/null 2>&1 || return 1
    if [ -d "${destination_path}" ] && [ ! -L "${destination_path}" ]; then
      find "${destination_path}" -depth -delete >/dev/null 2>&1 || return 1
    else
      rm -f -- "${destination_path}" >/dev/null 2>&1 || return 1
    fi
    return 0
  fi

  if [ -d "${destination_path}" ] && [ ! -L "${destination_path}" ]; then
    find "${destination_path}" -depth -delete >/dev/null 2>&1 || return 1
  else
    rm -f -- "${destination_path}" >/dev/null 2>&1 || return 1
  fi
  mkdir -p -- "$(dirname -- "${destination_path}")" || return 1
  cp -pP -- "${source_path}" "${destination_path}" || return 1
  git -C "${repository_root}" restore --staged --source="${expected_baseline}" -- "${path}" \
    >/dev/null 2>&1 || return 1
}

restore_all_tracked_state() {
  [ "${baseline_backup_ready}" = yes ] || return 0
  restore_list=${probe_directory}/cleanup-tracked-diff
  git -C "${repository_root}" diff --name-only "${expected_baseline}" -- >"${restore_list}" ||
    return 1
  restore_failed=no
  while IFS= read -r path; do
    [ -n "${path}" ] || continue
    restore_path_from_baseline "${path}" || restore_failed=yes
  done <"${restore_list}"
  [ "${restore_failed}" = no ]
}

remove_untracked_additions() {
  [ "${baseline_backup_ready}" = yes ] || return 0
  untracked_list=${probe_directory}/untracked-additions
  git -C "${repository_root}" -c core.quotepath=false ls-files \
    --others --exclude-standard >"${untracked_list}" || return 1
  additions_found=no
  while IFS= read -r path; do
    [ -n "${path}" ] || continue
    case "/${path}/" in
      */../* | */./*) return 1 ;;
    esac
    destination_path=${repository_root}/${path}
    if [ -d "${destination_path}" ] && [ ! -L "${destination_path}" ]; then
      find "${destination_path}" -depth -delete >/dev/null 2>&1 || return 1
    else
      rm -f -- "${destination_path}" >/dev/null 2>&1 || return 1
    fi
    parent=$(dirname -- "${path}")
    while [ "${parent}" != . ] && [ "${parent}" != / ]; do
      rmdir -- "${repository_root}/${parent}" >/dev/null 2>&1 || break
      parent=$(dirname -- "${parent}")
    done
    additions_found=yes
  done <"${untracked_list}"
  [ "${additions_found}" = no ]
}

verify_tracked_state() {
  expected_edit=$1
  verification_list=${probe_directory}/tracked-diff
  git -C "${repository_root}" diff --name-only "${expected_baseline}" -- \
    >"${verification_list}" || fail "could not verify tracked worktree state"
  collateral=no

  while IFS= read -r path; do
    [ -n "${path}" ] || continue
    if [ -n "${expected_edit}" ] && [ "${path}" = "${expected_edit}" ]; then
      continue
    fi
    restore_path_from_baseline "${path}" ||
      fail "a probe command changed tracked state that could not be restored safely"
    collateral=yes
  done <"${verification_list}"

  if [ -n "${expected_edit}" ]; then
    if [ ! -f "${edit_output}" ] ||
      ! cmp -s -- "${edit_output}" "${repository_root}/${expected_edit}" ||
      ! git -C "${repository_root}" diff --cached --quiet -- "${expected_edit}"; then
      git -C "${repository_root}" restore --staged --source="${expected_baseline}" -- \
        "${expected_edit}" >/dev/null 2>&1 ||
        fail "the selected edit changed tracked state that could not be restored safely"
      cp -p -- "${edit_output}" "${repository_root}/${expected_edit}" ||
        fail "the selected edit could not be restored safely"
      collateral=yes
    fi
  fi

  if ! remove_untracked_additions; then
    collateral=yes
  fi

  if [ "${collateral}" = yes ]; then
    fail "a probe command created or mutated worktree state outside the selected temporary edit; state was restored and the run is non-adjudicable"
  fi
}

prepare_baseline_tree() {
  baseline_archive=${probe_directory}/baseline.tar
  baseline_tree_directory=${probe_directory}/tracked-baseline
  mkdir -p -- "${baseline_tree_directory}"
  git -C "${repository_root}" archive --format=tar \
    --output="${baseline_archive}" "${expected_baseline}" ||
    fail "could not create the tracked baseline backup"
  tar -xf "${baseline_archive}" -C "${baseline_tree_directory}" ||
    fail "could not extract the tracked baseline backup"
  baseline_backup_ready=yes
  verify_tracked_state ""
}

require_repository_state() {
  actual_root=$(git -C "${repository_root}" rev-parse --show-toplevel 2>/dev/null) ||
    fail "the probe must run from a Git worktree"
  actual_root=$(
    CDPATH=''
    cd -- "${actual_root}"
    pwd -P
  )
  [ "${actual_root}" = "${repository_root}" ] ||
    fail "the probe path does not match the Git worktree root"

  git_directory=$(git -C "${repository_root}" rev-parse --absolute-git-dir)
  common_directory=$(git -C "${repository_root}" rev-parse --git-common-dir)
  case "${common_directory}" in
    /*) ;;
    *) common_directory=${repository_root}/${common_directory} ;;
  esac
  git_directory=$(
    CDPATH=''
    cd -- "${git_directory}"
    pwd -P
  )
  common_directory=$(
    CDPATH=''
    cd -- "${common_directory}"
    pwd -P
  )
  [ "${git_directory}" != "${common_directory}" ] ||
    fail "use a linked Git worktree dedicated to this audit"
  if git -C "${repository_root}" symbolic-ref --quiet HEAD >/dev/null 2>&1; then
    fail "the audit worktree must have a detached HEAD at --expected-baseline"
  fi

  if [ -n "$(git -C "${repository_root}" status --porcelain=v1 --untracked-files=all)" ]; then
    fail "working tree is not clean; commit or stash before probing"
  fi

  [ "${#expected_baseline}" -eq 40 ] ||
    fail "--expected-baseline must be a full 40-character lowercase commit SHA"
  case "${expected_baseline}" in
    *[!0-9a-f]*) fail "--expected-baseline must be a full 40-character lowercase commit SHA" ;;
  esac
  resolved_baseline=$(git -C "${repository_root}" rev-parse --verify \
    "${expected_baseline}^{commit}" 2>/dev/null) || fail "expected baseline is not a commit"
  [ "${resolved_baseline}" = "${expected_baseline}" ] ||
    fail "expected baseline did not resolve exactly"
  actual_baseline=$(git -C "${repository_root}" rev-parse HEAD)
  [ "${actual_baseline}" = "${expected_baseline}" ] ||
    fail "HEAD does not match --expected-baseline"
}

risky_environment_is_set() {
  for variable in \
    AWS_ACCESS_KEY_ID \
    AWS_SECRET_ACCESS_KEY \
    AWS_SESSION_TOKEN \
    AWS_SECURITY_TOKEN \
    AWS_PROFILE \
    AWS_DEFAULT_PROFILE \
    AWS_CONFIG_FILE \
    AWS_SHARED_CREDENTIALS_FILE \
    AWS_WEB_IDENTITY_TOKEN_FILE \
    AWS_ROLE_ARN \
    AWS_ROLE_SESSION_NAME \
    AWS_CONTAINER_CREDENTIALS_FULL_URI \
    AWS_CONTAINER_CREDENTIALS_RELATIVE_URI \
    AWS_CONTAINER_AUTHORIZATION_TOKEN \
    AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE \
    AWS_REGION \
    AWS_DEFAULT_REGION \
    AWS_ENDPOINT_URL \
    AWS_ENDPOINT_URL_BRAKET \
    AWS_ENDPOINT_URL_S3 \
    AWS_ENDPOINT_URL_STS \
    AWS_S3_BUCKET \
    AMZN_BRAKET_TASK_RESULTS_S3_URI \
    AMAZON_BRAKET_DEVICE_ARN \
    AMAZON_BRAKET_RESERVATION_ARN \
    AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG \
    AMAZON_BRAKET_QDMI_TEST_ALLOW_BUCKET_CREATION \
    IQM_DEVICE_ARN \
    AMAZON_BRAKET_PENNYLANE_LIVE; do
    if env | grep -q "^${variable}="; then
      return 0
    fi
  done
  return 1
}

configure_cpp() {
  log=$1
  live_tests=OFF
  [ "${live}" = yes ] && live_tests=ON
  if AWS_EC2_METADATA_DISABLED=true cmake \
    -S "${repository_root}" \
    -B "${probe_directory}/cpp-build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_COVERAGE=ON \
    -DBUILD_AMAZON_BRAKET_TESTS=ON \
    -DBUILD_AMAZON_BRAKET_LIVE_TESTS="${live_tests}" \
    >"${log}" 2>&1; then
    command_status=0
  else
    command_status=$?
  fi
  verify_tracked_state "${edited_file}"
  return "${command_status}"
}

build_cpp() {
  log=$1
  if AWS_EC2_METADATA_DISABLED=true cmake --build "${probe_directory}/cpp-build" \
    --target "${target}" --parallel >"${log}" 2>&1; then
    command_status=0
  else
    command_status=$?
  fi
  verify_tracked_state "${edited_file}"
  return "${command_status}"
}

clear_cpp_coverage() {
  find "${probe_directory}/cpp-build" -type f -name '*.gcda' -exec rm -f -- {} +
  verify_tracked_state "${edited_file}"
}

run_cpp_suite() {
  log=$1
  if [ -n "${ctest_filter}" ]; then
    if AWS_EC2_METADATA_DISABLED=true ctest --test-dir "${probe_directory}/cpp-build" \
      --output-on-failure --no-tests=error -R "${ctest_filter}" >"${log}" 2>&1; then
      command_status=0
    else
      command_status=$?
    fi
  else
    if AWS_EC2_METADATA_DISABLED=true ctest --test-dir "${probe_directory}/cpp-build" \
      --output-on-failure --no-tests=error >"${log}" 2>&1; then
      command_status=0
    else
      command_status=$?
    fi
  fi
  verify_tracked_state "${edited_file}"
  return "${command_status}"
}

cpp_coverage() {
  log=$1
  if command -v gcovr >/dev/null 2>&1; then
    if gcovr --root "${repository_root}" --object-directory "${probe_directory}/cpp-build" \
      --filter "${repository_root}/${source_path}" --print-summary >"${log}" 2>&1; then
      command_status=0
    else
      command_status=$?
    fi
  else
    if UV_CACHE_DIR="${probe_directory}/uv-cache" uvx gcovr \
      --root "${repository_root}" --object-directory "${probe_directory}/cpp-build" \
      --filter "${repository_root}/${source_path}" --print-summary >"${log}" 2>&1; then
      command_status=0
    else
      command_status=$?
    fi
  fi
  verify_tracked_state "${edited_file}"
  [ "${command_status}" -eq 0 ] || return "${command_status}"
  line_counts=$(sed -n \
    's/^lines:.*(\([0-9][0-9]*\) out of \([0-9][0-9]*\)).*/\1 \2/p' \
    "${log}" | tail -n 1)
  branch_counts=$(sed -n \
    's/^branches:.*(\([0-9][0-9]*\) out of \([0-9][0-9]*\)).*/\1 \2/p' \
    "${log}" | tail -n 1)
  [ -n "${line_counts}" ] && [ -n "${branch_counts}" ] || return 1
  IFS=' ' read -r line_covered line_total branch_covered branch_total <<EOF
${line_counts} ${branch_counts}
EOF
  printf '%s %s %s %s %s %s\n' \
    "${line_total}" "${line_covered}" "$((line_total - line_covered))" \
    "${branch_total}" "${branch_covered}" "$((branch_total - branch_covered))"
}

run_python_suite() {
  log=$1
  env_directory=$2
  coverage_source=$3
  deselected_node=$4
  coverage_json=${5-}

  if [ -n "${coverage_source}" ] && [ -n "${deselected_node}" ]; then
    if AWS_EC2_METADATA_DISABLED=true \
      NO_COLOR=1 \
      TMPDIR="${probe_directory}" \
      UV_CACHE_DIR="${probe_directory}/uv-cache" \
      SKBUILD_BUILD_DIR="${probe_directory}/python-build" \
      COVERAGE_FILE="${probe_directory}/coverage" \
      uvx nox --envdir "${env_directory}" -s "${nox_session}" -- \
      "${tests_path}" --deselect "${deselected_node}" --cov="${coverage_source}" \
      --cov-branch --cov-report="json:${coverage_json}" >"${log}" 2>&1; then
      command_status=0
    else
      command_status=$?
    fi
  elif [ -n "${coverage_source}" ]; then
    if AWS_EC2_METADATA_DISABLED=true \
      NO_COLOR=1 \
      TMPDIR="${probe_directory}" \
      UV_CACHE_DIR="${probe_directory}/uv-cache" \
      SKBUILD_BUILD_DIR="${probe_directory}/python-build" \
      COVERAGE_FILE="${probe_directory}/coverage" \
      uvx nox --envdir "${env_directory}" -s "${nox_session}" -- \
      "${tests_path}" --cov="${coverage_source}" --cov-branch \
      --cov-report="json:${coverage_json}" >"${log}" 2>&1; then
      command_status=0
    else
      command_status=$?
    fi
  else
    if AWS_EC2_METADATA_DISABLED=true \
      NO_COLOR=1 \
      TMPDIR="${probe_directory}" \
      UV_CACHE_DIR="${probe_directory}/uv-cache" \
      SKBUILD_BUILD_DIR="${probe_directory}/python-build" \
      COVERAGE_FILE="${probe_directory}/coverage" \
      uvx nox --envdir "${env_directory}" -s "${nox_session}" -- \
      "${tests_path}" >"${log}" 2>&1; then
      command_status=0
    else
      command_status=$?
    fi
  fi
  verify_tracked_state "${edited_file}"
  return "${command_status}"
}

python_coverage() {
  report=$1
  [ -s "${report}" ] || return 1
  statement_total=$(sed -n 's/.*"num_statements": \([0-9][0-9]*\).*/\1/p' \
    "${report}" | tail -n 1)
  statement_covered=$(sed -n 's/.*"covered_lines": \([0-9][0-9]*\).*/\1/p' \
    "${report}" | tail -n 1)
  statement_missing=$(sed -n 's/.*"missing_lines": \([0-9][0-9]*\).*/\1/p' \
    "${report}" | tail -n 1)
  branch_total=$(sed -n 's/.*"num_branches": \([0-9][0-9]*\).*/\1/p' \
    "${report}" | tail -n 1)
  branch_covered=$(sed -n 's/.*"covered_branches": \([0-9][0-9]*\).*/\1/p' \
    "${report}" | tail -n 1)
  branch_missing=$(sed -n 's/.*"missing_branches": \([0-9][0-9]*\).*/\1/p' \
    "${report}" | tail -n 1)
  for metric in "${statement_total}" "${statement_covered}" "${statement_missing}" \
    "${branch_total}" "${branch_covered}" "${branch_missing}"; do
    [ -n "${metric}" ] || return 1
  done
  [ "${statement_total}" -eq $((statement_covered + statement_missing)) ] || return 1
  [ "${branch_total}" -eq $((branch_covered + branch_missing)) ] || return 1
  printf '%s %s %s %s %s %s\n' \
    "${statement_total}" "${statement_covered}" "${statement_missing}" \
    "${branch_total}" "${branch_covered}" "${branch_missing}"
}

sanitize_failure_ids() {
  sed \
    -e 's/\[[^]]*\]/[parameters redacted]/g' \
    -e 's#arn:[^][[:space:]]*#<ARN redacted>#g' \
    -e 's#s3://[^][[:space:]]*#<S3 location redacted>#g' \
    -e 's/[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]/<account redacted>/g' |
    LC_ALL=C sort -u
}

python_failure_ids() {
  log=$1
  sed -n 's/^FAILED \(.*\) - .*/\1/p' "${log}" | sanitize_failure_ids
}

cpp_failure_ids() {
  log=$1
  sed -n 's/^[[:space:]]*[0-9][0-9]* - \(.*\) (Failed)$/\1/p' "${log}" |
    sanitize_failure_ids
}

spank_failure_ids() {
  log=$1
  candidates=${probe_directory}/spank-failure-candidates
  sed -n 's/^FAILED \(.*\) - .*/\1/p' "${log}" >"${candidates}"
  if [ ! -s "${candidates}" ]; then
    sed -n 's/^=== \(.*\) ===$/spank: \1/p' "${log}" | tail -n 1 >"${candidates}"
  fi
  if [ ! -s "${candidates}" ]; then
    echo "spank/test/run_docker_tests.sh" >"${candidates}"
  fi
  sanitize_failure_ids <"${candidates}"
}

run_spank_suite() {
  phase=$1
  spank_failure_stage=none
  build_log=${probe_directory}/spank-${phase}-build.log
  suite_log=${probe_directory}/spank-${phase}-suite.log
  iid_file=${probe_directory}/spank-${phase}.iid

  if docker build --iidfile "${iid_file}" --file "${spank_context}/spank/Dockerfile" \
    "${spank_context}" >"${build_log}" 2>&1; then
    command_status=0
  else
    command_status=$?
  fi
  verify_tracked_state "${edited_file}"
  if [ "${command_status}" -ne 0 ]; then
    spank_failure_stage=build
    return "${command_status}"
  fi
  image_id=$(sed -n '1p' "${iid_file}")
  if [ -z "${image_id}" ]; then
    spank_failure_stage=build
    return 2
  fi
  printf '%s\n' "${image_id}" >>"${docker_images_file}"
  if docker run --rm --privileged "${image_id}" >"${suite_log}" 2>&1; then
    command_status=0
  else
    command_status=$?
  fi
  verify_tracked_state "${edited_file}"
  if [ "${command_status}" -ne 0 ]; then
    spank_failure_stage=suite
    return "${command_status}"
  fi
  return 0
}

print_header() {
  title=$1
  echo "=== SpecAudit probe: ${title} ==="
  echo "baseline commit : ${actual_baseline}"
  echo "language        : ${lang}"
  if [ "${live}" = yes ]; then
    echo "execution       : live (batch acknowledged)"
    echo "live batch      : ${batch_id}"
  else
    echo "execution       : offline"
  fi
}

tier=""
lang=""
expected_baseline=""
source_path=""
tests_path=""
target=""
ctest_filter=""
nox_session="tests"
drop_node=""
omit_spec=""
inject_spec=""
replacement=""
replacement_set=no
live=no
batch_id=""

[ "$#" -ge 1 ] || {
  usage
  exit 2
}

case "$1" in
  t1 | t2)
    tier=$1
    shift
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *) fail "first argument must be t1, t2, or --help" ;;
esac

while [ "$#" -gt 0 ]; do
  case "$1" in
    --expected-baseline)
      need_value "$1" "$#"
      expected_baseline=$2
      shift 2
      ;;
    --lang)
      need_value "$1" "$#"
      lang=$2
      shift 2
      ;;
    --source)
      need_value "$1" "$#"
      source_path=$2
      shift 2
      ;;
    --tests)
      need_value "$1" "$#"
      tests_path=$2
      shift 2
      ;;
    --target)
      need_value "$1" "$#"
      target=$2
      shift 2
      ;;
    --ctest)
      need_value "$1" "$#"
      ctest_filter=$2
      shift 2
      ;;
    --nox-session)
      need_value "$1" "$#"
      nox_session=$2
      shift 2
      ;;
    --drop)
      need_value "$1" "$#"
      drop_node=$2
      shift 2
      ;;
    --omit)
      need_value "$1" "$#"
      omit_spec=$2
      shift 2
      ;;
    --inject)
      need_value "$1" "$#"
      inject_spec=$2
      shift 2
      ;;
    --with)
      need_value "$1" "$#"
      replacement=$2
      replacement_set=yes
      shift 2
      ;;
    --live)
      live=yes
      shift
      ;;
    --batch-id)
      need_value "$1" "$#"
      batch_id=$2
      shift 2
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    --keep) fail "--keep is intentionally unsupported; probe edits are always restored" ;;
    *) fail "unknown option" ;;
  esac
done

[ -n "${expected_baseline}" ] || fail "--expected-baseline is required"
case "${lang}" in
  python | cpp | spank) ;;
  *) fail "--lang must be python, cpp, or spank" ;;
esac
case "${nox_session}" in
  tests | tests-3.10 | tests-3.11 | tests-3.12 | tests-3.13 | tests-3.14) ;;
  *) fail "--nox-session must be tests or tests-3.10 through tests-3.14" ;;
esac
case "${target}" in
  *[!A-Za-z0-9_.+-]*) fail "--target contains unsupported characters" ;;
esac

if [ "${live}" = yes ]; then
  [ -n "${batch_id}" ] || fail "--live requires --batch-id"
  case "${batch_id}" in
    *[!A-Za-z0-9_.-]* | [-.]* | '') fail "--batch-id contains unsupported characters" ;;
  esac
  [ "${lang}" != spank ] || fail "SPANK probes use a local fixture and cannot be live"
  if [ "${lang}" = cpp ]; then
    [ -n "${ctest_filter}" ] || fail "live C++ probes require a bounded --ctest selection"
  fi
else
  [ -z "${batch_id}" ] || fail "--batch-id requires --live"
fi

require_repository_state

if [ "${live}" = no ] && risky_environment_is_set; then
  fail "offline probes refuse exported AWS credential, Region, result, reservation, endpoint, and live-test variables"
fi

if [ "${tier}" = t1 ]; then
  [ "${lang}" != spank ] || fail "t1 does not support SPANK; use the Docker-backed t2 probe"
  [ -n "${source_path}" ] || fail "t1 requires --source"
  validate_relative_path "${source_path}" "--source"
  if [ "${lang}" = python ]; then
    [ -n "${tests_path}" ] || fail "t1 --lang python requires --tests"
    validate_relative_path "${tests_path}" "--tests"
    if [ -n "${drop_node}" ] && [ -n "${omit_spec}" ]; then
      fail "t1 accepts either --drop or --omit, not both"
    fi
    [ -n "${drop_node}" ] || [ -n "${omit_spec}" ] ||
      fail "t1 --lang python requires --drop or --omit"
  else
    [ -n "${target}" ] || fail "t1 --lang cpp requires --target"
    [ -n "${omit_spec}" ] || fail "t1 --lang cpp requires --omit"
    [ -z "${drop_node}" ] || fail "--drop applies only to Python"
  fi
else
  [ -n "${inject_spec}" ] || fail "t2 requires --inject"
  [ "${replacement_set}" = yes ] || fail "t2 requires --with, which may be empty"
  case "${lang}" in
    python)
      [ -n "${tests_path}" ] || fail "t2 --lang python requires --tests"
      validate_relative_path "${tests_path}" "--tests"
      ;;
    cpp)
      [ -n "${target}" ] || fail "t2 --lang cpp requires --target"
      ;;
    spank)
      command -v docker >/dev/null 2>&1 || fail "SPANK probes require Docker"
      ;;
  esac
fi

if [ "${tier}" = t1 ]; then
  if [ -n "${omit_spec}" ]; then
    parse_edit_spec "${omit_spec}" "--omit"
  fi
else
  parse_edit_spec "${inject_spec}" "--inject"
  if [ "${lang}" = spank ]; then
    case "${parsed_file}" in
      spank/*.cpp | spank/*.h | spank/*.hpp)
        spank_relative_file=${parsed_file#spank/}
        case "${spank_relative_file}" in
          */*) fail "SPANK probes accept only production source files directly under spank/" ;;
        esac
        ;;
      *) fail "SPANK probes accept only production .cpp/.h/.hpp files directly under spank/" ;;
    esac
  fi
fi

probe_directory=$(mktemp -d "${TMPDIR:-/tmp}/amazon-braket-audit-probe.XXXXXX") ||
  fail "could not create the isolated probe directory"
docker_images_file=${probe_directory}/docker-images
: >"${docker_images_file}"
if [ "${live}" = no ]; then
  offline_aws_config=${probe_directory}/empty-aws-config
  offline_aws_credentials=${probe_directory}/empty-aws-credentials
  : >"${offline_aws_config}"
  : >"${offline_aws_credentials}"
  AWS_CONFIG_FILE=${offline_aws_config}
  AWS_SHARED_CREDENTIALS_FILE=${offline_aws_credentials}
  AWS_EC2_METADATA_DISABLED=true
  unset AWS_PROFILE AWS_DEFAULT_PROFILE
  export AWS_CONFIG_FILE AWS_SHARED_CREDENTIALS_FILE
  export AWS_EC2_METADATA_DISABLED
fi
prepare_baseline_tree
if [ "${lang}" = spank ]; then
  spank_context=${probe_directory}/spank-context
  mkdir -p -- "${spank_context}"
  cp -pPR -- "${baseline_tree_directory}/." "${spank_context}/" ||
    fail "could not create the tracked-only SPANK Docker context"
  verify_tracked_state ""
fi

if [ "${tier}" = t1 ]; then
  if [ "${lang}" = python ]; then
    baseline_log=${probe_directory}/python-baseline.log
    baseline_report=${probe_directory}/python-baseline-coverage.json
    if ! run_python_suite "${baseline_log}" "${probe_directory}/nox-baseline" \
      "${source_path}" "" "${baseline_report}"; then
      fail "baseline Python suite failed; no evidence block was produced"
    fi
    baseline_coverage=$(python_coverage "${baseline_report}") ||
      fail "baseline Python coverage summary was unavailable"
    IFS=' ' read -r baseline_statement_total baseline_statement_covered \
      baseline_statement_missing baseline_branch_total baseline_branch_covered \
      baseline_branch_missing <<EOF
${baseline_coverage}
EOF

    if [ -n "${omit_spec}" ]; then
      apply_edit "${omit_spec}" ""
      omission_label=${omit_spec}
      omitted_node=""
    else
      omission_label="pytest node deselected"
      omitted_node=${drop_node}
    fi
    modified_log=${probe_directory}/python-modified.log
    modified_report=${probe_directory}/python-modified-coverage.json
    if ! run_python_suite "${modified_log}" "${probe_directory}/nox-modified" \
      "${source_path}" "${omitted_node}" "${modified_report}"; then
      fail "Python suite without the assertion failed; the temporary edit was restored"
    fi
    modified_coverage=$(python_coverage "${modified_report}") ||
      fail "modified Python coverage summary was unavailable"
    IFS=' ' read -r modified_statement_total modified_statement_covered \
      modified_statement_missing modified_branch_total modified_branch_covered \
      modified_branch_missing <<EOF
${modified_coverage}
EOF

    print_header "T1 coverage delta"
    echo "source          : ${source_path}"
    echo "suite           : ${tests_path}"
    echo "omission        : ${omission_label}"
    echo "baseline suite  : pass"
    echo "modified suite  : pass"
    echo "with statements : total=${baseline_statement_total} covered=${baseline_statement_covered} missing=${baseline_statement_missing}"
    echo "without stmts   : total=${modified_statement_total} covered=${modified_statement_covered} missing=${modified_statement_missing}"
    echo "statement delta : total=$((modified_statement_total - baseline_statement_total)) covered=$((modified_statement_covered - baseline_statement_covered)) missing=$((modified_statement_missing - baseline_statement_missing))"
    echo "with branches   : total=${baseline_branch_total} covered=${baseline_branch_covered} missing=${baseline_branch_missing}"
    echo "without branches: total=${modified_branch_total} covered=${modified_branch_covered} missing=${modified_branch_missing}"
    echo "branch delta    : total=$((modified_branch_total - baseline_branch_total)) covered=$((modified_branch_covered - baseline_branch_covered)) missing=$((modified_branch_missing - baseline_branch_missing))"
  else
    if ! configure_cpp "${probe_directory}/cpp-configure.log"; then
      fail "baseline C++ configuration failed; no evidence block was produced"
    fi
    if ! build_cpp "${probe_directory}/cpp-baseline-build.log"; then
      fail "baseline C++ build failed; no evidence block was produced"
    fi
    clear_cpp_coverage
    if ! run_cpp_suite "${probe_directory}/cpp-baseline-suite.log"; then
      fail "baseline C++ suite failed; no evidence block was produced"
    fi
    baseline_coverage=$(cpp_coverage "${probe_directory}/cpp-baseline-coverage.log") ||
      fail "baseline C++ coverage summary was unavailable"
    IFS=' ' read -r baseline_line_total baseline_line_covered baseline_line_missing \
      baseline_branch_total baseline_branch_covered baseline_branch_missing <<EOF
${baseline_coverage}
EOF

    apply_edit "${omit_spec}" ""
    if ! build_cpp "${probe_directory}/cpp-modified-build.log"; then
      fail "C++ build without the assertion failed; the temporary edit was restored"
    fi
    clear_cpp_coverage
    if ! run_cpp_suite "${probe_directory}/cpp-modified-suite.log"; then
      fail "C++ suite without the assertion failed; the temporary edit was restored"
    fi
    modified_coverage=$(cpp_coverage "${probe_directory}/cpp-modified-coverage.log") ||
      fail "modified C++ coverage summary was unavailable"
    IFS=' ' read -r modified_line_total modified_line_covered modified_line_missing \
      modified_branch_total modified_branch_covered modified_branch_missing <<EOF
${modified_coverage}
EOF

    print_header "T1 coverage delta"
    echo "source          : ${source_path}"
    echo "suite           : ${target}"
    echo "omission        : ${omit_spec}"
    echo "baseline suite  : pass"
    echo "modified suite  : pass"
    echo "with lines      : total=${baseline_line_total} covered=${baseline_line_covered} missing=${baseline_line_missing}"
    echo "without lines   : total=${modified_line_total} covered=${modified_line_covered} missing=${modified_line_missing}"
    echo "line delta      : total=$((modified_line_total - baseline_line_total)) covered=$((modified_line_covered - baseline_line_covered)) missing=$((modified_line_missing - baseline_line_missing))"
    echo "with branches   : total=${baseline_branch_total} covered=${baseline_branch_covered} missing=${baseline_branch_missing}"
    echo "without branches: total=${modified_branch_total} covered=${modified_branch_covered} missing=${modified_branch_missing}"
    echo "branch delta    : total=$((modified_branch_total - baseline_branch_total)) covered=$((modified_branch_covered - baseline_branch_covered)) missing=$((modified_branch_missing - baseline_branch_missing))"
  fi
  echo "reading         : equal coverage is evidence of redundancy, never proof"
  echo "next tier       : restore the assertion and use t2 before changing it"
  exit 0
fi

case "${lang}" in
  python)
    set +e
    run_python_suite "${probe_directory}/python-baseline.log" \
      "${probe_directory}/nox-baseline" "" ""
    baseline_status=$?
    set -e
    baseline_failures_file=${probe_directory}/baseline-failures
    python_failure_ids "${probe_directory}/python-baseline.log" >"${baseline_failures_file}"
    baseline_failure_count=$(grep -c . "${baseline_failures_file}" || true)
    if [ "${baseline_status}" -ne 0 ] || [ "${baseline_failure_count}" -gt 0 ]; then
      fail "baseline Python suite failed; no evidence block was produced"
    fi
    apply_edit "${inject_spec}" "${replacement}"
    set +e
    run_python_suite "${probe_directory}/python-fault.log" \
      "${probe_directory}/nox-fault" "" ""
    fault_status=$?
    set -e
    fault_failures_file=${probe_directory}/fault-failures
    python_failure_ids "${probe_directory}/python-fault.log" >"${fault_failures_file}"
    fault_failure_count=$(grep -c . "${fault_failures_file}" || true)
    if [ "${fault_failure_count}" -gt 0 ]; then
      fault_result=fail
    elif [ "${fault_status}" -eq 0 ]; then
      fault_result=pass
    else
      fault_result=runner-error
    fi
    suite_label=${tests_path}
    fault_build=pass
    ;;
  cpp)
    if ! configure_cpp "${probe_directory}/cpp-configure.log"; then
      fail "baseline C++ configuration failed; no evidence block was produced"
    fi
    if ! build_cpp "${probe_directory}/cpp-baseline-build.log"; then
      fail "baseline C++ build failed; no evidence block was produced"
    fi
    if ! run_cpp_suite "${probe_directory}/cpp-baseline-suite.log"; then
      fail "baseline C++ suite failed; no evidence block was produced"
    fi
    apply_edit "${inject_spec}" "${replacement}"
    fault_failures_file=${probe_directory}/fault-failures
    : >"${fault_failures_file}"
    fault_failure_count=0
    if build_cpp "${probe_directory}/cpp-fault-build.log"; then
      fault_build=pass
      set +e
      run_cpp_suite "${probe_directory}/cpp-fault-suite.log"
      fault_status=$?
      set -e
      cpp_failure_ids "${probe_directory}/cpp-fault-suite.log" >"${fault_failures_file}"
      fault_failure_count=$(grep -c . "${fault_failures_file}" || true)
      if [ "${fault_failure_count}" -gt 0 ]; then
        fault_result=fail
      elif [ "${fault_status}" -eq 0 ]; then
        fault_result=pass
      else
        fault_result=runner-error
      fi
    else
      fault_build=fail
      fault_result=not-run
    fi
    suite_label=${target}
    ;;
  spank)
    set +e
    run_spank_suite baseline
    baseline_status=$?
    set -e
    [ "${baseline_status}" -eq 0 ] ||
      fail "baseline SPANK Docker build or suite failed; no evidence block was produced"
    apply_edit "${inject_spec}" "${replacement}"
    cp -pP -- "${repository_root}/${edited_file}" "${spank_context}/${edited_file}" ||
      fail "could not overlay the selected edit onto the tracked-only SPANK context"
    verify_tracked_state "${edited_file}"
    set +e
    run_spank_suite fault
    fault_status=$?
    set -e
    fault_failures_file=${probe_directory}/fault-failures
    : >"${fault_failures_file}"
    fault_failure_count=0
    if [ "${spank_failure_stage}" = build ]; then
      fault_build=infrastructure-error
      fault_result=not-run
    else
      case "${fault_status}" in
      0)
        fault_build=pass
        fault_result=pass
        ;;
      1)
        fault_build=pass
        fault_result=fail
        spank_failure_ids "${probe_directory}/spank-fault-suite.log" >"${fault_failures_file}"
        fault_failure_count=$(grep -c . "${fault_failures_file}" || true)
        ;;
      *)
        fault_build=pass
        fault_result=infrastructure-error
        ;;
      esac
    fi
    suite_label="SPANK Docker suite"
    ;;
esac

print_header "T2 fault injection"
echo "injected        : ${inject_spec}"
echo "suite           : ${suite_label}"
echo "baseline build  : pass"
echo "baseline suite  : pass"
echo "fault build     : ${fault_build}"
echo "fault suite     : ${fault_result}"
echo "failing tests   : ${fault_failure_count}"
if [ "${fault_failure_count}" -gt 0 ]; then
  sed 's/^/  - /' "${fault_failures_file}"
fi
if [ "${fault_build}" = fail ]; then
  echo "reading         : the injected fault does not compile; the run is non-adjudicable"
elif [ "${fault_build}" = infrastructure-error ]; then
  echo "reading         : the Docker build failed; no detection verdict is valid"
elif [ "${fault_result}" = fail ]; then
  echo "reading         : the selected suite detects the injected fault"
elif [ "${fault_result}" = runner-error ]; then
  echo "reading         : the test runner failed without identifying a failing test"
elif [ "${fault_result}" = infrastructure-error ]; then
  echo "reading         : the Docker suite did not complete; no detection verdict is valid"
else
  echo "reading         : the selected suite does not detect the injected fault"
fi
echo "privacy         : raw output and replacement text suppressed"
if [ "${fault_build}" != pass ] || [ "${fault_result}" = runner-error ] ||
  [ "${fault_result}" = infrastructure-error ]; then
  exit 1
fi
