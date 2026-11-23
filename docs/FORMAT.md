# Register format

Version 1. The file is UTF-8 text with LF newlines. A writer may be read back by this program only when every check below passes.

## Layout

```
SRMS 1
count <N>
crc32 <8 hex digits>
---
<N record lines>
```

`SRMS 1` is the magic and the format version. This build reads version 1 and rejects every other version.

`count` is a decimal integer, the exact number of record lines. It is at most 10000.

`crc32` is the CRC-32 of the bytes after the `---\n` line, including the newlines of the records. The algorithm is the ISO-HDLC CRC-32: polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`. The digest of the empty payload is `00000000`. The digest of the ASCII string `123456789` is `cbf43926`. Hex digits may be upper or lower case on read. This program writes lower case.

The separator line is exactly `---`.

## Record

Nine fields, separated by `|`, no spaces around the separator:

```
roll|name|programme|year|paper_1|paper_2|paper_3|paper_4|practical
```

| Field | Rule |
| --- | --- |
| roll | Integer from 1 to 999999, unique in the file |
| name | 1–39 characters: letters, space, hyphen, apostrophe, period. No leading, trailing, or doubled space |
| programme | Same character set, 1–23 characters |
| year | Integer from 1970 to 2100 |
| papers | Five integers, each the mark in tenths, from 0 to 1000 |

`842` is 84.2. The percentage is not stored. It is the mean of the five marks, in tenths, rounded half up. The mean of tenths falls on a fifth, so the second decimal is never 5 and the rounding has no tie.

## Grades

| Grade | Percentage |
| --- | --- |
| A | 90.0–100.0 |
| B | 80.0–89.9 |
| C | 70.0–79.9 |
| D | 60.0–69.9 |
| E | 50.0–59.9 |
| F | below 50.0 |

## Writes

`storage_save` builds the payload, computes the CRC-32, writes `path.tmp` in the same directory, `fsync`s it, renames it over `path`, and `fsync`s the directory. A crash during the write leaves the previous file in place.

## CSV

Export writes one header and one row per record, in the current list order:

```
roll,name,programme,year,paper_1,paper_2,paper_3,paper_4,practical,percentage,grade
```

Fields that contain a comma, a quote, or a newline are quoted, and quotes are doubled. Import requires `name`, `programme`, `year`, the four papers, and `practical`. `roll` is optional. When it is present it is kept. When it is absent, rolls are assigned in file order as the smallest free number. `percentage` and `grade` are ignored and recomputed. Unknown columns are rejected. A byte-order mark is rejected.
