/**
 * gather.c - Independent random loads (memory-level parallelism test)
 * ============================================================================
 *
 * PURPOSE
 *   The baseline window-size vs. memory-latency experiment. It answers the
 *   proposal's first two questions: what ROB size saturates IPC as memory
 *   latency changes, and which structure (ROB, IQ, LQ, MSHRs) binds first.
 *   Every access is an independent cache miss with no locality, so miss
 *   density is controlled exactly and nothing else in the program competes
 *   with the window effect being measured.
 *
 * WHAT IT DOES
 *   Each iteration loads one element from a random index in an array much
 *   larger than the L2 and adds it to a running sum. The index comes from a
 *   generator that never depends on a loaded value, so the core can compute
 *   addresses far ahead of the misses and overlap as many as the window and
 *   MSHRs allow. -fill inserts independent ALU work after each load; this
 *   sets the instruction distance between misses, and therefore how many
 *   misses fit in one ROB.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define LINE 64
#define MAXK 64

typedef struct { size_t next; char pad[LINE - sizeof(size_t)]; } node_t;

static uint64_t *data; 
static size_t n_data;
static size_t data_mask;
static int g_k = 4; 
static int g_fill = 0;

static inline __attribute__((always_inline))
void fill(uint64_t n)
{
    // Force Independent work that won't be optimised away
    // Four to Seven ALU ops to Total Instructions 
    uint64_t it = 0, it1 = 0, it2 = 0, it3 = 0;
    while (it++ < n)
    {
        it1 += 3; it2 += 4; it3 += 5;
        asm volatile("" : "+r"(it), "+r"(it1), "+r"(it2), "+r"(it3));
    }
}

static inline size_t rand_idx(uint64_t *g, size_t mask) {
    *g += 0x9E3779B97F4A7C15ULL;
    return (size_t)(((*g * 0xBF58476D1CE4E5B9ULL) >> 20) & mask);
}

static uint64_t g = 88172645463325252ULL;   // fixed seed
static inline uint64_t rnd(void) {
    g ^= g << 13; g ^= g >> 7; g ^= g << 17;
    return g;
}


// Program Under Test 

static inline __attribute__((always_inline))
uint64_t test(size_t ws_size, long iters)
{
    uint64_t *const d = data;            /* locals: stores to d[] must not   */
    const size_t mask = data_mask;       /* force reloads of these globals   */
    const uint64_t fl = (uint64_t)g_fill;

    uint64_t s = 0;
    for (long n = 0; n < iters; n++) {
        s += d[rand_idx(&g, mask)];
        fill(fl);
    }
    return s;
}

void setup(size_t bytes)
{
    n_data = 1;                       /* power of two <= bytes/8 */
    while (n_data * 2 * 8 <= bytes) n_data *= 2;
    data_mask = n_data - 1;
    data = aligned_alloc(LINE, n_data * 8);
    for (size_t i = 0; i < n_data; i++) 
        data[i] = rnd();
}

// ==================== // 

enum ARG_TYPE
{
    K_CHAINS,
    WORKING_SET_SIZE,
    ALU_OPS,
    ITERS_PER_RUN,

    // Size of Args 
    TOTAL_ARGS
};

char args [5][10] =\
{
    "-k",        // Number of independent pointer chains 
    "-mb",       // Working set size in MiB
    "-fill",     // Number of independent ALU ops for each memory op
    "-iters",    // Number of loop iterations to run 
};

int main(int argc, char **argv) {
    // Defaults 
    size_t mb = 16;
    long iters = 2000000;
     
    // Collect Args 
    for (int i = 1; i < argc -1; i++)
    {
        for(int j = 0; j < TOTAL_ARGS; j++)
            if (strcmp(argv[i], args[j]) == 0)
            {
                long value = atol(argv[i+1]);
                switch(j)
                {
                    case K_CHAINS:          g_k = value;    break;
                    case WORKING_SET_SIZE:  mb = value;     break;
                    case ALU_OPS:           g_fill = value; break;
                    case ITERS_PER_RUN:     iters = value;  break;
                }  
                i++; 
                break;
            }       
    }

    // Make sure pointer chains nums are reasonable
    if (g_k < 1) g_k = 1;
    if (g_k > MAXK) g_k = MAXK;

    setup(mb << 20); 
    uint64_t result = test(mb, iters);

    printf("checksum=%llu\n", (unsigned long long)result);

    return 0; 

}