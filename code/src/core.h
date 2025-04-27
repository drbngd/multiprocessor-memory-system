///////////////////////////////////////////////////////////////////////////////
// You shouldn't need to modify this file.                                   //
///////////////////////////////////////////////////////////////////////////////

// core.h
// Declares a CPU core structure and related functions.

#ifndef __CORE_H__
#define __CORE_H__

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "memsys.h"
#include <sys/types.h>

// ===== TRACE HANDLING CHANGES START =====
// Added for ChampSim trace support
#define INST_TYPE_LOAD  0
#define INST_TYPE_STORE 1
#define INST_TYPE_OTHER 2

#define ACCESS_TYPE_IFETCH 0
#define ACCESS_TYPE_LOAD   1
#define ACCESS_TYPE_STORE  2
// ===== TRACE HANDLING CHANGES END =====

typedef struct MemorySystem MemorySystem;

typedef struct {
    unsigned int core_id;
    MemorySystem *memsys;
    
    // ===== TRACE HANDLING CHANGES START =====
    // Modified for ChampSim trace support
    bool is_champsim;  // Flag to indicate ChampSim trace format
    // ===== TRACE HANDLING CHANGES END =====
    
    // Trace reading
    int trace_fd;
    pid_t pid;
    uint8_t read_buf[4096];
    int read_buf_offset;
    int read_buf_left;
    
    // Current instruction
    // ===== TRACE HANDLING CHANGES START =====
    // Changed to 64-bit for ChampSim trace support
    uint64_t trace_inst_addr;  // 64-bit instruction address
    uint8_t trace_inst_type;
    uint64_t trace_ldst_addr;  // 64-bit load/store address
    // ===== TRACE HANDLING CHANGES END =====
    
    // Statistics
    uint64_t inst_count;
    uint64_t done_inst_count;
    uint64_t done_cycle_count;
    uint64_t snooze_end_cycle;
    bool done;
} Core;

// Function declarations
Core* core_new(MemorySystem *memsys, const char *trace_filename, unsigned int core_id);
void core_cycle(Core *core);
void core_print_stats(Core *core);
void core_read_trace(Core *core);

#endif // CORE_H
