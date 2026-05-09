# CSA-Part1 — Direct-Mapped Cache Simulator

C program that simulates the operation of a direct-mapped cache controller
for a processor with a 32-bit address bus and a 16-bit data bus. The cache
is **write-allocate / write-through**: every CPU write also writes one word
straight through to main memory, and a write that misses still loads the
containing block first.

## Files

| file              | purpose                                            |
|-------------------|----------------------------------------------------|
| `cache_sim.c`     | the simulator (single C17 source file)             |
| `trace_file.txt`  | validation trace that exercises every branch       |
| `Makefile`        | convenience build / run / clean targets            |

## Build

```bash
make
```

This compiles with `gcc -std=c17 -Wall -Wextra -O2`.

## Usage

```text
cache_sim trace_file blocks_in_cache words_in_block
```

| argument          | accepted values                                |
|-------------------|------------------------------------------------|
| `trace_file`      | path to a memory-trace `.txt` file             |
| `blocks_in_cache` | power of 2 in `[2, 512]`                       |
| `words_in_block`  | power of 2 in `[2, 256]`                       |

Example:

```bash
./cache_sim trace_file.txt 256 16
```

## Output

One labelled counter per line, finishing with the cache geometry and the
trace file name:

```text
CPUR = ...
CPUW = ...
NCRH = ...
NCRM = ...
NCWH = ...
NCWM = ...
NRA  = ...
NWA  = ...
WIB  = ...
BIC  = ...
file = ...
```

| field      | meaning                                         |
|------------|-------------------------------------------------|
| `CPUR`     | total CPU read accesses                         |
| `CPUW`     | total CPU write accesses                        |
| `NCRH`     | cache read  hits                                |
| `NCRM`     | cache read  misses                              |
| `NCWH`     | cache write hits                                |
| `NCWM`     | cache write misses                              |
| `NRA`      | words read  from external memory                |
| `NWA`      | words written to external memory                |
| `WIB`      | words in a cache block (echo of `words_in_block`)  |
| `BIC`      | blocks in the cache (echo of `blocks_in_cache`) |
| `file`     | trace file path that was simulated              |

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

```bash
make run
```

equivalent to:

```bash
./cache_sim trace_file.txt 256 16
```

### Expected output

```text
CPUR = 5
CPUW = 10
NCRH = 2
NCRM = 3
NCWH = 4
NCWM = 6
NRA  = 144
NWA  = 10
WIB  = 16
BIC  = 256
file = trace_file.txt
```

`trace_file.txt` exercises **all 6** decision paths of the flowchart
(`RH`, `RM-invalid`, `RM-valid`, `WH`, `WM-invalid`, `WM-valid`) and is
laid out so the offset / index / tag fields fall on hex-digit boundaries
(1 / 2 / 5 hex digits respectively). Each individual `R` / `W` line is
preceded by a numbered comment block (`Op 1`, `Op 2`, …) that explains
which branch it tests, the decoded `tag` / `index` / `offset` of the
address, and the cumulative counter values after the access.

For a write-allocate / write-through cache, every CPU write produces
exactly one external write, so `NWA == CPUW` is a structural property of
the policy and cannot be removed by trace design. Every other pair of
counter values in the output is distinct, so any counter swap that does
not involve the `CPUW` / `NWA` pair changes the simulator's output.
