#include "tlb.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// External variable for current cycle
extern uint64_t current_cycle;

// Helper function to calculate number of index bits
static uint64_t log2(uint64_t n) {
    uint64_t bits = 0;
    while (n >>= 1) bits++;
    return bits;
}

TLB* tlb_new(uint64_t num_entries, uint64_t associativity, uint64_t page_size, bool shared) {
    TLB *tlb = (TLB*)calloc(1, sizeof(TLB));
    
    tlb->num_sets = num_entries;
    tlb->num_ways = associativity;
    tlb->page_size = page_size;
    tlb->shared = shared;
    tlb->hit_latency = 0;  // Default hit latency
    tlb->miss_latency = 10; // Default miss latency
    
    // Calculate index bits and mask
    tlb->index_bits = log2(tlb->num_sets);
    tlb->index_mask = (1ULL << tlb->index_bits) - 1;
    
    tlb->sets = (TLBSet*)calloc(tlb->num_sets, sizeof(TLBSet));
    
    return tlb;
}

void tlb_free(TLB *tlb) {
    if (tlb) {
        free(tlb->sets);
        free(tlb);
    }
}

// Helper function to find a TLB entry in a set
static TLBEntry* find_entry_in_set(TLBSet *set, uint64_t num_ways, uint64_t tag, unsigned int core_id) {
    uint64_t start_way = (core_id == 0) ? 0 : (num_ways / 2);
    uint64_t end_way = (core_id == 0) ? (num_ways / 2) : num_ways;
    
    for (uint64_t i = start_way; i < end_way; i++) {
        if (set->ways[i].valid && 
            set->ways[i].tag == tag && 
            set->ways[i].core_id == core_id) {
            return &set->ways[i];
        }
    }
    return NULL;
}

// Helper function to find victim in a set
static TLBEntry* find_victim_in_set(TLBSet *set, uint64_t num_ways, unsigned int core_id) {
    uint64_t start_way = (core_id == 0) ? 0 : (num_ways / 2);
    uint64_t end_way = (core_id == 0) ? (num_ways / 2) : num_ways;
    
    TLBEntry *victim = &set->ways[start_way];
    
    // First try to find an invalid entry
    for (uint64_t i = start_way; i < end_way; i++) {
        if (!set->ways[i].valid) {
            return &set->ways[i];
        }
    }
    
    // If no invalid entry, find LRU
    for (uint64_t i = start_way + 1; i < end_way; i++) {
        if (set->ways[i].last_access < victim->last_access) {
            victim = &set->ways[i];
        }
    }
    return victim;
}

TLBResult tlb_access(TLB *tlb, uint64_t vpn, uint64_t *pfn, bool is_write, unsigned int core_id) {
    // Update statistics
    if (is_write) {
        tlb->stat_write_access++;
    } else {
        tlb->stat_read_access++;
    }
    
    // Extract index and tag from VPN
    uint64_t index = vpn & tlb->index_mask;
    uint64_t tag = vpn >> tlb->index_bits;
    
    TLBSet *set = &tlb->sets[index];
    TLBEntry *entry = find_entry_in_set(set, tlb->num_ways, tag, core_id);
    
    if (entry) {
        // Update LRU
        entry->last_access = current_cycle;
        *pfn = entry->pfn;
        return TLB_HIT;
    }
    
    // Update miss statistics
    if (is_write) {
        tlb->stat_write_miss++;
    } else {
        tlb->stat_read_miss++;
    }
    
    return TLB_MISS;
}

void tlb_install(TLB *tlb, uint64_t vpn, uint64_t pfn, bool is_write, unsigned int core_id) {
    // Extract index and tag from VPN
    uint64_t index = vpn & tlb->index_mask;
    uint64_t tag = vpn >> tlb->index_bits;
    
    TLBSet *set = &tlb->sets[index];
    TLBEntry *entry = find_entry_in_set(set, tlb->num_ways, tag, core_id);
    
    if (!entry) {
        entry = find_victim_in_set(set, tlb->num_ways, core_id);
    }
    
    entry->valid = true;
    entry->dirty = is_write;
    entry->tag = tag;
    entry->pfn = pfn;
    entry->core_id = core_id;
    entry->last_access = current_cycle;
}

void tlb_print_stats(TLB *tlb, const char *header) {
    double read_hit_rate = 0.0;
    double write_hit_rate = 0.0;
    double overall_hit_rate = 0.0;
    
    if (tlb->stat_read_access > 0) {
        read_hit_rate = (double)(tlb->stat_read_access - tlb->stat_read_miss) / 
                       (double)tlb->stat_read_access;
    }
    
    if (tlb->stat_write_access > 0) {
        write_hit_rate = (double)(tlb->stat_write_access - tlb->stat_write_miss) / 
                        (double)tlb->stat_write_access;
    }
    
    // Calculate overall hit rate
    uint64_t total_access = tlb->stat_read_access + tlb->stat_write_access;
    uint64_t total_miss = tlb->stat_read_miss + tlb->stat_write_miss;
    if (total_access > 0) {
        overall_hit_rate = (double)(total_access - total_miss) / (double)total_access;
    }
    
    printf("\n%s_READ_ACCESSES  \t : %10llu\n", header, tlb->stat_read_access);
    printf("%s_READ_MISSES    \t : %10llu\n", header, tlb->stat_read_miss);
    printf("%s_READ_HIT_RATE  \t : %10.3f\n", header, read_hit_rate);
    printf("%s_WRITE_ACCESSES \t : %10llu\n", header, tlb->stat_write_access);
    printf("%s_WRITE_MISSES   \t : %10llu\n", header, tlb->stat_write_miss);
    printf("%s_WRITE_HIT_RATE \t : %10.3f\n", header, write_hit_rate);
    printf("%s_OVERALL_HIT_RATE \t : %10.3f\n", header, overall_hit_rate);
} 