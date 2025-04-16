///////////////////////////////////////////////////////////////////////////////
// You will need to modify this file to implement part A and, for extra      //
// credit, parts E and F.                                                    //
///////////////////////////////////////////////////////////////////////////////

// cache.cpp
// Defines the functions used to implement the cache.

#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
// You may add any other #include directives you need here, but make sure they
// compile on the reference machine!

///////////////////////////////////////////////////////////////////////////////
//                    EXTERNALLY DEFINED GLOBAL VARIABLES                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * The current clock cycle number.
 *
 * This can be used as a timestamp for implementing the LRU replacement policy.
 */
extern uint64_t current_cycle;

/**
 * For static way partitioning, the quota of ways in each set that can be
 * assigned to core 0.
 *
 * The remaining number of ways is the quota for core 1.
 *
 * This is used to implement extra credit part E.
 */
extern unsigned int SWP_CORE0_WAYS;

///////////////////////////////////////////////////////////////////////////////
//                           FUNCTION DEFINITIONS                            //
///////////////////////////////////////////////////////////////////////////////

// As described in cache.h, you are free to deviate from the suggested
// implementation as you see fit.

// The only restriction is that you must not remove cache_print_stats() or
// modify its output format, since its output will be used for grading.

/**
 * Allocate and initialize a cache.
 *
 * This is intended to be implemented in part A.
 *
 * @param size The size of the cache in bytes.
 * @param associativity The associativity of the cache.
 * @param line_size The size of a cache line in bytes.
 * @param replacement_policy The replacement policy of the cache.
 * @return A pointer to the cache.
 */
Cache *cache_new(uint64_t size, uint64_t associativity, uint64_t line_size,
                 ReplacementPolicy replacement_policy)
{
    // TODO: Allocate memory to the data structures and initialize the required
    //       fields. (You might want to use calloc() for this.)


    Cache *cache = (Cache *)calloc(1, sizeof(Cache));
    if (!cache) {
        exit(1);
    }

    cache->size = size;

    cache->num_ways = associativity;

    cache->line_size = line_size;

    cache->num_sets = size / (associativity * line_size);

    cache->sets = (CacheSet *)calloc(cache->num_sets, sizeof(CacheSet));
    if (!cache->sets) {
        exit(1);
    }

    cache->replacement_policy = replacement_policy;


    return cache;
}

/**
 * Access the cache at the given address.
 *
 * Also update the cache statistics accordingly.
 *
 * This is intended to be implemented in part A.
 *
 * @param c The cache to access.
 * @param line_addr The address of the cache line to access (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @param is_write Whether this access is a write.
 * @param core_id The CPU core ID that requested this access.
 * @return Whether the cache access was a hit or a miss.
 */
CacheResult cache_access(Cache *c, uint64_t line_addr, bool is_write,
                         unsigned int core_id)
{
    // TODO: Return HIT if the access hits in the cache, and MISS otherwise.
    // TODO: If is_write is true, mark the resident line as dirty.
    // TODO: Update the appropriate cache statistics.


    /* calculate tag and set_index */
    unsigned int tag = line_addr / c->num_sets;
    unsigned int set_index = line_addr % c->num_sets;

//    printf("\t\tindex: %d, tag: %d, is_write: %d, core_id: %d\n", set_index, tag, is_write, core_id);

    /* index the cache set */
    CacheSet *set = &c->sets[set_index];

    /* update statistics */
    c->stat_write_access += is_write;
    c->stat_read_access += !is_write;

    /* check if in cache */
    for (unsigned int i = 0; i < c->num_ways; i++) {
        CacheLine *line = &set->ways[i];

        if (line == NULL) {
            continue;
        }

        if (line->tag == tag) {
            /* hit */
            if (is_write) { line->dirty = true; }
            line->last_access_time = current_cycle;
            // no need to install on write, as it is being done by in memsys.cpp
            return HIT;
        }
    }

    /* it's a MISS -> update miss stat */
    c->stat_write_miss += is_write;
    c->stat_read_miss += !is_write;

    return MISS;
}

/**
 * Install the cache line with the given address.
 *
 * Also update the cache statistics accordingly.
 *
 * This is intended to be implemented in part A.
 *
 * @param c The cache to install the line into.
 * @param line_addr The address of the cache line to install (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @param is_write Whether this install is triggered by a write.
 * @param core_id The CPU core ID that requested this access.
 */
void cache_install(Cache *c, uint64_t line_addr, bool is_write,
                   unsigned int core_id)
{
    // TODO: Use cache_find_victim() to determine the victim line to evict.
    // TODO: Copy it into a last_evicted_line field in the cache in order to
    //       track writebacks.
    // TODO: Initialize the victim entry with the line to install.
    // TODO: Update the appropriate cache statistics.

    /* calculate tag and set_index */
    unsigned int tag = line_addr / c->num_sets;
    unsigned int set_index = line_addr % c->num_sets;

    /* index the cache set */
    CacheSet *set = &c->sets[set_index];

    /* find victim */
    unsigned int victim_index = cache_find_victim(c, set_index, core_id);
    CacheLine *victim = &set->ways[victim_index];

    /* update statistics */
    if (victim->valid && victim->dirty) {
        c->stat_dirty_evicts++;
    }

    /* record last evicted line */
    c->last_evicted_line = *victim;

    /* install new line in cache */
    victim->valid = true;
    victim->dirty = is_write;
    victim->tag = tag;
    victim->line_addr = line_addr;
    victim->core_id = core_id;
    victim->last_access_time = current_cycle;

}

/**
 * Find which way in a given cache set to replace when a new cache line needs
 * to be installed. This should be chosen according to the cache's replacement
 * policy.
 *
 * The returned victim can be valid (non-empty), in which case the calling
 * function is responsible for evicting the cache line from that victim way.
 *
 * This is intended to be initially implemented in part A and, for extra
 * credit, extended in parts E and F.
 *
 * @param c The cache to search.
 * @param set_index The index of the cache set to search.
 * @param core_id The CPU core ID that requested this access.
 * @return The index of the victim way.
 */
unsigned int cache_find_victim(Cache *c, unsigned int set_index,
                               unsigned int core_id)
{
    // TODO: Find a victim way in the given cache set according to the cache's
    //       replacement policy.
    // TODO: In part A, implement the LRU and random replacement policies.
    // TODO: In part E, for extra credit, implement static way partitioning.
    // TODO: In part F, for extra credit, implement dynamic way partitioning.

    /* get num ways */
    unsigned int num_ways = c->num_ways;
    ReplacementPolicy  policy = c->replacement_policy;

    /* index the cache set */
    CacheSet *set = &c->sets[set_index];

    /* find victim */
    unsigned int victim_index = 0;
    switch (policy) {
        case LRU: {
            /* find the least-recently-used line in set */
            unsigned long long min_time = current_cycle;
            for (unsigned int i = 0; i < num_ways; i++) {

                /* line is invalid -> return the first invalid */
                if (!set->ways[i].valid) {
                    victim_index = i;
                    break;
                }

                /* if not invalid line found -> return LRU */
                if (set->ways[i].last_access_time < min_time) {
                    min_time = set->ways[i].last_access_time;
                    victim_index = i;
                }

            }
            break;
        }


        case RANDOM: {
            /* return a random line */
            victim_index = rand() % num_ways;
            break;
        }

        case SWP: break;

        case DWP: break;

    }



    return victim_index; // to suppress warning
}

/**
 * Print the statistics of the given cache.
 *
 * This is implemented for you. You must not modify its output format.
 *
 * @param c The cache to print the statistics of.
 * @param label A label for the cache, which is used as a prefix for each
 *              statistic.
 */
void cache_print_stats(Cache *c, const char *header)
{
    double read_miss_percent = 0.0;
    double write_miss_percent = 0.0;

    if (c->stat_read_access)
    {
        read_miss_percent = 100.0 * (double)(c->stat_read_miss) /
                            (double)(c->stat_read_access);
    }

    if (c->stat_write_access)
    {
        write_miss_percent = 100.0 * (double)(c->stat_write_miss) /
                             (double)(c->stat_write_access);
    }

    printf("\n");
    printf("%s_READ_ACCESS     \t\t : %10llu\n", header, c->stat_read_access);
    printf("%s_WRITE_ACCESS    \t\t : %10llu\n", header, c->stat_write_access);
    printf("%s_READ_MISS       \t\t : %10llu\n", header, c->stat_read_miss);
    printf("%s_WRITE_MISS      \t\t : %10llu\n", header, c->stat_write_miss);
    printf("%s_READ_MISS_PERC  \t\t : %10.3f\n", header, read_miss_percent);
    printf("%s_WRITE_MISS_PERC \t\t : %10.3f\n", header, write_miss_percent);
    printf("%s_DIRTY_EVICTS    \t\t : %10llu\n", header, c->stat_dirty_evicts);
}