/*
 * cache_sim.c
 *
 * Direct-mapped cache simulator implementing the algorithm described in the
 * project pseudo code. The simulator processes a memory-trace file, where each
 * non-empty / non-comment line has the form:
 *
 *     <op> <address> [<data>]
 *
 * with:
 *     op       -> 'R' (read) or 'W' (write)
 *     address  -> memory address (decimal, or hex if prefixed with 0x)
 *     data     -> optional, ignored by this simulator
 *
 * Lines that are blank or that start with '#' (after optional leading
 * whitespace) are ignored.
 *
 * The program reports the following counters once the trace is consumed:
 *
 *     CPUR  -> total CPU read requests
 *     CPUW  -> total CPU write requests
 *     NCRH  -> number of cache read hits
 *     NCRM  -> number of cache read misses
 *     NCWH  -> number of cache write hits
 *     NCWM  -> number of cache write misses
 *     NRA   -> number of words read from main memory
 *     NWA   -> number of words written to main memory
 *
 * The cache geometry (total size, block size, word size and address width)
 * is configurable from the command line; sensible defaults are provided.
 *
 * Build:
 *     gcc -O2 -Wall -Wextra -o cache_sim cache_sim.c -lm
 *
 * Usage:
 *     ./cache_sim <trace_file> [cache_size_bytes] [block_size_bytes]
 *                              [word_size_bytes]  [address_bits]
 *
 * Example:
 *     ./cache_sim trace.txt 1024 32 4 32
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int valid;
    uint64_t tag;
} cache_line_t;

static int is_power_of_two(uint64_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static int log2_u64(uint64_t x) {
    int n = 0;
    while (x > 1) {
        x >>= 1;
        n++;
    }
    return n;
}

static int line_is_blank_or_comment(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == '\0' || *s == '#';
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s <trace_file> [cache_size_bytes] "
                "[block_size_bytes] [word_size_bytes] [address_bits]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *trace_path     = argv[1];
    uint64_t cache_size_bytes  = (argc > 2) ? strtoull(argv[2], NULL, 0) : 1024;
    uint64_t block_size_bytes  = (argc > 3) ? strtoull(argv[3], NULL, 0) : 32;
    uint64_t word_size_bytes   = (argc > 4) ? strtoull(argv[4], NULL, 0) : 4;
    int      address_bits      = (argc > 5) ? (int)strtol(argv[5], NULL, 0) : 32;

    if (!is_power_of_two(cache_size_bytes) ||
        !is_power_of_two(block_size_bytes) ||
        !is_power_of_two(word_size_bytes)) {
        fprintf(stderr,
                "Error: cache size, block size and word size must all be "
                "powers of two.\n");
        return EXIT_FAILURE;
    }
    if (block_size_bytes < word_size_bytes) {
        fprintf(stderr,
                "Error: block size (%" PRIu64 ") must be >= word size "
                "(%" PRIu64 ").\n",
                block_size_bytes, word_size_bytes);
        return EXIT_FAILURE;
    }
    if (cache_size_bytes < block_size_bytes) {
        fprintf(stderr,
                "Error: cache size (%" PRIu64 ") must be >= block size "
                "(%" PRIu64 ").\n",
                cache_size_bytes, block_size_bytes);
        return EXIT_FAILURE;
    }

    uint64_t num_blocks      = cache_size_bytes / block_size_bytes;
    uint64_t words_in_block  = block_size_bytes / word_size_bytes;
    int offset_bits          = log2_u64(block_size_bytes);
    int index_bits           = log2_u64(num_blocks);
    int tag_bits             = address_bits - offset_bits - index_bits;

    if (tag_bits <= 0) {
        fprintf(stderr,
                "Error: address_bits (%d) is too small for the chosen "
                "cache geometry.\n",
                address_bits);
        return EXIT_FAILURE;
    }

    cache_line_t *cache = calloc((size_t)num_blocks, sizeof(cache_line_t));
    if (!cache) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(trace_path, "r");
    if (!fp) {
        perror(trace_path);
        free(cache);
        return EXIT_FAILURE;
    }

    uint64_t CPUR = 0, CPUW = 0;
    uint64_t NCRH = 0, NCRM = 0;
    uint64_t NCWH = 0, NCWM = 0;
    uint64_t NRA  = 0, NWA  = 0;

    char line[512];
    unsigned long line_no = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        line_no++;

        if (line_is_blank_or_comment(line)) continue;

        char op_str[16];
        char addr_str[64];
        int  parsed = sscanf(line, "%15s %63s", op_str, addr_str);
        if (parsed < 2) {
            fprintf(stderr,
                    "Warning: malformed trace line %lu (skipping): %s",
                    line_no, line);
            continue;
        }

        char op = (char)toupper((unsigned char)op_str[0]);
        if (op != 'R' && op != 'W') {
            fprintf(stderr,
                    "Warning: unknown operation '%s' on line %lu (skipping)\n",
                    op_str, line_no);
            continue;
        }

        uint64_t address = strtoull(addr_str, NULL, 0);

        uint64_t block_offset = address & ((UINT64_C(1) << offset_bits) - 1);
        uint64_t cache_index  = (address >> offset_bits) &
                                ((UINT64_C(1) << index_bits) - 1);
        uint64_t tag          = address >> (offset_bits + index_bits);
        (void)block_offset;

        cache_line_t *line_ptr = &cache[cache_index];

        if (op == 'R') {
            CPUR++;
            if (line_ptr->valid && line_ptr->tag == tag) {
                NCRH++;
            } else {
                NCRM++;
                NRA += words_in_block;
                line_ptr->valid = 1;
                line_ptr->tag   = tag;
            }
        } else { /* op == 'W' */
            CPUW++;
            if (line_ptr->valid && line_ptr->tag == tag) {
                NCWH++;
                NWA++;
            } else {
                NCWM++;
                NRA += words_in_block;
                line_ptr->valid = 1;
                line_ptr->tag   = tag;
                NWA++;
            }
        }
    }

    if (ferror(fp)) {
        perror("fread");
        fclose(fp);
        free(cache);
        return EXIT_FAILURE;
    }
    fclose(fp);

    uint64_t total_refs = CPUR + CPUW;
    uint64_t total_hits = NCRH + NCWH;
    uint64_t total_miss = NCRM + NCWM;

    printf("Cache configuration\n");
    printf("  cache size       : %" PRIu64 " bytes\n", cache_size_bytes);
    printf("  block size       : %" PRIu64 " bytes\n", block_size_bytes);
    printf("  word size        : %" PRIu64 " bytes\n", word_size_bytes);
    printf("  number of blocks : %" PRIu64 "\n",       num_blocks);
    printf("  words per block  : %" PRIu64 "\n",       words_in_block);
    printf("  address bits     : %d\n",                address_bits);
    printf("  tag/index/offset : %d / %d / %d\n",
           tag_bits, index_bits, offset_bits);
    printf("\n");

    printf("Simulation results\n");
    printf("  CPUR (CPU reads)        : %" PRIu64 "\n", CPUR);
    printf("  CPUW (CPU writes)       : %" PRIu64 "\n", CPUW);
    printf("  NCRH (read  hits)       : %" PRIu64 "\n", NCRH);
    printf("  NCRM (read  misses)     : %" PRIu64 "\n", NCRM);
    printf("  NCWH (write hits)       : %" PRIu64 "\n", NCWH);
    printf("  NCWM (write misses)     : %" PRIu64 "\n", NCWM);
    printf("  NRA  (words read  mem)  : %" PRIu64 "\n", NRA);
    printf("  NWA  (words written mem): %" PRIu64 "\n", NWA);

    if (total_refs > 0) {
        double hit_rate  = 100.0 * (double)total_hits / (double)total_refs;
        double miss_rate = 100.0 * (double)total_miss / (double)total_refs;
        printf("\n");
        printf("  total references  : %" PRIu64 "\n", total_refs);
        printf("  overall hit  rate : %.4f%%\n", hit_rate);
        printf("  overall miss rate : %.4f%%\n", miss_rate);
    }

    free(cache);
    return EXIT_SUCCESS;
}
