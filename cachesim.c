/*
 * cachesim.c
 *
 * Direct-mapped cache controller simulator.
 *
 * File          : cachesim.c
 * Author        : <your name>
 * Student ID    : <your student ID>
 * Created       : 2026-05-09
 * Last modified : 2026-05-09
 *
 * What this program does
 * ----------------------
 * The processor has a 32-bit address bus and a 16-bit data bus, with a
 * direct-mapped cache sitting between the CPU and main memory.  This program
 * does not actually store any data; it just *counts* how the cache controller
 * would respond to each memory access in a trace file.  Counting is enough
 * to study how the cache geometry and the write policy affect performance.
 *
 * The two write policies supported are:
 *
 *   WAWB - Write-Allocate / Write-Back
 *          On a write hit we only mark the block "dirty" and write nothing
 *          to memory.  Memory is updated lazily: when a dirty block is
 *          evicted (because of a later miss on the same cache index) the
 *          whole block is written back to main memory.
 *
 *   WAWT - Write-Allocate / Write-Through
 *          Every CPU write also writes the same word straight through to
 *          main memory.  There is no dirty bit and no write-back, but writes
 *          generate a lot more memory traffic.
 *
 * Both policies are write-allocate: a write that misses still causes the
 * containing block to be loaded from memory before the write is performed.
 *
 * Address layout used by a direct-mapped cache
 * --------------------------------------------
 *   bits 31 .. (offset_bits + index_bits)   the Tag
 *   bits (offset_bits + index_bits - 1) .. offset_bits   the Cache index
 *   bits (offset_bits - 1) .. 0             the Block offset (word inside
 *                                           the cache block)
 *
 * The number of offset bits is log2(words_in_block); the number of index
 * bits is log2(blocks_in_cache); the tag fills the remaining high bits.
 *
 * Software flowchart implemented below
 * ------------------------------------
 *   For every non-comment, non-blank line of the trace file:
 *
 *     read op, address, data
 *     index  = (address >> offset_bits) & (blocks_in_cache - 1)
 *     tag    =  address >> (offset_bits + index_bits)
 *
 *     if op == 'R':                              (CPU read)
 *         CPUR++
 *         if valid[index] and tag[index] == tag:        -> NCRH++   (RH)
 *         else:                                                     (RM)
 *             NCRM++
 *             if WAWB and valid[index] and dirty[index]:
 *                 NWA += words_in_block          (write back the dirty block)
 *             NRA += words_in_block              (load the new block)
 *             valid[index]=1; dirty[index]=0; tag[index]=tag
 *
 *     if op == 'W':                              (CPU write)
 *         CPUW++
 *         if valid[index] and tag[index] == tag:        -> NCWH++   (WH)
 *             if WAWB:  dirty[index] = 1         (lazy: just mark dirty)
 *             else:     NWA++                    (write through 1 word)
 *         else:                                                     (WM)
 *             NCWM++
 *             if WAWB and valid[index] and dirty[index]:
 *                 NWA += words_in_block          (write back the dirty block)
 *             NRA += words_in_block              (write-allocate: load block)
 *             valid[index]=1; tag[index]=tag
 *             if WAWB:  dirty[index] = 1
 *             else:     dirty[index] = 0; NWA++  (write through 1 word)
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
 * Constants describing the hardware envelope set by the lab brief.
 *   ADDRESS_BITS         : width of the processor's address bus.
 *   MIN/MAX_BLOCKS_IN_..   allowed range for the number of cache blocks.
 *   MIN/MAX_WORDS_IN_..    allowed range for the words-per-block parameter.
 *   MAX_TRACE_LINE_LEN   : longest trace line we will read in one go.  The
 *                          trace format puts at most  R/W/!  + space + 8 hex
 *                          digits + space + 8 hex digits + newline on a
 *                          line, so a few tens of characters suffice; 256
 *                          leaves comfortable room for any comment line.
 */
#define ADDRESS_BITS         32
#define MIN_BLOCKS_IN_CACHE   2
#define MAX_BLOCKS_IN_CACHE 512
#define MIN_WORDS_IN_BLOCK    2
#define MAX_WORDS_IN_BLOCK  256
#define MAX_TRACE_LINE_LEN  256


/*
 * One slot of the direct-mapped cache.  We don't simulate the data words
 * themselves because the lab only asks for the *counters*.  We just need to
 * know:
 *   valid  - has anything ever been loaded into this slot?
 *   dirty  - has the CPU written to it since it was loaded? (WAWB only)
 *   tag    - which memory block currently sits in this slot?
 */
struct cache_line {
    int      valid;
    int      dirty;
    uint32_t tag;
};


/*
 * is_power_of_two
 *   The cache geometry must use powers of two so that the index and offset
 *   fields of the address line up on bit boundaries.  The standard trick:
 *   for n > 0, n is a power of two iff n & (n - 1) == 0 (the bit pattern
 *   has exactly one 1-bit, and subtracting 1 turns that bit off).
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
 *   The trace specification says blank lines and lines that contain only
 *   whitespace must be ignored.  fgets keeps the trailing '\n' (and on
 *   some platforms '\r\n'), so we treat those as whitespace too.
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
     * The brief specifies four positional arguments in this order:
     *     trace_filename  write_policy  blocks_in_cache  words_in_block
     *
     * argc therefore must be exactly 5 (program name + 4 args).  Anything
     * else is a usage error, so we print a usage message to stderr and
     * exit with a non-zero status so shells / Makefiles can detect it.
     * -------------------------------------------------------------- */
    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s trace_filename write_policy "
                "blocks_in_cache words_in_block\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *trace_filename = argv[1];
    const char *write_policy   = argv[2];
    int         blocks_in_cache = atoi(argv[3]);
    int         words_in_block  = atoi(argv[4]);

    /* The write policy must match one of the two strings exactly.  Any
     * typo (e.g. "wawb", "WAWB ", "WB") is rejected so we never silently
     * simulate the wrong policy.  We also remember the choice as a small
     * boolean to keep the main loop branch-free of string comparisons. */
    if (strcmp(write_policy, "WAWB") != 0 &&
        strcmp(write_policy, "WAWT") != 0) {
        fprintf(stderr,
                "Error: write_policy must be 'WAWB' or 'WAWT'.\n");
        return EXIT_FAILURE;
    }
    int is_write_back = (strcmp(write_policy, "WAWB") == 0);

    /* Cache geometry must be powers of two and within the hardware
     * envelope from the brief: 2..512 blocks, 2..256 words per block. */
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
     * Once we know how many bits are used for the offset and the index
     * we never need to recompute them inside the per-access loop.
     *
     *   offset_bits  : number of bits used for the block offset
     *                  (= log2 of words_in_block).
     *   index_bits   : number of bits used for the cache index
     *                  (= log2 of blocks_in_cache).
     *   index_mask   : (blocks_in_cache - 1).  AND-ing the shifted address
     *                  with this mask keeps just the index bits.
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
     * calloc zeroes the memory, which gives us valid=0, dirty=0, tag=0 for
     * every block - i.e. an empty cache, exactly the post-reset state.
     * -------------------------------------------------------------- */
    struct cache_line *cache =
        calloc((size_t)blocks_in_cache, sizeof(struct cache_line));
    if (cache == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        return EXIT_FAILURE;
    }

    /* --------------------------------------------------------------
     * Step 4 - open the trace file.
     *
     * Opened in text mode ("r"): fgets gives us one logical line per call
     * and converts platform-specific line endings into '\n'.
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
     * Step 5 - the eight performance counters required by the brief.
     *
     *   CPUR  - total CPU read  accesses
     *   CPUW  - total CPU write accesses
     *   NRA   - words read   from external memory  (block loads)
     *   NWA   - words written to external memory   (write-throughs and
     *                                                write-backs)
     *   NCRH  - cache read  hits
     *   NCRM  - cache read  misses
     *   NCWH  - cache write hits
     *   NCWM  - cache write misses
     *
     * They are all signed ints because the trace files we use are short
     * enough that there is no risk of overflow; for very long traces a
     * larger type (e.g. long long) could be substituted with no other
     * change to the program.
     * -------------------------------------------------------------- */
    int CPUR = 0, CPUW = 0;
    int NRA  = 0, NWA  = 0;
    int NCRH = 0, NCRM = 0;
    int NCWH = 0, NCWM = 0;

    /* --------------------------------------------------------------
     * Step 6 - the main simulation loop.  One iteration per line of the
     * trace file.  We pull the next line into the local buffer with
     * fgets and then decide what to do based on the first character.
     * -------------------------------------------------------------- */
    char line[MAX_TRACE_LINE_LEN];

    while (fgets(line, sizeof(line), trace_fp) != NULL) {

        char op = line[0];

        /* Per the brief: '!' in column 1 means the whole line is a
         * comment.  We just skip to the next line. */
        if (op == '!')                continue;

        /* Blank lines and whitespace-only lines are ignored. */
        if (line_is_blank(line))      continue;

        /* Anything else with a non-R / non-W first character is malformed.
         * We choose the lenient option: skip silently rather than abort,
         * which makes the simulator robust against editor artefacts in a
         * trace file (e.g. a stray byte at start-of-file). */
        if (op != 'R' && op != 'W')   continue;

        /* The data field is read but never used by the simulator (we
         * don't model the actual contents of memory).  Still, parsing it
         * helps validate that the trace line is well-formed. */
        unsigned long address_in = 0;
        unsigned long data_in    = 0;
        if (sscanf(line + 1, "%lx %lx", &address_in, &data_in) < 1) {
            continue;
        }
        (void)data_in;

        /* Split the 32-bit address into tag / index / offset.
         * We don't need the offset value (we never look inside a block),
         * so we don't compute it here. */
        uint32_t address     = (uint32_t)address_in;
        uint32_t cache_index = (address >> offset_bits) & index_mask;
        uint32_t tag         = address >> (offset_bits + index_bits);

        /* Pointer to the cache slot this address maps to.  In a
         * direct-mapped cache the index *uniquely* selects the slot;
         * there is no associativity to search. */
        struct cache_line *block = &cache[cache_index];

        if (op == 'R') {
            /* ------------------- READ -------------------- */
            CPUR++;

            if (block->valid && block->tag == tag) {
                /* Branch RH: the requested block is already resident. */
                NCRH++;
            } else {
                /* Branch RM: read miss.  Whether the slot was empty
                 * (cold miss) or held a different block (capacity /
                 * conflict miss), we have to bring the requested block
                 * in from main memory. */
                NCRM++;

                /* Under WAWB, if the slot held a *dirty* block we must
                 * first write that block back to memory before
                 * overwriting it.  Each block is words_in_block words
                 * long, so the write back is words_in_block memory
                 * writes.  WAWT has no dirty bit, so this branch
                 * never fires for it. */
                if (is_write_back && block->valid && block->dirty) {
                    NWA += words_in_block;
                }

                /* Load the new block from memory:  words_in_block
                 * memory reads. */
                NRA += words_in_block;

                /* Update the metadata for the slot. */
                block->valid = 1;
                block->dirty = 0;
                block->tag   = tag;
            }
        } else {
            /* ------------------- WRITE ------------------- */
            CPUW++;

            if (block->valid && block->tag == tag) {
                /* Branch WH: write hit. */
                NCWH++;

                if (is_write_back) {
                    /* WAWB: don't touch memory now, just remember that
                     * this slot has been modified.  Memory will be
                     * updated when this block is later evicted. */
                    block->dirty = 1;
                } else {
                    /* WAWT: write the word straight through to memory
                     * (1 word) so that memory and cache stay
                     * consistent.  There is no dirty bit. */
                    NWA++;
                }
            } else {
                /* Branch WM: write miss.  Both policies are
                 * write-allocate, so we still bring the block into
                 * the cache before performing the write. */
                NCWM++;

                /* WAWB: write back the previous occupant if it was
                 * dirty (same reasoning as the read-miss case). */
                if (is_write_back && block->valid && block->dirty) {
                    NWA += words_in_block;
                }

                /* Allocate the missing block: load words_in_block
                 * words from memory. */
                NRA += words_in_block;

                block->valid = 1;
                block->tag   = tag;

                if (is_write_back) {
                    /* The block has just been written to, so it is
                     * dirty straight away. */
                    block->dirty = 1;
                } else {
                    /* WAWT: also write the just-written word through
                     * to memory.  The block stays clean (there is no
                     * dirty bit, but we keep the field zeroed). */
                    block->dirty = 0;
                    NWA++;
                }
            }
        }
    }

    fclose(trace_fp);

    /* --------------------------------------------------------------
     * Step 7 - emit the result line in exactly the format required by
     * the brief.  Twelve fields, space-separated, all numeric values
     * formatted as decimal integers, finishing with the trace filename
     * and the write policy string.
     * -------------------------------------------------------------- */
    printf("%d %d %d %d %d %d %d %d %d %d %s %s\n",
           CPUR, CPUW, NRA, NWA, NCRH, NCRM, NCWH, NCWM,
           words_in_block, blocks_in_cache,
           trace_filename, write_policy);

    free(cache);
    return EXIT_SUCCESS;
}
