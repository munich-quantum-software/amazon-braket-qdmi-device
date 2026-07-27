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

# /// script
# requires-python = ">=3.12,<3.13"
# dependencies = []
# ///

"""Minimal local Amazon Braket API for the real-Slurm SPANK tests."""

from __future__ import annotations

import argparse
import contextlib
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import ClassVar
from urllib.parse import unquote

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
    """Serve the GetDevice response used during SPANK validation."""

    request_count: ClassVar[int] = 0

    def do_GET(self) -> None:
        """Return fixture health, counters, or a deterministic device."""
        if self.path == "/health":
            self._send_json({"status": "ok"})
            return
        if self.path == "/request-count":
            self._send_json({"count": self.request_count})
            return

        decoded_path = unquote(self.path)
        if not decoded_path.startswith("/device/"):
            self.send_error(404)
            return

        type(self).request_count += 1
        status = "OFFLINE" if "offline-device" in decoded_path else "ONLINE"
        self._send_json({
            "deviceArn": decoded_path.removeprefix("/device/"),
            "deviceName": "Local SV1",
            "providerName": "Amazon Braket test fixture",
            "deviceType": "SIMULATOR",
            "deviceStatus": status,
            "deviceCapabilities": DEVICE_CAPABILITIES,
            "deviceQueueInfo": [],
        })

    def log_message(
        self,
        format: str,  # ruff:ignore[builtin-argument-shadowing]
        *args: object,
    ) -> None:
        """Suppress routine request logging in the test container."""

    def _send_json(self, payload: object) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(200)
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
