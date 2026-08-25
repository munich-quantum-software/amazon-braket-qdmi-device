# Optional Amazon Braket SPANK plugin

[![SPANK Plugin GPLv3 License](https://img.shields.io/static/v1?logo=gnu&label=License&message=GPLv3&color=informational&style=flat-square)](https://www.gnu.org/licenses/gpl-3.0.en.html)

This optional plugin injects the installed QDMI catalogue path and AWS
configuration references into concrete Amazon Braket Slurm jobs. It neither
distributes credentials nor contacts AWS inside `slurmstepd`.

The authoritative build, installation, configuration, job, and validation guide
is the [Slurm and SPANK documentation]. Keeping that workflow in one place
prevents the source-tree instructions from diverging from the published docs.

[Slurm and SPANK documentation]: https://amazon-braket-qdmi-device.readthedocs.io/en/latest/slurm.html
