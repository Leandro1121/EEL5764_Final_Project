/*
 * chase.c - K independent pointer chains (memory-level parallelism test)
 *
 * PURPOSE: Finds the window size where IPC saturates as memory latency and
 * MLP change. k=1 is a fully serial chase (MLP=1, lower bound); larger k gives
 * ~k outstanding misses, and -fill sets the instruction distance between them,
 * which decides how many misses fit in one ROB. At high k the L1D MSHRs should
 * bind before the ROB (watch dcache blockedCycles::no_mshrs).
 *
 * ARGS: -k {1,2,4,8,16}  -mb <MiB, keep >> 1 MiB L2>  -fill <n>  -iters <n>
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

static node_t *nodes;  
static size_t n_nodes;
static size_t chain_pos[MAXK];
static int g_k = 4; 
static int g_fill = 0;

static inline __attribute__((always_inline))
void fill(int n)
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
uint64_t test(const int K, long iters)
{
    node_t *const nd = nodes;     /* locals so globals aren't re-read */
    const int fl = g_fill;
    size_t p[MAXK];

    for (int j = 0; j < K; j++)
        p[j] = chain_pos[j];

    for (long n = 0; n < iters; n++) {
        #pragma GCC unroll 16
        for (int j = 0; j < K; j++)
            p[j] = nd[p[j]].next;     /* K independent misses */
        fill(fl);
    }

    uint64_t s = 0;
    for (int j = 0; j < K; j++)
        s += p[j];
    return s;
}

void setup(size_t bytes)
{
    n_nodes = bytes / LINE;
    nodes = aligned_alloc(LINE, n_nodes * sizeof(node_t));
    if (!nodes) 
    { 
        fprintf(stderr, "alloc failed\n"); 
        exit(1); 
    }
    for (size_t i = 0; i < n_nodes; i++) 
        nodes[i].next = i;
    /* Sattolo's shuffle: yields ONE random cycle (no short loops) */
    for (size_t i = n_nodes - 1; i > 0; i--) 
    {
        size_t j = (size_t) rnd() % i;
        size_t t = nodes[i].next; 
        nodes[i].next = nodes[j].next; 
        nodes[j].next = t;
    }
    
    /* Evenly spaced chain starts: chains stay far apart on the cycle, so one
     * chain never revisits lines another touched recently (no accidental hits) */
    size_t cur = 0, gap = n_nodes / MAXK;
    for (int j = 0; j < MAXK; j++) {
        chain_pos[j] = cur;
        for (size_t s = 0; s < gap; s++)
            cur = nodes[cur].next;
    }
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
    for (int i = 0; i < argc; i++)
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

            }       
    }

    // Make sure pointer chains nums are reasonable
    if (g_k < 1) g_k = 1;
    if (g_k > MAXK) g_k = MAXK;

    setup(mb << 20); 

    uint64_t res; 
    switch (g_k)
    {
        case 1:  res = test(1, iters);  break; 
        case 2:  res = test(2, iters);  break; 
        case 4:  res = test(4, iters);  break; 
        case 8:  res = test(8, iters);  break; 
        case 16: res = test(16, iters); break; 
        default:
            fprintf(stderr, "k must be 1, 2, 4, 8 or 16\n");
            exit(1);
    }

    printf("checksum=%llu\n", (unsigned long long)res);

    return 0; 

}