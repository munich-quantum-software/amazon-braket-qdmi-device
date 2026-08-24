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

"""Qiskit integration for the Amazon Braket QDMI device."""

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar

try:
    from mqt.core.plugins.qiskit.backend import QDMIBackend
    from mqt.core.plugins.qiskit.exceptions import UnsupportedFormatError
    from mqt.core.qdmi import Device as QDMIDeviceHandle
    from mqt.core.qdmi import ProgramFormat
    from mqt.core.qdmi.driver import open_device
except ImportError as error:
    msg = "Install 'amazon-braket-qdmi[qiskit]' to use the Qiskit backend."
    raise ImportError(msg) from error

try:
    from qiskit import qasm3
    from qiskit.circuit import Gate, Instruction, Parameter, QuantumCircuit
    from qiskit.circuit.library import get_standard_gate_name_mapping
except ImportError as error:
    msg = "Install 'amazon-braket-qdmi[qiskit]' to use the Qiskit backend."
    raise ImportError(msg) from error

from . import AMAZON_BRAKET_QDMI_DEVICE_ID
from ._catalogue import register_device

if TYPE_CHECKING:
    from collections.abc import Iterable, Mapping

__all__ = ["AmazonBraketBackend"]


def __dir__() -> list[str]:
    return __all__


def _opaque_gate(name: str, num_qubits: int, num_parameters: int) -> Gate:
    """Create a Braket OpenQASM gate for Qiskit's target model.

    Returns:
        An opaque Qiskit gate with the requested signature.
    """
    return Gate(name, num_qubits, [Parameter(f"p{index}") for index in range(num_parameters)])


def _build_gate_mappings(
    aliases: dict[str, set[str]],
    extra_gates: Mapping[str, Instruction | type[Instruction]],
) -> tuple[dict[str, Instruction | type[Instruction]], dict[str, set[str]]]:
    """Build subclass-aware mappings for MQT Core 3.9 and newer.

    Returns:
        The operation-to-instruction and instruction-to-operation mappings.
    """
    operation_to_gate = dict(QDMIBackend._OPERATION_TO_GATE_MAP)  # ruff: ignore[private-member-access]
    qiskit_to_operation = {
        name: set(operation_names)
        for name, operation_names in QDMIBackend._QISKIT_TO_QDMI_GATE_MAP.items()  # ruff: ignore[private-member-access]
    }
    standard_gates = get_standard_gate_name_mapping()
    for canonical_name, alias_names in aliases.items():
        names = {canonical_name, *alias_names}
        gate = standard_gates.get(canonical_name)
        if gate is None:
            gate = operation_to_gate.get(canonical_name)
        if gate is None:
            continue
        for name in names:
            operation_to_gate[name] = gate
            qiskit_to_operation[name] = names
    for name, gate in extra_gates.items():
        operation_to_gate[name] = gate
        qiskit_to_operation[name] = {name}
    return operation_to_gate, qiskit_to_operation


def _serialize_to_braket_qasm3(
    circuit: QuantumCircuit,
    operation_names: Iterable[str],
    aliases: Mapping[str, set[str]],
) -> str:
    """Serialize a Qiskit circuit without unsupported OpenQASM includes.

    Returns:
        A self-contained OpenQASM 3 program.
    """
    available = set(operation_names)
    translated = circuit.copy()
    basis_gates = set()
    for index, instruction in enumerate(translated.data):
        name = instruction.operation.name.lower()
        supported_names = aliases.get(name, {name}) & available
        translated_name = name if name in available else min(supported_names, default=name)
        if translated_name not in {"barrier", "measure", "reset"}:
            basis_gates.add(translated_name)
        if translated_name != name:
            translated.data[index] = instruction.replace(operation=instruction.operation.copy(name=translated_name))
    return qasm3.dumps(translated, includes=(), basis_gates=sorted(basis_gates))


class AmazonBraketBackend(QDMIBackend):
    """Qiskit backend for an Amazon Braket QDMI catalogue device.

    Args:
        device_id: Stable ID from the packaged Amazon Braket device catalogue.
        device: An already-open Amazon Braket QDMI device, such as a device
            selected from a Slurm license.
        device_arn: Optional ARN override. The generic device requires an ARN.
        region: Optional AWS Region override.
        reservation_arn: Optional Amazon Braket reservation ARN.
    """

    _GATE_ALIASES: ClassVar[dict[str, set[str]]] = {
        **QDMIBackend._GATE_ALIASES,  # ruff: ignore[private-member-access]
        "ccx": {"ccnot"},
        "cp": {"cphaseshift"},
        "p": QDMIBackend._GATE_ALIASES["p"] | {"phaseshift"},  # ruff: ignore[private-member-access]
        "rxx": {"xx"},
        "ryy": {"yy"},
        "rzz": {"zz"},
        "sdg": {"si"},
        "sx": {"v"},
        "sxdg": {"vi"},
        "tdg": {"ti"},
    }
    _EXTRA_GATES: ClassVar[dict[str, Instruction | type[Instruction]]] = {
        "cphaseshift00": _opaque_gate("cphaseshift00", 2, 1),
        "cphaseshift01": _opaque_gate("cphaseshift01", 2, 1),
        "cphaseshift10": _opaque_gate("cphaseshift10", 2, 1),
        "gpi": _opaque_gate("gpi", 1, 1),
        "gpi2": _opaque_gate("gpi2", 1, 1),
        "ms": _opaque_gate("ms", 2, 3),
        "pswap": _opaque_gate("pswap", 2, 1),
        "xy": _opaque_gate("xy", 2, 1),
    }
    _OPERATION_TO_GATE_MAP: ClassVar[dict[str, Instruction | type[Instruction]]]
    _QISKIT_TO_QDMI_GATE_MAP: ClassVar[dict[str, set[str]]]
    _OPERATION_TO_GATE_MAP, _QISKIT_TO_QDMI_GATE_MAP = _build_gate_mappings(
        _GATE_ALIASES,
        _EXTRA_GATES,
    )

    @classmethod
    def _map_operation_to_gate(cls, op_name: str) -> Instruction | type[Instruction] | None:
        """Map an Amazon Braket operation to its Qiskit instruction.

        Returns:
            The mapped Qiskit instruction, if one exists.
        """
        return cls._OPERATION_TO_GATE_MAP.get(op_name.lower())

    @classmethod
    def _map_qiskit_gate_to_operation_names(cls, qiskit_gate_name: str) -> set[str]:
        """Return the Amazon Braket names for a Qiskit instruction."""
        return cls._QISKIT_TO_QDMI_GATE_MAP.get(qiskit_gate_name.lower(), {qiskit_gate_name.lower()})

    def _braket_program(
        self,
        circuit: QuantumCircuit,
        supported_program_formats: Iterable[ProgramFormat],
    ) -> tuple[str, ProgramFormat]:
        """Serialize one circuit to the OpenQASM dialect accepted by Amazon Braket.

        Returns:
            The self-contained program and its OpenQASM 3 format.

        Raises:
            UnsupportedFormatError: If the QDMI device does not advertise OpenQASM 3.
        """
        if ProgramFormat.QASM3 not in supported_program_formats:
            msg = "The Amazon Braket QDMI device does not advertise OpenQASM 3."
            raise UnsupportedFormatError(msg)
        return (
            _serialize_to_braket_qasm3(
                circuit,
                self._braket_operation_names,
                self._QISKIT_TO_QDMI_GATE_MAP,
            ),
            ProgramFormat.QASM3,
        )

    def _convert_circuit(
        self,
        circuit: QuantumCircuit,
        supported_program_formats: Iterable[ProgramFormat],
    ) -> tuple[str, ProgramFormat]:
        """Serialize a circuit with the MQT Core 3.9 adapter interface.

        Returns:
            The Amazon Braket program and its format.
        """
        return self._braket_program(circuit, supported_program_formats)

    def _serialize_circuit(
        self,
        circuit: QuantumCircuit,
        supported_program_formats: Iterable[ProgramFormat],
    ) -> tuple[str, ProgramFormat]:
        """Serialize a circuit with the current MQT Core adapter interface.

        Returns:
            The Amazon Braket program and its format.
        """
        return self._braket_program(circuit, supported_program_formats)

    def __init__(
        self,
        device_id: str | None = None,
        *,
        device: QDMIDeviceHandle | None = None,
        device_arn: str | None = None,
        region: str | None = None,
        reservation_arn: str | None = None,
    ) -> None:
        """Open the selected Amazon Braket device and adapt it for Qiskit.

        Raises:
            ValueError: If the generic device has no ARN.
        """
        if device is not None:
            if device_id is not None or any((device_arn, region, reservation_arn)):
                msg = "An already-open device cannot be combined with a device ID or session overrides."
                raise ValueError(msg)
            self._braket_operation_names = frozenset(operation.name().lower() for operation in device.operations())
            super().__init__(device=device)
            return

        resolved_device_id = device_id or AMAZON_BRAKET_QDMI_DEVICE_ID
        if resolved_device_id == AMAZON_BRAKET_QDMI_DEVICE_ID and not device_arn:
            msg = "The generic Amazon Braket device requires a non-empty device_arn."
            raise ValueError(msg)

        register_device(resolved_device_id)
        device = open_device(
            resolved_device_id,
            base_url=device_arn,
            custom2=region,
            custom3=reservation_arn,
        )
        self._braket_operation_names = frozenset(operation.name().lower() for operation in device.operations())
        super().__init__(device=device, device_id=resolved_device_id)
