/*
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <string_view>

namespace amazon::braket::qdmi::test {

// Synthetic, reduced capability fixtures for parser tests. The provider and
// device names identify the Braket schema variants that each document models;
// qubit counts, connectivity graphs, and calibration values are deliberately
// invented and must not be read as specifications or snapshots of live devices.
constexpr std::string_view AQT_IBEX_Q1 = R"({
  "paradigm": {
    "qubitCount": 4,
    "nativeGateSet": ["prx", "xx", "rz"],
    "connectivity": {"fullyConnected": true}
  },
  "action": {"braket.ir.openqasm.program": {
    "supportedOperations": ["prx", "xx", "rz", "h", "cnot", "swap"]
  }}
})";

constexpr std::string_view IONQ_FORTE = R"({
  "paradigm": {
    "qubitCount": 3,
    "nativeGateSet": ["gpi", "gpi2", "zz"],
    "connectivity": {"fullyConnected": true}
  },
  "action": {"braket.ir.openqasm.program": {
    "supportedOperations": ["x", "y", "z", "rx", "ry", "rz", "h", "cnot", "gpi", "gpi2", "zz"]
  }}
})";

constexpr std::string_view IQM_GARNET = R"({
  "paradigm": {
    "qubitCount": 3,
    "nativeGateSet": ["cz", "prx"],
    "connectivity": {
      "fullyConnected": false,
      "connectivityGraph": {"1": ["2"], "2": ["5"], "5": ["1"]}
    }
  },
  "action": {"braket.ir.openqasm.program": {
    "supportedOperations": ["h", "cnot", "cz", "prx", "rx", "rz"]
  }},
  "provider": {"properties": {"one_qubit": {
    "1": {"T1": 0.00004, "T2": 0.00003},
    "2": {"T1": 0.00005, "T2": 0.00004},
    "5": {"T1": 0.00006, "T2": 0.00005}
  }}},
  "standardized": {"twoQubitProperties": {
    "1-2": {"twoQubitGateFidelity": [{
      "gateName": "cz", "fidelity": 0.987,
      "direction": {"control": 1, "target": 2}
    }]}
  }}
})";

constexpr std::string_view RIGETTI_ANKAA_3 = R"({
  "paradigm": {
    "qubitCount": 3,
    "nativeGateSet": ["rx", "rz", "iswap"],
    "connectivity": {
      "fullyConnected": false,
      "connectivityGraph": {"0": ["1"], "1": ["2"], "2": []}
    }
  },
  "action": {"braket.ir.openqasm.program": {
    "supportedOperations": ["rx", "rz", "iswap", "cz", "xy", "h", "cnot"]
  }}
})";

constexpr std::string_view RIGETTI_ANKAA_12 = R"({
  "paradigm": {
    "qubitCount": 12,
    "nativeGateSet": ["rx", "rz", "cz"],
    "connectivity": {
      "fullyConnected": false,
      "connectivityGraph": {
        "0": ["1"], "1": ["2"], "2": ["3"], "3": ["4"],
        "4": ["5"], "5": ["6"], "6": ["7"], "7": ["8"],
        "8": ["9"], "9": ["10"], "10": ["11"], "11": []
      }
    }
  },
  "action": {"braket.ir.openqasm.program": {
    "supportedOperations": ["rx", "rz", "cz"]
  }}
})";

constexpr std::string_view SV1 = R"({
  "paradigm": {"qubitCount": 4},
  "action": {"braket.ir.openqasm.program": {
    "supportedOperations": ["h", "cnot", "rx", "ry", "rz", "unitary", "gphase"]
  }}
})";

constexpr std::string_view DM1 = R"({
  "paradigm": {"qubitCount": 3},
  "action": {"braket.ir.openqasm.program": {
    "supportedOperations": ["x", "cnot", "kraus", "bit_flip", "gphase"]
  }}
})";

} // namespace amazon::braket::qdmi::test
