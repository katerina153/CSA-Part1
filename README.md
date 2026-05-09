# CSA-Part1 — Direct-Mapped Cache Simulator

C program that simulates the operation of a direct-mapped cache controller
for a processor with a 32-bit address bus and a 16-bit data bus. Both
**Write-Allocate / Write-Back (WAWB)** and **Write-Allocate / Write-Through
(WAWT)** policies are supported.

> **Important — academic integrity**
> The lab brief states: *"The program must not be stored in a software
> repository, such as GitHub, with shared access."* Make sure this repository
> is **private** and not shared with anyone else, and remember to remove or
> replace the placeholder author / student-ID fields before submission.

## Files

| file | purpose |
|------|---------|
| `cachesim.c`           | the simulator (single C17 source file) |
| `WAWB_validation.txt`  | validation trace for the WAWB policy |
| `WAWT_validation.txt`  | validation trace for the WAWT policy |
| `Makefile`             | convenience build / validate / clean targets |

When submitting on Canvas, rename the files to match the spec:

```
<familyname>_<studentID>_cachesim.c
<familyname>_<studentID>_WAWB_validation.txt
<familyname>_<studentID>_WAWT_validation.txt
```

(e.g. `Green_1234567_cachesim.c`).

## Build

```bash
make
```

This compiles with `gcc -std=c17 -Wall -Wextra -O2`.

## Usage

```text
cachesim trace_filename write_policy blocks_in_cache words_in_block
```

| argument          | accepted values                                |
|-------------------|------------------------------------------------|
| `trace_filename`  | path to a memory-trace `.txt` file             |
| `write_policy`    | `WAWB` or `WAWT`                               |
| `blocks_in_cache` | power of 2 in `[2, 512]`                       |
| `words_in_block`  | power of 2 in `[2, 256]`                       |

## Output

A single line of 12 space-separated items written to `stdout`:

```text
CPUR CPUW NRA NWA NCRH NCRM NCWH NCWM WIB BIC filename WP
```

| field      | meaning                                         |
|------------|-------------------------------------------------|
| `CPUR`     | total CPU read accesses                         |
| `CPUW`     | total CPU write accesses                        |
| `NRA`      | words read  from external memory                |
| `NWA`      | words written to external memory                |
| `NCRH`     | cache read  hits                                |
| `NCRM`     | cache read  misses                              |
| `NCWH`     | cache write hits                                |
| `NCWM`     | cache write misses                              |
| `WIB`      | words in a cache block                          |
| `BIC`      | blocks in the cache                             |
| `filename` | trace file path that was simulated              |
| `WP`       | write policy (`WAWB` or `WAWT`)                 |

## Trace file format

```text
R<sp>address<sp>data       hexadecimal read access
W<sp>address<sp>data       hexadecimal write access
! free-form comment
```

* `R`, `W` and `!` must appear in column 1.
* `address` and `data` are hexadecimal and may be up to 8 hex digits.
* Blank lines and lines that contain only whitespace are ignored.

## Validation

Run both validation traces:

```bash
make validate
```

### Expected output

```text
8 6 112 32 5 3 2 4 16 256 WAWB_validation.txt WAWB
5 10 144 10 2 3 4 6 16 256 WAWT_validation.txt WAWT
```

`WAWB_validation.txt` exercises **all 8** decision paths of the WAWB
flowchart (RH; RM-invalid / RM-valid-clean / RM-valid-dirty; WH;
WM-invalid / WM-valid-clean / WM-valid-dirty) and every numeric output
value is distinct.

`WAWT_validation.txt` exercises **all 6** decision paths of the WAWT
flowchart (RH; RM-invalid / RM-valid; WH; WM-invalid / WM-valid). Under
WAWT every CPU write produces exactly one external write, so
`NWA == CPUW` is a structural property of the policy and cannot be
broken by trace design; every other pair of numeric outputs is distinct.

Each trace file contains a comment header (filename, author, student ID,
date, command-line parameters) and section comments that describe what is
being validated and the expected counter values after each operation, as
required by the lab brief.
