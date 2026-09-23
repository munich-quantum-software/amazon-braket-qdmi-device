# Upgrading

## Unreleased

### Shared Slurm integration

MQT Core's shared SPANK component replaces the provider plugin and requires
Slurm 25.11 or newer. Deploy and validate it from a released Core version before
replacing the old plugstack directive. Remove `amazon-braket-qdmi-spank.so`; do
not load both plugins. The `BUILD_AMAZON_BRAKET_SPANK_PLUGIN` option and
`amazon-braket-qdmi-spank-plugin` install component have been removed.

Update job options as follows:

| Old option                                     | Replacement                                      |
| ---------------------------------------------- | ------------------------------------------------ |
| `--amazon-braket-qdmi-config-file=PATH`        | `--qdmi-config-file=PATH`                        |
| `--amazon-braket-profile=NAME`                 | `--qdmi-ref-AWS_PROFILE=NAME`                    |
| `--amazon-braket-config-file=PATH`             | `--qdmi-ref-AWS_CONFIG_FILE=PATH`                |
| `--amazon-braket-shared-credentials-file=PATH` | `--qdmi-ref-AWS_SHARED_CREDENTIALS_FILE=PATH`    |
| `--amazon-braket-task-results-s3-uri=URI`      | `--qdmi-ref-AMZN_BRAKET_TASK_RESULTS_S3_URI=URI` |
| `--amazon-braket-reservation-arn=ARN`          | `--qdmi-ref-AMAZON_BRAKET_RESERVATION_ARN=ARN`   |

Translate old plugstack defaults into `qdmi_config_file` and corresponding
`reference` entries, and list the same concrete IDs under `licenses`. Preserve
the existing catalogue and AWS references during migration. Shared injection
does not add early device validation: application opening continues to
authenticate and check status. The optional generic Core launch checker is a
separate deployment choice.

See {doc}`slurm` for setup and validation.
