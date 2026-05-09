# CSA-Part1

A small **direct-mapped cache simulator** written in C, implementing the
algorithm described in the project pseudo code.

## Files

- `cache_sim.c`     – the simulator (single translation unit, C99).
- `Makefile`        – convenience build / run / clean targets.
- `sample_trace.txt` – a tiny example trace used by `make run`.

## Build

```bash
make
```

This produces an executable called `cache_sim`.

## Usage

```text
./cache_sim <trace_file> [cache_size_bytes] [block_size_bytes]
                         [word_size_bytes]  [address_bits]
```

Defaults (when arguments are omitted):

| parameter           | default |
|---------------------|---------|
| `cache_size_bytes`  | 1024    |
| `block_size_bytes`  | 32      |
| `word_size_bytes`   | 4       |
| `address_bits`      | 32      |

Cache-size, block-size and word-size must be powers of two.

### Trace format

Each non-empty / non-comment line in the trace must look like:

```text
<op> <address> [<data>]
```

- `<op>` is `R` (read) or `W` (write) – case-insensitive.
- `<address>` is decimal, or hexadecimal when prefixed with `0x`.
- `<data>` is optional and is ignored by the simulator.
- Lines that are blank or that start with `#` are ignored.

### Example

```bash
make run
```

equivalent to:

```bash
./cache_sim sample_trace.txt
```

## Reported counters

| symbol | meaning                                |
|--------|----------------------------------------|
| `CPUR` | total CPU read  requests               |
| `CPUW` | total CPU write requests               |
| `NCRH` | cache read  hits                       |
| `NCRM` | cache read  misses                     |
| `NCWH` | cache write hits                       |
| `NCWM` | cache write misses                     |
| `NRA`  | number of *words* read from main memory|
| `NWA`  | number of *words* written to main mem. |

The miss policy follows the supplied pseudo code:

- **Read miss**  → load the block (`NRA += words_in_block`).
- **Write hit**  → write-through (`NWA += 1`).
- **Write miss** → write-allocate **and** write-through
  (`NRA += words_in_block`, `NWA += 1`).
