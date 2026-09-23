# Amazon Braket workload for the shared Core Slurm fixture

MQT Core owns the Docker image, Slurm services, configuration, SPANK build,
transport assertions, and cleanup. This directory supplies the Braket mock,
credential-process fixture, catalogue selection, and short Qiskit and PennyLane
circuits. All AWS requests stay inside the fixture network.

Use a Core checkout containing the shared fixture and a directory containing its
released Linux `mqt_core-4.0.0` wheel for the Docker host architecture. The
fixture requires privileged Docker with cgroup v2 and Slurm 25.11 or newer.

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
and submits and retrieves eight Bell-state shots through each SDK. Core runs the
same workload as a non-root user with explicit environment setup and with shared
SPANK injection, then runs its generic transport checks.

The mock accepts only dummy AWS credentials returned by the local credential
process. Do not pass real AWS credentials or endpoints to this fixture.

See [the provider guide](../../docs/slurm.md) for deployment and migration.
