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

// Reduced Braket capability snapshots retrieved on 2026-11-05. Actual site
// labels, couplers, native operations, and OpenQASM flags are retained.
// Calibration, service metadata, and redundant all-to-all graphs are omitted.

// emerald.json SHA-256:
// ec97149b974d5d1e276602d6d75ff1f4c5e3298b5ffa5c9897417ef4decd00df
constexpr std::string_view IQM_EMERALD_SNAPSHOT = R"({
  "paradigm": {
    "qubitCount": 54,
    "nativeGateSet": ["cz", "prx", "cc_prx", "measure_ff", "barrier"],
    "connectivity": {
      "connectivityGraph": {
        "1": ["2", "5"],
        "10": ["4", "9", "11", "18"],
        "11": ["5", "10", "12", "19"],
        "12": ["6", "11", "13", "20"],
        "13": ["7", "12", "14", "21"],
        "14": ["13", "22"],
        "15": ["16", "23"],
        "16": ["8", "15", "17", "24"],
        "17": ["9", "16", "18", "25"],
        "18": ["10", "17", "19", "26"],
        "19": ["11", "18", "20", "27"],
        "2": ["1", "6"],
        "20": ["12", "19", "21", "28"],
        "21": ["13", "20", "22", "29"],
        "22": ["14", "21", "30"],
        "23": ["15", "24"],
        "24": ["16", "23", "25"],
        "25": ["17", "24", "26", "33"],
        "26": ["18", "25", "27", "34"],
        "27": ["19", "26", "28", "35"],
        "28": ["20", "27", "29", "36"],
        "29": ["21", "28", "30", "37"],
        "3": ["4", "9"],
        "30": ["22", "29", "31", "38"],
        "31": ["30", "39"],
        "32": ["33", "40"],
        "33": ["25", "32", "34", "41"],
        "34": ["26", "33", "35", "42"],
        "35": ["27", "34", "36", "43"],
        "36": ["28", "35", "37", "44"],
        "37": ["29", "36", "38", "45"],
        "38": ["30", "37", "39", "46"],
        "39": ["31", "38"],
        "4": ["3", "5", "10"],
        "40": ["32", "41"],
        "41": ["33", "40", "42", "47"],
        "42": ["34", "41", "43", "48"],
        "43": ["35", "42", "44", "49"],
        "44": ["36", "43", "45", "50"],
        "45": ["37", "44", "46", "51"],
        "46": ["38", "45"],
        "47": ["41", "48"],
        "48": ["42", "47", "49", "52"],
        "49": ["43", "48", "53"],
        "5": ["1", "4", "6", "11"],
        "50": ["44", "51"],
        "51": ["45", "50"],
        "52": ["48", "53"],
        "53": ["49", "52", "54"],
        "54": ["53"],
        "6": ["2", "5", "7", "12"],
        "7": ["6", "13"],
        "8": ["9", "16"],
        "9": ["3", "8", "10", "17"]
      },
      "fullyConnected": false
    }
  },
  "action": {
    "braket.ir.openqasm.program": {
      "supportedOperations": ["ccnot", "cnot", "cphaseshift", "cphaseshift00", "cphaseshift01", "cphaseshift10", "cswap", "swap", "iswap", "pswap", "ecr", "cy", "cz", "xy", "xx", "yy", "zz", "h", "i", "phaseshift", "rx", "ry", "rz", "s", "si", "t", "ti", "v", "vi", "x", "y", "z", "prx", "cc_prx", "measure_ff", "barrier", "if"],
      "supportedPragmas": ["verbatim", "braket_result_type_sample", "braket_result_type_expectation", "braket_result_type_variance", "braket_result_type_probability"],
      "supportPhysicalQubits": true,
      "requiresAllQubitsMeasurement": false,
      "requiresContiguousQubitIndices": false
    }
  }
})";

// forte-1.json SHA-256:
// 65bdac39c9030c560b8e9f26b44d75a9cf971c2aced47e3265a8e5ae4e591f8d
constexpr std::string_view IONQ_FORTE_1_SNAPSHOT = R"({
  "paradigm": {
    "qubitCount": 36,
    "nativeGateSet": ["GPI", "GPI2", "ZZ"],
    "connectivity": {
      "fullyConnected": true
    }
  },
  "action": {
    "braket.ir.openqasm.program": {
      "supportedOperations": ["x", "y", "z", "h", "s", "si", "t", "ti", "v", "vi", "rx", "ry", "rz", "cnot", "swap", "xx", "yy", "zz"],
      "supportedPragmas": ["braket_result_type_sample", "braket_result_type_expectation", "braket_result_type_variance", "braket_result_type_probability", "verbatim"],
      "supportPhysicalQubits": true,
      "requiresAllQubitsMeasurement": false,
      "requiresContiguousQubitIndices": false
    }
  }
})";

// cepheus-1-108q.json SHA-256:
// 54efc680aad505af31a6f0f20cacffb47ad5e42cc3356adb8f700cf1fd5fe9e6
constexpr std::string_view RIGETTI_CEPHEUS_SNAPSHOT = R"({
  "paradigm": {
    "qubitCount": 107,
    "nativeGateSet": ["rx", "rz", "cz", "barrier"],
    "connectivity": {
      "connectivityGraph": {
        "0": ["1", "9"],
        "1": ["0", "2", "10"],
        "10": ["1", "9", "11", "19"],
        "100": ["91", "99", "101"],
        "101": ["92", "100", "102"],
        "102": ["93", "101", "103"],
        "103": ["94", "102", "104"],
        "104": ["95", "103", "105"],
        "105": ["96", "104", "106"],
        "106": ["97", "105", "107"],
        "107": ["98", "106"],
        "11": ["2", "10", "12", "20"],
        "12": ["3", "11", "13", "21"],
        "13": ["4", "12", "14", "22"],
        "14": ["5", "13", "15", "23"],
        "15": ["6", "14", "16", "24"],
        "16": ["7", "15", "17", "25"],
        "17": ["16", "26"],
        "18": ["9", "19", "27"],
        "19": ["10", "18", "20", "28"],
        "2": ["1", "3", "11"],
        "20": ["11", "19", "21", "29"],
        "21": ["12", "20", "22", "30"],
        "22": ["13", "21", "23", "31"],
        "23": ["14", "22", "24", "32"],
        "24": ["15", "23", "25", "33"],
        "25": ["16", "24", "26", "34"],
        "26": ["17", "25", "35"],
        "27": ["18", "28", "36"],
        "28": ["19", "27", "29", "37"],
        "29": ["20", "28", "30", "38"],
        "3": ["2", "4", "12"],
        "30": ["21", "29", "31", "39"],
        "31": ["22", "30", "32", "40"],
        "32": ["23", "31", "33", "41"],
        "33": ["24", "32", "34", "42"],
        "34": ["25", "33", "35", "43"],
        "35": ["26", "34", "44"],
        "36": ["27", "37", "45"],
        "37": ["28", "36", "38", "46"],
        "38": ["29", "37", "39", "47"],
        "39": ["30", "38", "40", "48"],
        "4": ["3", "5", "13"],
        "40": ["31", "39", "41", "49"],
        "41": ["32", "40", "42", "50"],
        "42": ["33", "41", "43", "51"],
        "43": ["34", "42", "44", "52"],
        "44": ["35", "43", "53"],
        "45": ["36", "46", "54"],
        "46": ["37", "45", "47", "55"],
        "47": ["38", "46", "48", "56"],
        "48": ["39", "47", "49", "57"],
        "49": ["40", "48", "50", "58"],
        "5": ["4", "6", "14"],
        "50": ["41", "49", "51", "59"],
        "51": ["42", "50", "52", "60"],
        "52": ["43", "51", "53", "61"],
        "53": ["44", "52", "62"],
        "54": ["45", "55", "63"],
        "55": ["46", "54", "56", "64"],
        "56": ["47", "55", "57", "65"],
        "57": ["48", "56", "58", "66"],
        "58": ["49", "57", "59", "67"],
        "59": ["50", "58", "60", "68"],
        "6": ["5", "7", "15"],
        "60": ["51", "59", "61", "69"],
        "61": ["52", "60", "62", "70"],
        "62": ["53", "61", "71"],
        "63": ["54", "64", "72"],
        "64": ["55", "63", "65", "73"],
        "65": ["56", "64", "66", "74"],
        "66": ["57", "65", "67", "75"],
        "67": ["58", "66", "68", "76"],
        "68": ["59", "67", "69", "77"],
        "69": ["60", "68", "70", "78"],
        "7": ["6", "16"],
        "70": ["61", "69", "71", "79"],
        "71": ["62", "70", "80"],
        "72": ["63", "73", "81"],
        "73": ["64", "72", "74", "82"],
        "74": ["65", "73", "75", "83"],
        "75": ["66", "74", "76", "84"],
        "76": ["67", "75", "77", "85"],
        "77": ["68", "76", "78", "86"],
        "78": ["69", "77", "79", "87"],
        "79": ["70", "78", "80", "88"],
        "80": ["71", "79", "89"],
        "81": ["72", "82", "90"],
        "82": ["73", "81", "83", "91"],
        "83": ["74", "82", "84", "92"],
        "84": ["75", "83", "85", "93"],
        "85": ["76", "84", "86", "94"],
        "86": ["77", "85", "87", "95"],
        "87": ["78", "86", "88", "96"],
        "88": ["79", "87", "89", "97"],
        "89": ["80", "88", "98"],
        "9": ["0", "10", "18"],
        "90": ["81", "91", "99"],
        "91": ["82", "90", "92", "100"],
        "92": ["83", "91", "93", "101"],
        "93": ["84", "92", "94", "102"],
        "94": ["85", "93", "95", "103"],
        "95": ["86", "94", "96", "104"],
        "96": ["87", "95", "97", "105"],
        "97": ["88", "96", "98", "106"],
        "98": ["89", "97", "107"],
        "99": ["90", "100"]
      },
      "fullyConnected": false
    }
  },
  "action": {
    "braket.ir.openqasm.program": {
      "supportedOperations": ["cz", "xy", "ccnot", "cnot", "cphaseshift", "cphaseshift00", "cphaseshift01", "cphaseshift10", "cswap", "h", "i", "iswap", "phaseshift", "pswap", "rx", "ry", "rz", "s", "si", "swap", "t", "ti", "x", "y", "z", "barrier"],
      "supportedPragmas": ["verbatim", "braket_result_type_sample", "braket_result_type_expectation", "braket_result_type_variance", "braket_result_type_probability"],
      "supportPhysicalQubits": true,
      "requiresAllQubitsMeasurement": false,
      "requiresContiguousQubitIndices": false
    }
  }
})";

} // namespace amazon::braket::qdmi::test
