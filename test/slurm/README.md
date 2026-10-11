# Amazon Braket Slurm smoke test

This test runs Qiskit and PennyLane workloads against Amazon Braket SV1 in
[MQT Core's Slurm cluster example](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm_cluster.html).
It requires AWS credentials with Braket and S3 access in `us-east-1`. Each
installation mode submits two tasks with eight shots each; AWS charges apply.

Check out the MQT Core revision pinned in the Slurm workflow and set
`CORE_SOURCE` to that checkout. Run the commands below from this repository.
Docker builds MQT Core and the device implementation inside the Linux image. Use
Docker Engine on Linux or Docker Desktop on macOS, with uv on the host. Follow
the
[cluster setup](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm_cluster.html#start-the-cluster-and-log-in)
for the Docker configuration and host requirements.

```sh
PROVIDER_INSTALL_MODE=native uv run --no-project \
  "$CORE_SOURCE/test/slurm/run_integration.py" \
  --workload . \
  --setup-script test/slurm/setup.sh \
  --compose-file test/slurm/compose.yml \
  --device-license amazon.braket.sv1 \
  --qdmi-config-file /opt/provider-catalogue.json \
  -- python3 /workload/test/slurm/workload.py
```

Repeat with `PROVIDER_INSTALL_MODE=wheel`. Both modes install the Python
adapters. Native mode selects the separately installed Runtime catalogue; wheel
mode selects the bundled library. The workload checks the loaded library path
and submits and retrieves eight Bell-state shots through each SDK. The workload
runs as the unprivileged `mqt-dev` user. Only the `amazon.braket.sv1` catalogue
entry is enabled, using the standard regional S3 result bucket.

Export `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, and, for temporary
credentials, `AWS_SESSION_TOKEN` before running the command. The Compose overlay
passes them to the login node for job submission and to the controller for
availability monitoring. Slurm exports the login node's submission environment
to the job. This smoke test uses one identity for both roles; production
deployments should configure separate site-owned monitor credentials. Use
short-lived credentials and keep their values out of command arguments, build
arguments, and logs. CI receives the same three secrets and runs both
installation modes.

See [Amazon Braket on Slurm](../../docs/slurm.md) for deployment.

For IQM and Braket jobs on the same cluster, use the
[MQT Core multi-vendor example](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm_cluster.html#multiple-device-implementations).
MQT Core and all device implementations share one Python environment. Each job
opens its allocated device through the QDMI Client Interface, with that device's
credentials. All quantum access nodes share the `quantum` partition and can
reach every configured device. The cluster monitors each device before admitting
its jobs. `setup.sh [OUTPUT]` writes the catalogue to the chosen path.

## Credentialed checks for pull requests from forks

Fork and Dependabot CI runs without cloud credentials. Credentialed jobs are
visibly skipped; passing those offline checks does not establish device
integration. For a reviewed fork commit, a maintainer can run the upstream Slurm
workflow without copying the branch into the upstream repository:

```bash
gh workflow run slurm-reviewed.yml --repo munich-quantum-software/amazon-braket-qdmi-device --ref main \
  -f pull-request=237 -f reviewed-sha=FULL_REVIEWED_COMMIT_SHA
```

Review the exact commit, including its build scripts, dependencies, setup and
workload, before dispatching: that code will receive the test credentials. The
workflow verifies that the SHA is still the PR head and checks out that
immutable commit. A later push requires another review and dispatch. The
workflow itself and its reusable job come from upstream `main`; this entry point
becomes available after the workflow is merged there. Attach the run URL to the
PR as separate Slurm evidence. Full credentialed CI and executable notebooks
continue to run on trusted branches and main.

GitHub's
[fork-run approval](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/approve-runs-from-forks)
does not grant repository secrets. A protected
[environment](https://docs.github.com/en/actions/how-tos/deploy/configure-and-manage-deployments/manage-environments)
can add a reviewer gate and hold environment secrets, but requires administrator
configuration. It does not make unreviewed code safe to execute with
credentials.
