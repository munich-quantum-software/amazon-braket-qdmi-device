# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <https://www.gnu.org/licenses/>.

"""Local Braket and S3 endpoints for license-selected adapter execution."""

from __future__ import annotations

import argparse
import contextlib
import json
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
from typing import ClassVar
from urllib.parse import unquote, urlsplit

ACCESS_KEY = "temporary-access-key"
SESSION_TOKEN = "temporary-session-token"  # ruff: ignore[hardcoded-password-string]
DEVICE_CAPABILITIES = json.dumps(
    {
        "paradigm": {"qubitCount": 2},
        "action": {
            "braket.ir.openqasm.program": {
                "supportedOperations": ["h", "cnot"],
            }
        },
    },
    separators=(",", ":"),
)


class BraketRequestHandler(BaseHTTPRequestHandler):
    """Serve deterministic device, task, and measurement responses."""

    tasks: ClassVar[dict[str, dict[str, object]]] = {}
    lock: ClassVar[Lock] = Lock()

    def do_GET(self) -> None:
        """Return fixture health, device, task, or measurement data."""
        path = unquote(urlsplit(self.path).path)
        if path == "/health":
            self._send_json({"status": "ok"})
            return
        if not self._has_temporary_credentials():
            self._send_json(
                {
                    "__type": "AccessDeniedException",
                    "message": "temporary credentials required",
                },
                HTTPStatus.FORBIDDEN,
            )
            return
        if path.startswith("/quantum-task/"):
            task_id = path.rsplit("/", maxsplit=1)[-1]
            with self.lock:
                task = self.tasks[task_id]
            self._send_json({
                "quantumTaskArn": f"arn:aws:braket:us-east-1:123456789012:quantum-task/{task_id}",
                "deviceArn": task["deviceArn"],
                "status": "COMPLETED",
                "shots": task["shots"],
                "outputS3Bucket": "fixture-bucket",
                "outputS3Directory": task_id,
            })
            return
        if path.endswith("/results.json"):
            task_id = path.split("/")[-2]
            with self.lock:
                shots = self.tasks[task_id]["shots"]
            assert isinstance(shots, int)
            self._send_json({"measurements": [[index % 2, index % 2] for index in range(shots)]})
            return
        if not path.startswith("/device/"):
            self.send_error(HTTPStatus.NOT_FOUND)
            return

        retired = path.endswith("/dm1")
        self._send_json({
            "deviceArn": path.removeprefix("/device/"),
            "deviceName": "Local DM1" if retired else "Local SV1",
            "providerName": "Amazon Braket test fixture",
            "deviceType": "SIMULATOR",
            "deviceStatus": "RETIRED" if retired else "ONLINE",
            "deviceCapabilities": DEVICE_CAPABILITIES,
            "deviceQueueInfo": [],
        })

    def do_POST(self) -> None:
        """Accept a short OpenQASM task without contacting AWS."""
        if not self._has_temporary_credentials():
            self._send_json({"__type": "AccessDeniedException"}, HTTPStatus.FORBIDDEN)
            return
        if self.path != "/quantum-task":
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        payload = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        action = json.loads(payload["action"])
        assert action["braketSchemaHeader"]["name"] == "braket.ir.openqasm.program"
        assert "OPENQASM" in action["source"]
        assert "measure" in action["source"]
        statements = {"".join(statement.split()) for statement in action["source"].split(";")}
        assert "hq[0]" in statements
        assert "cnotq[0],q[1]" in statements
        assert payload["deviceArn"].endswith("/amazon/sv1")
        assert payload["outputS3Bucket"] == "fixture-bucket"
        with self.lock:
            task_id = str(len(self.tasks) + 1)
            self.tasks[task_id] = payload
        self._send_json({"quantumTaskArn": f"arn:aws:braket:us-east-1:123456789012:quantum-task/{task_id}"})

    def log_message(
        self,
        format: str,  # ruff: ignore[builtin-argument-shadowing]
        *args: object,
    ) -> None:
        """Suppress routine request logging in the test container."""

    def _has_temporary_credentials(self) -> bool:
        authorization = self.headers.get("Authorization", "")
        token = self.headers.get("X-Amz-Security-Token", "")
        return f"Credential={ACCESS_KEY}/" in authorization and token == SESSION_TOKEN

    def _send_json(
        self,
        payload: object,
        status: HTTPStatus = HTTPStatus.OK,
    ) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("x-amzn-requestid", "local-spank-test")
        self.end_headers()
        with contextlib.suppress(BrokenPipeError):
            self.wfile.write(body)


def main() -> None:
    """Run the local fixture until the test container exits."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=18080, type=int)
    args = parser.parse_args()
    ThreadingHTTPServer((args.host, args.port), BraketRequestHandler).serve_forever()


if __name__ == "__main__":
    main()
