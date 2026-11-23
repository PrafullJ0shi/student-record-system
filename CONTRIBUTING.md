# Contributing

Bugs and small patches are welcome. Open an issue before a large change, so the file format and the screen layout stay compatible.

## Build and test

```sh
make
make test
make san
```

`make san` rebuilds with AddressSanitizer and UndefinedBehaviorSanitizer. A release build is also checked with:

```sh
valgrind --leak-check=full --show-leak-kinds=all ./build/srms_tests
```

CMake is the other entry:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Pass `-DSRMS_SANITIZERS=ON` for the same sanitizers. The code is C11 and is expected to be clean under gcc and clang with `-Wall -Wextra -Wpedantic -Werror`.

## Style

Four spaces, no tabs, braces on the next line for functions. Errors return an `SrmsStatus` and a sentence in `srms_detail()`. Library code does not print. The screens in `ui.c` are the only place that knows about colour.

Keep the palette in `docs/DESIGN.md`. Do not add a second accent.

## Sample cohort

`data/sample_cohort.csv` is the source. Regenerate the register with:

```sh
./build/srms --import-csv data/sample_cohort.csv --file data/sample_cohort.srms
```

The people in that file are fictional.
