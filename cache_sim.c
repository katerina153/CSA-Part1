/*
 * cache_sim.c
 *
 * Direct-mapped cache controller simulator.
 *
 * File          : cache_sim.c
 * Author        : <your name>
 * Student ID    : <your student ID>
 * Created       : 2026-05-09
 * Last modified : 2026-05-09
 *
 * What this program does
 * ----------------------
 * The processor has a 32-bit address bus and a 16-bit data bus.  A direct
 * mapped cache sits between the CPU and main memory.  This program does NOT
 * actually store any data; it just *counts* how the cache controller would
 * respond to each memory access in a trace file.  Counting is enough to
 * study how the cache geometry affects performance.
 *
 * Cache policy implemented
 *   * Direct mapped       (no associativity; the index alone selects the
 *                          cache slot).
 *   * Write-allocate      (a write that misses still loads the missing
 *                          block from memory before performing the write).
 *   * Write-through       (every CPU write also writes one word straight
 *                          through to main memory, so memory and cache
 *                          stay consistent; there is no dirty bit).
 *
 * Address layout used by a direct-mapped cache
 * --------------------------------------------
 *   bits 31 .. (offset_bits + index_bits)              the Tag
 *   bits (offset_bits + index_bits - 1) .. offset_bits the Cache index
 *   bits (offset_bits - 1) .. 0                        the Block offset
 *
 * The number of offset bits is log2(words_in_block); the number of index
 * bits is log2(blocks_in_cache); the tag fills the remaining high bits.
 *
 * Software flowchart implemented below
 * ------------------------------------
 *   For every non-comment, non-blank line of the trace file:
 *
 *     read op, address, data
 *     index = (address >> offset_bits) & (blocks_in_cache - 1)
 *     tag   =  address >> (offset_bits + index_bits)
 *
 *     if op == 'R':                    (CPU read)
 *         CPUR++
 *         if valid[index] and tag[index] == tag:    -> NCRH++   (read hit)
 *         else:                                                 (read miss)
 *             NCRM++
 *             NRA += words_in_block    (load the missing block)
 *             valid[index] = 1; tag[index] = tag
 *
 *     if op == 'W':                    (CPU write)
 *         CPUW++
 *         if valid[index] and tag[index] == tag:    -> NCWH++   (write hit)
 *             NWA++                    (write the 1 word through to memory)
 *         else:                                                 (write miss)
 *             NCWM++
 *             NRA += words_in_block    (write-allocate: load the block)
 *             valid[index] = 1; tag[index] = tag
 *             NWA++                    (write the 1 word through to memory)
 *
 * Usage
 * -----
 *     cache_sim trace_file blocks_in_cache words_in_block
 *
 *     trace_file       path to a memory trace file
 *     blocks_in_cache  power of 2 in [2, 512]
 *     words_in_block   power of 2 in [2, 256]
 *
 * Example
 *     ./cache_sim trace_file.txt 256 16
 *
 * Build (ANSI C17):
 *     gcc -std=c17 -Wall -Wextra -o cache_sim cache_sim.c
 *
 * AI usage statement
 * ------------------
 * <Describe here which AI tool(s) you used, the prompts you supplied, and
 *  which lines of code were generated or modified by AI.  Fill this in
 *  before submitting.>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


/*
 * Hardware envelope (taken from the lab brief).
 *   ADDRESS_BITS         : width of the processor's address bus.
 *   MIN/MAX_BLOCKS_IN_..   allowed range for the number of cache blocks.
 *   MIN/MAX_WORDS_IN_..    allowed range for the words-per-block parameter.
 *   MAX_TRACE_LINE_LEN   : longest trace line we will read in one go.
 */
#define ADDRESS_BITS         32
#define MIN_BLOCKS_IN_CACHE   2
#define MAX_BLOCKS_IN_CACHE 512
#define MIN_WORDS_IN_BLOCK    2
#define MAX_WORDS_IN_BLOCK  256
#define MAX_TRACE_LINE_LEN  256


/*
 * One slot of the direct-mapped cache.  We don't simulate the data words
 * themselves because the lab only asks for the *counters*.  We just need:
 *   valid - has anything ever been loaded into this slot?
 *   tag   - which memory block currently sits in this slot?
 */
struct cache_line {
    int      valid;
    uint32_t tag;
};


/*
 * is_power_of_two
 *   The cache geometry must use powers of two so that the index and offset
 *   fields of the address line up on bit boundaries.  Standard trick:
 *   for n > 0, n is a power of two iff n & (n - 1) == 0.
 */
static int is_power_of_two(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}


/*
 * log2_int
 *   Returns the integer base-2 logarithm of n.  Pre-condition: n is a
 *   positive power of two (we always check that with is_power_of_two
 *   first).  Used to convert words_in_block / blocks_in_cache into the
 *   number of bits used for the offset / index fields of the address.
 */
static int log2_int(int n)
{
    int bits = 0;
    while (n > 1) {
        n >>= 1;
        bits++;
    }
    return bits;
}


/*
 * line_is_blank
 *   Trace lines that are empty or contain only whitespace must be ignored.
 *   fgets keeps the trailing newline (and on some platforms a '\r'), so we
 *   treat those as whitespace too.
 */
static int line_is_blank(const char *line)
{
    int i;
    for (i = 0; line[i] != '\0'; i++) {
        if (line[i] != ' '  && line[i] != '\t' &&
            line[i] != '\n' && line[i] != '\r') {
            return 0;
        }
    }
    return 1;
}


int main(int argc, char *argv[])
{
    /* --------------------------------------------------------------
     * Step 1 - command line parsing and validation.
     *
     *   cache_sim trace_file blocks_in_cache words_in_block
     *
     * argc must be exactly 4 (program name + 3 args).
     * -------------------------------------------------------------- */
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s trace_file blocks_in_cache words_in_block\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *trace_filename   = argv[1];
    int         blocks_in_cache  = atoi(argv[2]);
    int         words_in_block   = atoi(argv[3]);

    /* Cache geometry must be powers of two and inside the hardware
     * envelope: 2..512 blocks and 2..256 words per block. */
    if (blocks_in_cache < MIN_BLOCKS_IN_CACHE ||
        blocks_in_cache > MAX_BLOCKS_IN_CACHE ||
        !is_power_of_two(blocks_in_cache)) {
        fprintf(stderr,
                "Error: blocks_in_cache must be a power of 2 in the "
                "range [%d, %d].\n",
                MIN_BLOCKS_IN_CACHE, MAX_BLOCKS_IN_CACHE);
        return EXIT_FAILURE;
    }
    if (words_in_block < MIN_WORDS_IN_BLOCK ||
        words_in_block > MAX_WORDS_IN_BLOCK ||
        !is_power_of_two(words_in_block)) {
        fprintf(stderr,
                "Error: words_in_block must be a power of 2 in the "
                "range [%d, %d].\n",
                MIN_WORDS_IN_BLOCK, MAX_WORDS_IN_BLOCK);
        return EXIT_FAILURE;
    }

    /* --------------------------------------------------------------
     * Step 2 - precompute the address-decoding constants.
     *
     *   offset_bits : number of bits used for the block offset
     *                 (= log2 of words_in_block).
     *   index_bits  : number of bits used for the cache index
     *                 (= log2 of blocks_in_cache).
     *   index_mask  : (blocks_in_cache - 1).  AND-ing the shifted address
     *                 with this mask keeps just the index bits.
     * -------------------------------------------------------------- */
    int      offset_bits = log2_int(words_in_block);
    int      index_bits  = log2_int(blocks_in_cache);
    if (offset_bits + index_bits >= ADDRESS_BITS) {
        fprintf(stderr,
                "Error: chosen cache geometry leaves no tag bits.\n");
        return EXIT_FAILURE;
    }
    uint32_t index_mask = (uint32_t)(blocks_in_cache - 1);

    /* --------------------------------------------------------------
     * Step 3 - allocate and initialise the cache.
     *
     * calloc zeroes the memory, giving valid=0 and tag=0 for every slot
     * (i.e. an empty cache - exactly the post-reset state).
     * -------------------------------------------------------------- */
    struct cache_line *cache =
        calloc((size_t)blocks_in_cache, sizeof(struct cache_line));
    if (cache == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        return EXIT_FAILURE;
    }

    /* --------------------------------------------------------------
     * Step 4 - open the trace file in text mode.
     * -------------------------------------------------------------- */
    FILE *trace_fp = fopen(trace_filename, "r");
    if (trace_fp == NULL) {
        fprintf(stderr,
                "Error: cannot open trace file '%s'.\n",
                trace_filename);
        free(cache);
        return EXIT_FAILURE;
    }

    /* --------------------------------------------------------------
     * Step 5 - the eight performance counters.
     *
     *   CPUR  - total CPU read  accesses
     *   CPUW  - total CPU write accesses
     *   NCRH  - cache read  hits
     *   NCRM  - cache read  misses
     *   NCWH  - cache write hits
     *   NCWM  - cache write misses
     *   NRA   - words read   from external memory  (block loads)
     *   NWA   - words written to external memory   (write-throughs)
     * -------------------------------------------------------------- */
    int CPUR = 0, CPUW = 0;
    int NCRH = 0, NCRM = 0;
    int NCWH = 0, NCWM = 0;
    int NRA  = 0, NWA  = 0;

    /* --------------------------------------------------------------
     * Step 6 - the main simulation loop: one iteration per trace line.
     * -------------------------------------------------------------- */
    char line[MAX_TRACE_LINE_LEN];

    while (fgets(line, sizeof(line), trace_fp) != NULL) {

        char op = line[0];

        /* '!' in column 1 -> the whole line is a comment. */
        if (op == '!')                continue;

        /* Blank lines and whitespace-only lines are ignored. */
        if (line_is_blank(line))      continue;

        /* Anything else with a non-R / non-W first character is
         * malformed; skip silently for robustness. */
        if (op != 'R' && op != 'W')   continue;

        /* The data field is parsed for validation but not used by the
         * simulator (we don't model the actual contents of memory). */
        unsigned long address_in = 0;
        unsigned long data_in    = 0;
        if (sscanf(line + 1, "%lx %lx", &address_in, &data_in) < 1) {
            continue;
        }
        (void)data_in;

        /* Split the 32-bit address into tag / index / (offset).
         * We don't need the offset value; we never look inside a block. */
        uint32_t address     = (uint32_t)address_in;
        uint32_t cache_index = (address >> offset_bits) & index_mask;
        uint32_t tag         = address >> (offset_bits + index_bits);

        /* Pointer to the cache slot this address maps to.  In a
         * direct-mapped cache the index uniquely selects the slot. */
        struct cache_line *block = &cache[cache_index];

        if (op == 'R') {
            /* ------------------- READ -------------------- */
            CPUR++;

            if (block->valid && block->tag == tag) {
                /* Read hit: requested block is already resident. */
                NCRH++;
            } else {
                /* Read miss: cold (slot empty) OR wrong tag.  Either
                 * way, bring the requested block in from main memory. */
                NCRM++;
                NRA += words_in_block;
                block->valid = 1;
                block->tag   = tag;
            }
        } else {
            /* ------------------- WRITE ------------------- */
            CPUW++;

            if (block->valid && block->tag == tag) {
                /* Write hit: write the word into the cache AND through
                 * to memory (write-through). */
                NCWH++;
                NWA++;
            } else {
                /* Write miss: write-allocate -> load the block first;
                 * then write the word into the cache and through to
                 * memory (write-through). */
                NCWM++;
                NRA += words_in_block;
                block->valid = 1;
                block->tag   = tag;
                NWA++;
            }
        }
    }

    fclose(trace_fp);

    /* --------------------------------------------------------------
     * Step 7 - print the results.
     *
     * One labelled counter per line so the output is easy to read at
     * the terminal.  WIB and BIC are echoed at the end so the reader
     * can see the cache geometry that produced these numbers.
     * -------------------------------------------------------------- */
    printf("CPUR = %d\n", CPUR);
    printf("CPUW = %d\n", CPUW);
    printf("NCRH = %d\n", NCRH);
    printf("NCRM = %d\n", NCRM);
    printf("NCWH = %d\n", NCWH);
    printf("NCWM = %d\n", NCWM);
    printf("NRA  = %d\n", NRA);
    printf("NWA  = %d\n", NWA);
    printf("WIB  = %d\n", words_in_block);
    printf("BIC  = %d\n", blocks_in_cache);
    printf("file = %s\n", trace_filename);

    free(cache);
    return EXIT_SUCCESS;
}
