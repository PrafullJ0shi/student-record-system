# Security

SRMS is a local program. It does not open a socket, and it does not run shell commands.

## What it trusts

The operator chooses every path. The program opens that path and no other. A register or CSV file is data, not code: the parsers accept a fixed grammar, reject overlong fields, and stop at 10,000 records or 2 MiB.

A register that fails its CRC-32, its version check, or its field checks is not loaded. The in-memory list is left as it was.

## Reporting a problem

If you find a crash, an over-read, or a case where a hostile file can write outside the path you passed, open a private security advisory on this repository. Please include the file, if it is safe to share, and the version from `srms --version`.

There is no separate security address.
