# Dependencies

## Runtime dependencies

The native device implements the QDMI C interface using the AWS SDK for C++
clients for Braket, S3, and STS. A QDMI client can load the library without
Python or MQT Core. AWS credentials and connection settings are described in
{doc}`configuration`.

## Optional Python integrations

The Python package provides the native library and device catalogue. Its
`qiskit` and `pennylane` extras install MQT Core with the corresponding
framework integration. MQT Core supplies compilation, submission orchestration,
and result handling; the provider adapters configure the Amazon Braket
connection.

```console
uv pip install "amazon-braket-qdmi[qiskit,pennylane]"
```

Select just one extra when only that framework is needed. See {doc}`qiskit` and
{doc}`pennylane` for usage. Dependency versions are declared in
[`pyproject.toml`](https://github.com/munich-quantum-software/amazon-braket-qdmi-device/blob/main/pyproject.toml).

## Build dependencies

A source build needs a C++20 compiler and CMake 3.24 or later. CMake obtains
QDMI and the AWS SDK for C++ when suitable installed packages are unavailable.
Initial dependency downloads require network access. The AWS SDK uses platform
HTTP and TLS libraries; Linux builds need libcurl and OpenSSL development
packages. Native tests use GoogleTest.

Building the Python package also requires Python 3.11 or later and the build
backend declared in `pyproject.toml`. Documentation uses Sphinx and Doxygen. See
{doc}`installation` for build commands and {doc}`development` for tests and
documentation builds.
