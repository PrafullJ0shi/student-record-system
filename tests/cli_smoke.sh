#!/bin/sh
# Exercise the installed command the way an operator would.
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: cli_smoke.sh /path/to/srms" >&2
    exit 2
fi

SRMS=$1
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

"$SRMS" --version | grep -q "srms 1.0.0"
"$SRMS" --help | grep -q "Usage"

"$SRMS" --no-color --ascii --file "$TMP/reg.srms" <<'EOF'
1
Ada Lovelace
Mathematics
2020
90
91
92
93
94
y

9

3

0
EOF

"$SRMS" --no-color --ascii --file "$TMP/reg.srms" --stats | grep -q "Average"
"$SRMS" --file "$TMP/reg.srms" --export-csv "$TMP/out.csv"
grep -q "Ada Lovelace" "$TMP/out.csv"
"$SRMS" --file "$TMP/copy.srms" --import-csv "$TMP/out.csv"
"$SRMS" --file "$TMP/copy.srms" --report - | grep -q "count	1"

printf 'not a register\n' > "$TMP/bad.srms"
if "$SRMS" --file "$TMP/bad.srms" --stats >/dev/null 2>"$TMP/err"; then
    echo "corrupt register was accepted" >&2
    exit 1
fi
grep -q "Error" "$TMP/err"

echo "cli smoke passed"
