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

"""Shared collection gate for PennyLane tests."""

from __future__ import annotations

import sys

PENNYLANE_MINIMUM_PYTHON = (3, 11)
PENNYLANE_PYTHON_SUPPORTED = sys.version_info >= PENNYLANE_MINIMUM_PYTHON
PENNYLANE_SKIP_REASON = f"requires Python >= {PENNYLANE_MINIMUM_PYTHON[0]}.{PENNYLANE_MINIMUM_PYTHON[1]}"
