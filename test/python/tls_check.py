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

"""Check native service, credential, and CRT trust against local HTTPS servers."""

from __future__ import annotations

import argparse
import contextlib
import http.server
import json
import os
import pathlib
import ssl
import subprocess
import tempfile
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    """Serve synthetic credentials without recording requests."""

    def handle(self) -> None:
        """Allow TLS-only probes to close without sending or reading HTTP data."""
        with contextlib.suppress(BrokenPipeError, ConnectionResetError):
            super().handle()

    def do_GET(self) -> None:
        """Return dummy credentials; these never leave the loopback interface."""
        data = json.dumps({
            "AccessKeyId": "test",
            "SecretAccessKey": "test",
            "Token": "test",
            "Expiration": "2099-01-01T00:00:00Z",
        }).encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, format: str, *args: object) -> None:  # ruff: ignore[builtin-argument-shadowing] - Base class signature.
        """Keep the test output free of request data."""


class CountingHandler(Handler):
    """Count plain HTTP requests to detect transmission after an option error."""

    requests = 0

    def do_GET(self) -> None:
        """Count requests before serving the same synthetic response."""
        type(self).requests += 1
        super().do_GET()


def main() -> None:
    """Generate two roots and check trust isolation in fresh native processes.

    Raises:
        AssertionError: If a failed curl option allows an HTTP request through.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("--openssl", default="openssl")
    parser.add_argument("--observe", action="store_true")
    args = parser.parse_args()
    executable = args.executable.resolve()
    with tempfile.TemporaryDirectory() as directory, contextlib.ExitStack() as stack:
        root = pathlib.Path(directory)
        servers = []
        for name in ("a", "b"):
            certificate = root / f"{name}.pem"
            key = root / f"{name}.key"
            subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true] - Caller-selected OpenSSL test tool, no shell.
                [
                    args.openssl,
                    "req",
                    "-x509",
                    "-newkey",
                    "rsa:2048",
                    "-nodes",
                    "-keyout",
                    str(key),
                    "-out",
                    str(certificate),
                    "-days",
                    "1",
                    "-subj",
                    f"/CN=TLS test {name}",
                    "-addext",
                    "subjectAltName=DNS:localhost",
                ],
                check=True,
                capture_output=True,
            )
            server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            server.daemon_threads = True
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(certificate, key)
            server.socket = context.wrap_socket(server.socket, server_side=True)
            stack.callback(server.server_close)
            worker = threading.Thread(target=server.serve_forever, daemon=True)
            worker.start()
            stack.callback(worker.join)
            stack.callback(server.shutdown)
            servers.append(server)

        # A stale hashed CA directory must not restore the rejected second root.
        certificates = root / "certificates"
        certificates.mkdir()
        (certificates / "b.pem").write_bytes((root / "b.pem").read_bytes())
        certificate_hash = subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true] - Caller-selected OpenSSL test tool, no shell.
            [args.openssl, "x509", "-in", str(root / "b.pem"), "-noout", "-hash"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
        (certificates / f"{certificate_hash}.0").write_bytes((root / "b.pem").read_bytes())
        environment = {
            key: value for key, value in os.environ.items() if not key.startswith("AWS_") and "PROXY" not in key.upper()
        }
        environment.update({
            "AWS_EC2_METADATA_DISABLED": "true",
            "AWS_CONFIG_FILE": str(root / "empty-config"),
            "AWS_SHARED_CREDENTIALS_FILE": str(root / "empty-credentials"),
            "AWS_DEFAULT_REGION": "us-east-1",
            "SSL_CERT_DIR": str(certificates),
            "SSL_CERT_FILE": str(root / "b.pem"),
            "AWS_CA_BUNDLE": str(root / "a.pem"),
        })
        for server, accepted in zip(servers, (True, False), strict=True):
            environment["AMAZON_BRAKET_QDMI_TEST_TLS_PORT"] = str(server.server_port)
            environment["AMAZON_BRAKET_QDMI_TEST_TLS_ACCEPT"] = "observe" if args.observe else "1" if accepted else "0"
            subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true] - Caller-selected native test executable, no shell.
                [str(executable), "--gtest_filter=AwsTrustInitializationTest.LocalTlsServiceCredentialsAndCrt"],
                check=True,
                env=environment,
                timeout=60,
            )
        plain = http.server.ThreadingHTTPServer(("127.0.0.1", 0), CountingHandler)
        plain.daemon_threads = True
        stack.callback(plain.server_close)
        worker = threading.Thread(target=plain.serve_forever, daemon=True)
        worker.start()
        stack.callback(worker.join)
        stack.callback(plain.shutdown)
        environment["AMAZON_BRAKET_QDMI_TEST_HTTP_PORT"] = str(plain.server_port)
        subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true] - Caller-selected native test executable, no shell.
            [str(executable), "--gtest_filter=AwsTrustInitializationTest.CurlOptionFailurePreventsTransmission"],
            check=True,
            env=environment,
            timeout=15,
        )
        if CountingHandler.requests != 1:
            message = f"Expected only the control HTTP request, received {CountingHandler.requests}"
            raise AssertionError(message)


if __name__ == "__main__":
    main()
