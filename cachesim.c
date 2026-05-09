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
 * Description
 * -----------
 * Simulates a direct-mapped cache controller for a processor with a 32-bit
 * address bus and a 16-bit data bus.  The cache controller can be configured
 * for two write policies:
 *
 *   WAWB - Write-Allocate / Write-Back
 *   WAWT - Write-Allocate / Write-Through
 *
 * Allowed cache geometry (powers of two):
 *
 *   blocks_in_cache : 2 .. 512
 *   words_in_block  : 2 .. 256
 *
 * Build (ANSI C17):
 *   gcc -std=c17 -Wall -Wextra -o cachesim cachesim.c
 *
 * Usage:
 *   cachesim trace_filename write_policy blocks_in_cache words_in_block
 *
 * Output (single line, decimal integers, space separated):
 *   CPUR CPUW NRA NWA NCRH NCRM NCWH NCWM WIB BIC filename WP
 *
 * Software flowchart implemented by this simulator
 * ------------------------------------------------
 *   For each non-comment, non-blank line of the trace file:
 *
 *     read op, address, data
 *     index  = (address >> offset_bits) & (blocks_in_cache - 1)
 *     tag    =  address >> (offset_bits + index_bits)
 *
 *     if op == 'R':
 *         CPUR++
 *         if valid[index] and tag[index] == tag:        -> NCRH++       (RH)
 *         else:
 *             NCRM++
 *             if WAWB and valid[index] and dirty[index]:
 *                 NWA += words_in_block                 (write back)
 *             NRA += words_in_block                     (load block)
 *             valid[index] = 1; dirty[index] = 0; tag[index] = tag
 *
 *     if op == 'W':
 *         CPUW++
 *         if valid[index] and tag[index] == tag:        -> NCWH++
 *             if WAWB:  dirty[index] = 1
 *             else:     NWA++                           (write through)
 *         else:
 *             NCWM++
 *             if WAWB and valid[index] and dirty[index]:
 *                 NWA += words_in_block                 (write back)
 *             NRA += words_in_block                     (write-allocate)
 *             valid[index] = 1; tag[index] = tag
 *             if WAWB:  dirty[index] = 1
 *             else:     dirty[index] = 0; NWA++         (write through)
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

#define ADDRESS_BITS         32
#define MIN_BLOCKS_IN_CACHE   2
#define MAX_BLOCKS_IN_CACHE 512
#define MIN_WORDS_IN_BLOCK    2
#define MAX_WORDS_IN_BLOCK  256
#define MAX_TRACE_LINE_LEN  256

struct cache_line {
    int      valid;
    int      dirty;
    uint32_t tag;
};

static int is_power_of_two(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

static int log2_int(int n)
{
    int bits = 0;
    while (n > 1) {
        n >>= 1;
        bits++;
    }
    return bits;
}

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

    if (strcmp(write_policy, "WAWB") != 0 &&
        strcmp(write_policy, "WAWT") != 0) {
        fprintf(stderr,
                "Error: write_policy must be 'WAWB' or 'WAWT'.\n");
        return EXIT_FAILURE;
    }
    int is_write_back = (strcmp(write_policy, "WAWB") == 0);

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

    int offset_bits = log2_int(words_in_block);
    int index_bits  = log2_int(blocks_in_cache);
    if (offset_bits + index_bits >= ADDRESS_BITS) {
        fprintf(stderr,
                "Error: chosen cache geometry leaves no tag bits.\n");
        return EXIT_FAILURE;
    }
    uint32_t index_mask = (uint32_t)(blocks_in_cache - 1);

    struct cache_line *cache =
        calloc((size_t)blocks_in_cache, sizeof(struct cache_line));
    if (cache == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        return EXIT_FAILURE;
    }

    FILE *trace_fp = fopen(trace_filename, "r");
    if (trace_fp == NULL) {
        fprintf(stderr,
                "Error: cannot open trace file '%s'.\n",
                trace_filename);
        free(cache);
        return EXIT_FAILURE;
    }

    int CPUR = 0, CPUW = 0;
    int NRA  = 0, NWA  = 0;
    int NCRH = 0, NCRM = 0;
    int NCWH = 0, NCWM = 0;

    char line[MAX_TRACE_LINE_LEN];

    while (fgets(line, sizeof(line), trace_fp) != NULL) {

        char op = line[0];

        if (op == '!')                continue;
        if (line_is_blank(line))      continue;
        if (op != 'R' && op != 'W')   continue;

        unsigned long address_in = 0;
        unsigned long data_in    = 0;
        if (sscanf(line + 1, "%lx %lx", &address_in, &data_in) < 1) {
            continue;
        }
        (void)data_in;

        uint32_t address     = (uint32_t)address_in;
        uint32_t cache_index = (address >> offset_bits) & index_mask;
        uint32_t tag         = address >> (offset_bits + index_bits);

        struct cache_line *block = &cache[cache_index];

        if (op == 'R') {
            CPUR++;

            if (block->valid && block->tag == tag) {
                NCRH++;
            } else {
                NCRM++;
                if (is_write_back && block->valid && block->dirty) {
                    NWA += words_in_block;
                }
                NRA += words_in_block;
                block->valid = 1;
                block->dirty = 0;
                block->tag   = tag;
            }
        } else {
            CPUW++;

            if (block->valid && block->tag == tag) {
                NCWH++;
                if (is_write_back) {
                    block->dirty = 1;
                } else {
                    NWA++;
                }
            } else {
                NCWM++;
                if (is_write_back && block->valid && block->dirty) {
                    NWA += words_in_block;
                }
                NRA += words_in_block;
                block->valid = 1;
                block->tag   = tag;
                if (is_write_back) {
                    block->dirty = 1;
                } else {
                    block->dirty = 0;
                    NWA++;
                }
            }
        }
    }

    fclose(trace_fp);

    printf("%d %d %d %d %d %d %d %d %d %d %s %s\n",
           CPUR, CPUW, NRA, NWA, NCRH, NCRM, NCWH, NCWM,
           words_in_block, blocks_in_cache,
           trace_filename, write_policy);

    free(cache);
    return EXIT_SUCCESS;
}
