# CSA-Part1

A simple direct-mapped cache simulator written in C.

## Files

- `cache_sim.c` – the program
- `trace.txt`   – the input memory trace
- `Makefile`    – builds the program

## Cache settings (edit at the top of `cache_sim.c` if you want to change them)

| setting       | value     |
|---------------|-----------|
| cache size    | 1024 B    |
| block size    | 32 B      |
| word size     | 4 B       |
| organization  | direct-mapped |

## Trace format

Each line of `trace.txt` looks like:

```
<op> <address_in_hex> <data>
```

- `<op>` is `R` or `W`
- Blank lines and lines starting with `#` are ignored

Example:

```
R 0x00000000 0
W 0x00000004 42
```

## Build and run

```bash
make
./cache_sim
```

The program prints the counters from the pseudo code:

```
CPUR, CPUW, NCRH, NCRM, NCWH, NCWM, NRA, NWA
```
