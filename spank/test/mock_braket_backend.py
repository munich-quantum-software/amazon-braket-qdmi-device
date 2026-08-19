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

"""Minimal signed GetDevice fixture for the real-Slurm connector test."""

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
    """Serve deterministic GetDevice responses."""

    counters: ClassVar[dict[str, int]] = {
        "get_device": 0,
        "signed_requests": 0,
        "signature_failures": 0,
    }
    lock: ClassVar[Lock] = Lock()

    def do_GET(self) -> None:
        """Return fixture health, counters, or one device description."""
        path = unquote(urlsplit(self.path).path)
        if path == "/health":
            self._send_json({"status": "ok"})
            return
        if path == "/state":
            with self.lock:
                payload = dict(self.counters)
            self._send_json(payload)
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
        if not path.startswith("/device/"):
            self.send_error(HTTPStatus.NOT_FOUND)
            return

        self._increment("get_device")
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

    def log_message(
        self,
        format: str,  # ruff: ignore[builtin-argument-shadowing]
        *args: object,
    ) -> None:
        """Suppress routine request logging in the test container."""

    @classmethod
    def _increment(cls, name: str) -> None:
        with cls.lock:
            cls.counters[name] += 1

    def _has_temporary_credentials(self) -> bool:
        authorization = self.headers.get("Authorization", "")
        token = self.headers.get("X-Amz-Security-Token", "")
        valid = f"Credential={ACCESS_KEY}/" in authorization and token == SESSION_TOKEN
        self._increment("signed_requests" if valid else "signature_failures")
        return valid

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
