# Amazon Braket Slurm smoke test

This test runs Qiskit and PennyLane workloads against local Braket/S3 HTTP
responses in
[MQT Core's Docker Slurm cluster](https://github.com/munich-quantum-toolkit/core/tree/main/docker/slurm).
All AWS requests stay inside the fixture network.

Use the MQT Core revision pinned in the Slurm workflow and build its Linux wheel
with `uv build --wheel --out-dir "$CORE_DIST"` from that checkout. The wheel
must match the Docker host architecture. The fixture requires rootful Docker on
a disposable Linux cgroup-v2 host and Slurm 25.11 or newer.

```sh
PROVIDER_INSTALL_MODE=native uv run --no-project \
  "$CORE_SOURCE/test/slurm/run_integration.py" \
  --workload . --dist "$CORE_DIST" \
  --setup-script test/slurm/setup.sh \
  --compose-file test/slurm/compose.yml \
  --device-license amazon.braket.sv1 \
  --qdmi-config-file /opt/provider-catalogue.json \
  --reference AWS_EC2_METADATA_DISABLED=true \
  --reference AWS_ENDPOINT_URL_BRAKET=http://braket:18080 \
  --reference AWS_ENDPOINT_URL_S3=http://braket:18080 \
  --reference AWS_PROFILE=spank-test \
  --reference AWS_CONFIG_FILE=/workload/test/slurm/aws_config \
  --reference AMZN_BRAKET_TASK_RESULTS_S3_URI=s3://fixture-bucket/tasks \
  -- python3 /workload/test/slurm/probe.py
```

Repeat with `PROVIDER_INSTALL_MODE=wheel`. Both modes install the Python
adapters. Native mode selects the separately installed Runtime catalogue; wheel
mode selects the bundled library. The workload checks the loaded library path
and submits and retrieves eight Bell-state shots through each SDK. The workload
runs as an unprivileged user, once with explicit job configuration and once with
site defaults.

The mock accepts only dummy AWS credentials returned by the local credential
process. Do not pass real AWS credentials or endpoints to this fixture.

See [Amazon Braket on Slurm](../../docs/slurm.md) for deployment.
