# Amazon Braket Slurm smoke test

This test runs Qiskit and PennyLane workloads against Amazon Braket SV1 in
[MQT Core's Slurm cluster example](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm_cluster.html).
It requires AWS credentials with Braket and S3 access in `us-east-1`. Each
installation mode submits two tasks with eight shots each; AWS charges apply.

Use the MQT Core revision pinned in the Slurm workflow and build its Linux wheel
with
`uv build --python 3.15 --wheel --out-dir "$CORE_DIST" -Ccmake.define.DEPLOY=ON`
from that checkout. The wheel must match the host architecture and Python 3.15.
The example deploys Slurm 25.11 or newer through rootful Docker on a disposable
Linux cgroup-v2 host.

```sh
PROVIDER_INSTALL_MODE=native uv run --no-project \
  "$CORE_SOURCE/test/slurm/run_integration.py" \
  --workload . --dist "$CORE_DIST" \
  --setup-script test/slurm/setup.sh \
  --compose-file test/slurm/compose.yml \
  --device-license amazon.braket.sv1 \
  --qdmi-config-file /opt/provider-catalogue.json \
  -- python3 /workload/test/slurm/probe.py
```

Repeat with `PROVIDER_INSTALL_MODE=wheel`. Both modes install the Python
adapters. Native mode selects the separately installed Runtime catalogue; wheel
mode selects the bundled library. The workload checks the loaded library path
and submits and retrieves eight Bell-state shots through each SDK. The workload
runs as an unprivileged user. Only the `amazon.braket.sv1` catalogue entry is
enabled, using the standard regional S3 result bucket.

Export `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, and, for temporary
credentials, `AWS_SESSION_TOKEN` before running the command. The Compose overlay
passes them to the controller at runtime; Slurm exports them to the job. Use
short-lived credentials and keep their values out of command arguments, build
arguments, and logs. CI receives the same three secrets and reports an explicit
skip when the access key or secret key is unavailable.

See [Amazon Braket on Slurm](../../docs/slurm.md) for deployment.

For IQM and Braket jobs on the same cluster, use the
[MQT Core multi-vendor example](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm_cluster.html#multiple-device-implementations).
MQT Core and all device implementations share one Python environment. Each job
opens its selected device through the driver, with the credentials that device
needs. The cluster monitors each device before admitting its jobs.
`setup.sh [OUTPUT]` writes the catalogue to the chosen path.
