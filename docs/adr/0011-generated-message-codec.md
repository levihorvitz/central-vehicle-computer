# 0011. Message codec generated from the DBC by our own generator

- **Status:** Accepted
- **Date:** 2026-11-26

## Context

The ICD is `icd/central_vehicle_computer.dbc`. It defines every message, the position of every signal in the payload, its scale and offset, and the names of its values. The central computer decodes the messages it receives and encodes the messages it sends, and the simulated car does the same on the other side. The bit positions and conversions in that code must match the DBC exactly. DBC 0.1.0 has 5 messages; connecting all seven units will add more, and every change to the ICD must reach the code.

## Decision

The message codec is C++ code generated from the DBC by a Python generator in this repository.

- The generator reads the DBC with the cantools library. It does not parse the DBC itself.
- It emits C++17 that follows `docs/coding-guidelines.md`, is compiled with the same warning flags as the rest of the code, and allocates no memory (ADR 0005).
- For each message it emits a struct of the signals, an encode function that fills the 8-byte payload, and a decode function. Decode reports a raw value outside the range in the DBC, or a value with no name in its value table, as invalid instead of converting it. Value tables become enums.
- The `AliveCounter` and `Crc` signals are not part of the structs: the E2E layer writes and checks them (ADR 0007). The generator checks that every message has both, at exactly the positions ADR 0007 defines.
- It supports only what the DBC uses: 11-bit IDs, 8-byte messages, little-endian (Intel) unsigned signals, scale, offset and value tables. Any other feature stops the generator with an error that names the message and the signal.
- The generated files are committed. CI runs the generator again and fails if the result differs from the committed files.

## Alternatives considered

- **Hand-written codec for each message.** The fastest way to the walking skeleton, which needs only a few messages. But the DBC and the code become two copies of the ICD, kept in sync by hand. Every bit position and conversion is a chance for a mistake that tests written by the same person can repeat, and every new message is more manual code.
- **The C generator in cantools (`generate_c_source`).** Produces working code from the DBC, but it is C, with its own naming and style, outside `docs/coding-guidelines.md`. It would need a C++ wrapper and different compiler rules from the rest of the project.
- **Our own DBC parser.** No dependency, but the DBC format has many features this project does not use. The parser would be more work than the generator, for no gain.
- **Generating at build time instead of committing the output.** The code could never be out of date, but every build, including on the Raspberry Pi, would need Python and cantools, and reviewers would not see the generated code in the pull request.

## Consequences

- The DBC is the only source of truth for the message layout. A change to the ICD reaches the code by running the generator, and CI catches a forgotten run.
- The generator is code too, and needs its own tests. The generated codec is tested against frames encoded by cantools, an implementation independent of the bit arithmetic our generator emits.
- cantools becomes a development dependency, pinned to one version. Changing the DBC needs it; building the project does not.
- Using a DBC feature the generator does not support, such as signed signals, means extending the generator first.
- The first codec costs more than hand-written code would, before the walking skeleton is complete.
- How decode reports an invalid signal, and the C++ type of physical values, are decided in the detailed design (#18).
