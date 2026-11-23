# Decisions

## 1. The register is a singly linked list

Records are inserted in file order and walked in that order. A singly linked list makes that ownership obvious: one `malloc` per record, one walk to free them, and a stable merge sort that rewires pointers instead of moving students. Cohorts here are at most 10,000 records. The cost of a linear walk is not the constraint.

The smallest free roll is at most `count + 1`. Finding it is one pass over a byte array of that length.

## 2. Marks are integer tenths

A mark is an integer from 0 to 1000. 84.2 is stored as 842. The percentage is the mean of the five papers, in the same units, rounded half up. Nothing in the core uses binary floating point, so a file written on one machine matches a file written on another.

## 3. The file is text, with a checksum

A binary dump would be smaller and harder to repair. The text form can be diffed, and a CRC-32 over the payload catches a truncated or hand-edited file before any record replaces the list in memory. The header (magic, count, checksum) sits outside the checksum so a reader can report which check failed. The count must still match the number of lines, or the load is rejected.

Writes are atomic: a temporary file in the same directory, `fsync`, then `rename`.

## 4. The roll does not change

The roll is the identity of the row. Modify edits the name, the programme, the year, and the marks. Delete frees the roll, and the next add takes the smallest free number, which may be the one just freed.

## 5. Sort is stable and it mutates the session

Merge sort keeps the previous order of equal keys. Name comparison is ASCII case-insensitive and does not use the roll as a tie break, so two students with the same name stay in the order they had. Percentage works the same way. The new order is part of the session. It reaches disk only on save.

## 6. One accent

The screen uses the paper, ink, muted, and accent values from `docs/DESIGN.md`. Errors are the word `Error` in the accent, not a second colour. `--no-color`, a non-terminal, or `NO_COLOR` drops the escapes. `--color` forces them.

## 7. CSV is a projection, not the register

The register file is the source of truth. CSV is an extract for a spreadsheet and a way to load a cohort. Percentage and grade in a CSV are recomputed on import, so a stale grade in a sheet cannot enter the list.
