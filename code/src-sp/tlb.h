#ifndef TLB_H
#define TLB_H

#include <stdint.h>
#include <stdbool.h>
#include "cache.h"  // For MAX_WAYS_PER_CACHE_SET

// TLB Result Enum
typedef enum TLBResultEnum {
    TLB_HIT = 1,    // The access hit the TLB
    TLB_MISS = 0,   // The access missed the TLB
} TLBResult;

// TLB Entry Structure
typedef struct {
    bool valid;               // Valid bit
    bool dirty;               // Dirty bit
    uint64_t tag;            // Tag part of VPN
    uint64_t pfn;            // Physical Frame Number
    unsigned int core_id;     // Core ID this entry belongs to
    uint64_t last_access;    // For LRU replacement
} TLBEntry;

// TLB Set Structure
typedef struct {
    TLBEntry ways[MAX_WAYS_PER_CACHE_SET];  // Using same max ways as cache
} TLBSet;

// TLB Structure
typedef struct {
    uint64_t num_sets;           // Number of sets in the TLB
    uint64_t num_ways;           // Number of ways in each set
    uint64_t page_size;          // Page size in bytes
    bool shared;                 // Whether TLB is shared between cores
    uint64_t hit_latency;        // TLB hit latency in cycles
    uint64_t miss_latency;       // TLB miss latency in cycles
    
    // Tag and index calculation
    uint64_t index_bits;         // Number of bits used for index
    uint64_t index_mask;         // Mask to extract index from VPN
    
    TLBSet *sets;                // The TLB sets
    
    // Statistics
    uint64_t stat_read_access;   // Number of read accesses
    uint64_t stat_read_miss;     // Number of read misses
    uint64_t stat_write_access;  // Number of write accesses
    uint64_t stat_write_miss;    // Number of write misses
} TLB;

// Function declarations
TLB* tlb_new(uint64_t num_entries, uint64_t associativity, uint64_t page_size, bool shared);
void tlb_free(TLB *tlb);
TLBResult tlb_access(TLB *tlb, uint64_t vpn, uint64_t *pfn, bool is_write, unsigned int core_id);
void tlb_install(TLB *tlb, uint64_t vpn, uint64_t pfn, bool is_write, unsigned int core_id);
void tlb_print_stats(TLB *tlb, const char *header);

#endif // TLB_H 