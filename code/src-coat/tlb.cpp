#include "tlb.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// External variable for current cycle
extern uint64_t current_cycle;

// PRINCE cipher constants and functions
#ifdef TLBCOAT
static const uint8_t PRINCE_SBOX[16] = {0xB, 0xF, 0x3, 0x2, 0xA, 0xC, 0x9, 0x1, 0x6, 0x7, 0x8, 0x0, 0xE, 0x5, 0xD, 0x4};
// not used, its needed for decryption
//static const uint8_t PRINCE_SBOX_INV[16] = {0xB, 0x7, 0x3, 0x2, 0xF, 0xD, 0x8, 0x9, 0xA, 0x6, 0x4, 0x0, 0x5, 0xE, 0xC, 0x1};
static const uint32_t PRINCE_M0[16] = {
    0x0111, 0x2220, 0x4404, 0x8088,
    0x1011, 0x0222, 0x4440, 0x8808,
    0x1101, 0x2022, 0x0444, 0x8880,
    0x1110, 0x2202, 0x4044, 0x0888
};
static const uint32_t PRINCE_M1[16] = {
    0x1110, 0x2202, 0x4044, 0x0888,
    0x0111, 0x2220, 0x4404, 0x8088,
    0x1011, 0x0222, 0x4440, 0x8808,
    0x1101, 0x2022, 0x0444, 0x8880
};

// PRINCE cipher helper functions
static uint64_t prince_gf2_mul_16(uint64_t in, const uint32_t mat[16]) {
    uint64_t out = 0;
    for(uint8_t i = 0; i < 16; i++) {
        if((in >> i) & 1) {
            out ^= mat[i];
        }
    }
    return out;
}

static void prince_s_layer(uint64_t* block) {
    uint64_t out = 0;
    for(uint8_t i = 15; i > 0; --i) {
        out |= PRINCE_SBOX[(*block >> (i << 2)) & 0xF];
        out <<= 4;
    }
    out |= PRINCE_SBOX[*block & 0xF];
    *block = out;
}

static void prince_m_prime_layer(uint64_t* block) {
    const uint64_t out_0 = prince_gf2_mul_16(*block, PRINCE_M0);
    const uint64_t out_1 = prince_gf2_mul_16(*block >> 16, PRINCE_M1);
    const uint64_t out_2 = prince_gf2_mul_16(*block >> 32, PRINCE_M1);
    const uint64_t out_3 = prince_gf2_mul_16(*block >> 48, PRINCE_M0);
    *block = (out_3 << 48) | (out_2 << 32) | (out_1 << 16) | out_0;
}

uint64_t tlb_prince_encrypt(uint64_t input, uint64_t key) {
    uint64_t output = input;
    
    // Round 1
    output ^= key;
    output ^= 0x13198a2e03707344ULL;
    prince_m_prime_layer(&output);
    prince_s_layer(&output);
    
    // Round 2
    output ^= key;
    output ^= 0xa4093822299f31d0ULL;
    prince_m_prime_layer(&output);
    prince_s_layer(&output);
    
    output ^= key;
    
    // Round 3
    prince_s_layer(&output);
    prince_m_prime_layer(&output);
    
    return output;
}

void tlb_randomize(TLB *tlb, uint64_t vpn, unsigned int core_id, uint64_t *set_indices) {
    assert(core_id < tlb->num_cores);
    // Use core_id directly as process identifier for randomization
    uint64_t randomization = tlb_prince_encrypt(vpn, 
        tlb->prince_key ^ core_id ^ tlb->core_states[core_id].rid);
    
    for(uint64_t i = 0; i < tlb->num_ways; i++) {
        set_indices[i] = (randomization >> (i*4)) & (tlb->num_sets - 1);
    }
}

void tlb_set_miss_threshold(TLB *tlb, unsigned int core_id, uint64_t threshold) {
    assert(core_id < tlb->num_cores);
    tlb->core_states[core_id].miss_threshold = threshold;
}

void tlb_update_rplru(TLB *tlb, uint64_t set_index, uint64_t way_index) {
    TLBSet *set = &tlb->sets[set_index];
    
    // If entry is already MRU (state 1), no need to update
    if (set->ways[way_index].rplru_state == 1) return;
    
    // Update states
    uint8_t old_state = set->ways[way_index].rplru_state;
    set->ways[way_index].rplru_state = 1;  // Make it MRU
    
    // Update other entries
    for (uint64_t i = 0; i < tlb->num_ways; i++) {
        if (i == way_index) continue;
        
        // If the entry's state was more recent than the one we're updating,
        // make it one step older
        if (set->ways[i].rplru_state <= old_state) {
            set->ways[i].rplru_state++;
        }
    }
}
#endif

// Helper function to calculate number of index bits
static uint64_t log2(uint64_t n) {
    uint64_t bits = 0;
    while (n >>= 1) bits++;
    return bits;
}

TLB* tlb_new(uint64_t num_entries, uint64_t associativity, uint64_t page_size, bool shared, uint64_t num_cores) {
    TLB *tlb = (TLB*)calloc(1, sizeof(TLB));
    
    tlb->num_sets = num_entries / associativity;
    tlb->num_ways = associativity;
    tlb->page_size = page_size;
    tlb->shared = shared;
    tlb->hit_latency = 1;  // Default hit latency
    tlb->miss_latency = 10; // Default miss latency
    tlb->num_cores = num_cores;
    
    // Calculate index bits and mask
    tlb->index_bits = log2(tlb->num_sets);
    tlb->index_mask = (1ULL << tlb->index_bits) - 1;
    
    tlb->sets = (TLBSet*)calloc(tlb->num_sets, sizeof(TLBSet));
    
    #ifdef TLBCOAT
    // Initialize TLBcoat specific fields
    tlb->prince_key = 0x0011223344556677ULL;  // Default key
    tlb->core_states = (TLBcoatCoreState*)calloc(num_cores, sizeof(TLBcoatCoreState));
    
    // Initialize per-core state
    for (uint64_t i = 0; i < num_cores; i++) {
        tlb->core_states[i].miss_threshold = tlb->num_sets;  // Set threshold to number of sets, generic trend from the paper
        tlb->core_states[i].miss_counter = 0;
        tlb->core_states[i].rid = 0;
    }
    
    // Initialize RPLRU states - state ranges from 1 (MRU) to num_ways (LRU)
    for (uint64_t i = 0; i < tlb->num_sets; i++) {
        for (uint64_t j = 0; j < tlb->num_ways; j++) {
            tlb->sets[i].ways[j].rplru_state = tlb->num_ways;  // Initialize as LRU
        }
    }
    #endif
    
    return tlb;
}

void tlb_free(TLB *tlb) {
    if (tlb) {
        #ifdef TLBCOAT
        free(tlb->core_states);
        #endif
        free(tlb->sets);
        free(tlb);
    }
}

/* // Helper function to find a TLB entry in a set
static TLBEntry* find_entry_in_set(TLBSet *set, uint64_t num_ways, uint64_t tag, unsigned int core_id) {
    for (uint64_t i = 0; i < num_ways; i++) {
        if (set->ways[i].valid && 
            set->ways[i].tag == tag && 
            set->ways[i].core_id == core_id) {
            return &set->ways[i];
        }
    }
    return NULL;
}

// Helper function to find victim in a set
static TLBEntry* find_victim_in_set(TLBSet *set, uint64_t num_ways) {
    TLBEntry *victim = &set->ways[0];
    
    // First try to find an invalid entry
    for (uint64_t i = 0; i < num_ways; i++) {
        if (!set->ways[i].valid) {
            return &set->ways[i];
        }
    }
    
    // If no invalid entry, find LRU
    for (uint64_t i = 1; i < num_ways; i++) {
        if (set->ways[i].last_access < victim->last_access) {
            victim = &set->ways[i];
        }
    }
    return victim;
} */

TLBResult tlb_access(TLB *tlb, uint64_t vpn, uint64_t *pfn, bool is_write, unsigned int core_id) {
    assert(core_id < tlb->num_cores);
    
    // Update statistics
    if (is_write) {
        tlb->stat_write_access++;
    } else {
        tlb->stat_read_access++;
    }
    
    #ifdef TLBCOAT
    uint64_t set_indices[MAX_WAYS_PER_CACHE_SET];
    tlb_randomize(tlb, vpn, core_id, set_indices);
    
    // Check all possible ways
    for (uint64_t i = 0; i < tlb->num_ways; i++) {
        TLBSet *set = &tlb->sets[set_indices[i]];
        if (set->ways[i].valid && set->ways[i].tag == vpn && 
            (!tlb->shared || set->ways[i].core_id == core_id)) {
            // Hit
            tlb_update_rplru(tlb, set_indices[i], i);
            *pfn = set->ways[i].pfn;
            return TLB_HIT;
        }
    }
    
    // Miss - update counter and check for re-randomization
    tlb->core_states[core_id].miss_counter++;
    if (tlb->core_states[core_id].miss_counter >= tlb->core_states[core_id].miss_threshold) {
        tlb->core_states[core_id].rid++;  // Simple increment for simulation
        tlb->core_states[core_id].miss_counter = 0;
        tlb->stat_rerand_count++;
    }
    
    #else
    // Original set-associative TLB logic
    uint64_t index = vpn & tlb->index_mask;
    uint64_t tag = vpn >> tlb->index_bits;
    
    TLBSet *set = &tlb->sets[index];
    TLBEntry *entry = find_entry_in_set(set, tlb->num_ways, tag, core_id);
    
    if (entry) {
        entry->last_access = current_cycle;
        *pfn = entry->pfn;
        return TLB_HIT;
    }
    #endif
    
    // Update miss statistics
    if (is_write) {
        tlb->stat_write_miss++;
    } else {
        tlb->stat_read_miss++;
    }
    
    return TLB_MISS;
}

void tlb_install(TLB *tlb, uint64_t vpn, uint64_t pfn, bool is_write, unsigned int core_id) {
    assert(core_id < tlb->num_cores);
    
    #ifdef TLBCOAT
    uint64_t set_indices[MAX_WAYS_PER_CACHE_SET];
    tlb_randomize(tlb, vpn, core_id, set_indices);
    
    // First try to find an invalid entry
    int64_t victim_way = -1;
    for (uint64_t i = 0; i < tlb->num_ways; i++) {
        if (!tlb->sets[set_indices[i]].ways[i].valid) {
            victim_way = i;
            break;
        }
    }
    
    // If no invalid entry, find LRU
    if (victim_way == -1) {
        for (uint64_t i = 0; i < tlb->num_ways; i++) {
            if (tlb->sets[set_indices[i]].ways[i].rplru_state == tlb->num_ways) {  // Check for LRU state
                victim_way = i;
                break;
            }
        }
    }
    
    // Install entry
    TLBEntry *entry = &tlb->sets[set_indices[victim_way]].ways[victim_way];
    entry->valid = true;
    entry->dirty = is_write;
    entry->tag = vpn;
    entry->pfn = pfn;
    entry->core_id = core_id;
    tlb_update_rplru(tlb, set_indices[victim_way], victim_way);
    
    #else
    // Original set-associative TLB logic
    uint64_t index = vpn & tlb->index_mask;
    uint64_t tag = vpn >> tlb->index_bits;
    
    TLBSet *set = &tlb->sets[index];
    TLBEntry *entry = find_entry_in_set(set, tlb->num_ways, tag, core_id);
    
    if (!entry) {
        entry = find_victim_in_set(set, tlb->num_ways);
    }
    
    entry->valid = true;
    entry->dirty = is_write;
    entry->tag = tag;
    entry->pfn = pfn;
    entry->core_id = core_id;
    entry->last_access = current_cycle;
    #endif
}

void tlb_print_stats(TLB *tlb, const char *header) {
    double read_hit_rate = 0.0;
    double write_hit_rate = 0.0;
    
    if (tlb->stat_read_access > 0) {
        read_hit_rate = (double)(tlb->stat_read_access - tlb->stat_read_miss) / 
                       (double)tlb->stat_read_access;
    }
    
    if (tlb->stat_write_access > 0) {
        write_hit_rate = (double)(tlb->stat_write_access - tlb->stat_write_miss) / 
                        (double)tlb->stat_write_access;
    }
    
    printf("\n%s_READ_ACCESSES  \t : %10llu\n", header, tlb->stat_read_access);
    printf("%s_READ_MISSES    \t : %10llu\n", header, tlb->stat_read_miss);
    printf("%s_READ_HIT_RATE  \t : %10.3f\n", header, read_hit_rate);
    printf("%s_WRITE_ACCESSES \t : %10llu\n", header, tlb->stat_write_access);
    printf("%s_WRITE_MISSES   \t : %10llu\n", header, tlb->stat_write_miss);
    printf("%s_WRITE_HIT_RATE \t : %10.3f\n", header, write_hit_rate);
    #ifdef TLBCOAT
    printf("%s_RERAND_COUNT   \t : %10llu\n", header, tlb->stat_rerand_count);
    #endif
} 