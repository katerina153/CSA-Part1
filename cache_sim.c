/*
 * cache_sim.c
 *
 * Simple direct-mapped cache simulator.
 * Reads a trace file where each line is:  <op> <address> <data>
 * with op = R or W. Blank lines and lines starting with '#' are ignored.
 */

#include <stdio.h>
#include <string.h>

#define CACHE_SIZE      1024    /* total cache size in bytes */
#define BLOCK_SIZE      32      /* block size in bytes       */
#define WORD_SIZE       4       /* word size in bytes        */

#define NUM_BLOCKS      (CACHE_SIZE / BLOCK_SIZE)   /* 32 */
#define WORDS_IN_BLOCK  (BLOCK_SIZE / WORD_SIZE)    /*  8 */
#define OFFSET_BITS     5       /* log2(BLOCK_SIZE) */
#define INDEX_BITS      5       /* log2(NUM_BLOCKS) */

int main(void) {
    int valid[NUM_BLOCKS];
    unsigned int tag_arr[NUM_BLOCKS];

    int CPUR = 0, CPUW = 0;
    int NCRH = 0, NCRM = 0;
    int NCWH = 0, NCWM = 0;
    int NRA  = 0, NWA  = 0;

    for (int i = 0; i < NUM_BLOCKS; i++) {
        valid[i]   = 0;
        tag_arr[i] = 0;
    }

    FILE *fp = fopen("trace.txt", "r");
    if (fp == NULL) {
        printf("Could not open trace.txt\n");
        return 1;
    }

    char line[200];

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (line[0] == '\n' || line[0] == '#') continue;

        char op;
        unsigned int address;
        int data;

        if (sscanf(line, " %c %x %d", &op, &address, &data) < 2) continue;

        unsigned int cache_index = (address >> OFFSET_BITS) & (NUM_BLOCKS - 1);
        unsigned int tag         = address >> (OFFSET_BITS + INDEX_BITS);

        if (op == 'R') {
            CPUR++;
            if (valid[cache_index] == 1 && tag_arr[cache_index] == tag) {
                NCRH++;
            } else {
                NCRM++;
                NRA += WORDS_IN_BLOCK;
                valid[cache_index]   = 1;
                tag_arr[cache_index] = tag;
            }
        }
        else if (op == 'W') {
            CPUW++;
            if (valid[cache_index] == 1 && tag_arr[cache_index] == tag) {
                NCWH++;
                NWA++;
            } else {
                NCWM++;
                NRA += WORDS_IN_BLOCK;
                valid[cache_index]   = 1;
                tag_arr[cache_index] = tag;
                NWA++;
            }
        }
    }

    fclose(fp);

    printf("CPUR = %d\n", CPUR);
    printf("CPUW = %d\n", CPUW);
    printf("NCRH = %d\n", NCRH);
    printf("NCRM = %d\n", NCRM);
    printf("NCWH = %d\n", NCWH);
    printf("NCWM = %d\n", NCWM);
    printf("NRA  = %d\n", NRA);
    printf("NWA  = %d\n", NWA);

    return 0;
}
