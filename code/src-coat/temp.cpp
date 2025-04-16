#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern uint64_t current_cycle;

Cache *cache_new(uint64_t size, uint64_t associativity, uint64_t line_size, ReplacementPolicy replacement_policy)
{
    Cache *cache = (Cache *)calloc(1, sizeof(Cache));
    cache->num_sets = size / (associativity * line_size);
    cache->num_ways = associativity;
    cache->replacement_policy = replacement_policy;
    cache->sets = (CacheSet *)calloc(cache->num_sets, sizeof(CacheSet));

    return cache;
}

CacheResult cache_access(Cache *c, uint64_t line_addr, bool is_write, unsigned int core_id)
{
    uint64_t set_index = line_addr % c->num_sets;
    uint64_t tag = line_addr / c->num_sets;
    CacheSet *set = &c->sets[set_index];

    c->stat_read_access += !is_write;
    c->stat_write_access += is_write;

    auto it = set->line_map.find(tag);
    if (it != set->line_map.end())
    {
        CacheLine *line = it->second;
        line->last_access_time = current_cycle;
        if (is_write)
        {
            line->dirty = true;
        }
        // Move the accessed line to the head of the list
        if (line != set->head)
        {
            if (line->prev) line->prev->next = line->next;
            if (line->next) line->next->prev = line->prev;
            if (line == set->tail) set->tail = line->prev;
            line->next = set->head;
            line->prev = nullptr;
            if (set->head) set->head->prev = line;
            set->head = line;
        }
        return HIT;
    }

    if (is_write)
    {
        c->stat_write_miss++;
    }
    else
    {
        c->stat_read_miss++;
    }

    cache_install(c, line_addr, is_write, core_id);
    return MISS;
}

void cache_install(Cache *c, uint64_t line_addr, bool is_write, unsigned int core_id)
{
    uint64_t set_index = line_addr % c->num_sets;
    uint64_t tag = line_addr / c->num_sets;
    CacheSet *set = &c->sets[set_index];

    unsigned int victim_index = cache_find_victim(c, set_index, core_id);
    CacheLine *victim = set->head;
    for (unsigned int i = 0; i < victim_index; i++)
    {
        victim = victim->next;
    }

    if (victim->valid && victim->dirty)
    {
        c->stat_dirty_evicts++;
    }

    c->last_evicted_line = *victim;

    set->line_map.erase(victim->tag);
    victim->valid = true;
    victim->dirty = is_write;
    victim->tag = tag;
    victim->core_id = core_id;
    victim->last_access_time = current_cycle;
    set->line_map[tag] = victim;

    // Move the installed line to the head of the list
    if (victim != set->head)
    {
        if (victim->prev) victim->prev->next = victim->next;
        if (victim->next) victim->next->prev = victim->prev;
        if (victim == set->tail) set->tail = victim->prev;
        victim->next = set->head;
        victim->prev = nullptr;
        if (set->head) set->head->prev = victim;
        set->head = victim;
    }
}

unsigned int cache_find_victim(Cache *c, unsigned int set_index, unsigned int core_id)
{
    CacheSet *set = &c->sets[set_index];
    unsigned int victim_index = 0;

    if (c->replacement_policy == LRU)
    {
        CacheLine *line = set->tail;
        while (line && line->valid)
        {
            line = line->prev;
            victim_index++;
        }
    }
    else if (c->replacement_policy == RANDOM)
    {
        victim_index = rand() % c->num_ways;
    }

    return victim_index;
}