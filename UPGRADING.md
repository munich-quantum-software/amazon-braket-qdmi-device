# Upgrade guide

## Unreleased

QDMI shots and histogram keys now preserve Braket's measurement-array order. For
example, `[1, 0, 0]` now becomes `"100"`, instead of `"001"`. Remove any
client-side reversal used to obtain measurement order.

The Qiskit and PennyLane adapters require the matching result-order update in
[MQT Core](https://github.com/munich-quantum-toolkit/core/pull/2619). Released
MQT Core 4.0.0 expects the previous order; do not combine it with this device
change.
