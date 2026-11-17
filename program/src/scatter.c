/*
 * scatter.c - Independent random stores (store-queue pressure test)
 * ============================================================================
 *
 * PURPOSE
 *   Answers the proposal's second question for the store queue: does the SQ
 *   bind before the ROB? A store holds its SQ entry until it has committed
 *   and been written to the cache, so a store that misses can occupy an entry
 *   for a full memory latency. With independent random stores, SQ entries can
 *   fill while the ROB still has room, which makes the SQ (not the ROB) the
 *   first structure to stall rename. This test measures where that happens.
 *
 * WHAT IT DOES
 *   Each iteration stores to a random index in an array much larger than the
 *   L2. The index comes from a generator that never depends on a loaded
 *   value, and the stored value is just the loop counter, so no store waits on
 *   another. -fill inserts independent ALU work after each store, setting the
 *   instruction distance between store misses and therefore how many fit in
 *   the SQ at once.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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


// Program Under Test 

static inline __attribute__((always_inline))
uint64_t test(size_t ws_size, long iters)
{
    uint64_t *const d = data;            /* locals: stores to d[] must not   */
    const size_t mask = data_mask;       /* force reloads of these globals   */
    const uint64_t fl = (uint64_t)g_fill;

    for (long n = 0; n < iters; n++) {
        d[random() & mask] = (uint64_t)n;
        fill(fl);
    }
    return d[0];
}

void setup(size_t bytes)
{
    n_data = 1;                       /* power of two <= bytes/8 */
    while (n_data * 2 * 8 <= bytes) n_data *= 2;
    data_mask = n_data - 1;
    data = aligned_alloc(LINE, n_data * 8);
    for (size_t i = 0; i < n_data; i++) 
        data[i] = random();
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

    srandom(12345);  // Seed the Random Number Generator
    setup(mb << 20); 
    uint64_t result = test(mb, iters);

    printf("checksum=%llu\n", (unsigned long long)result);

    return 0; 

}