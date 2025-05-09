///////////////////////////////////////////////////////////////////////////////
// You will need to modify this file to implement parts B and C.             //
///////////////////////////////////////////////////////////////////////////////

// dram.cpp
// Defines the functions used to implement DRAM.

#include "dram.h"
#include <stdio.h>
#include <stdlib.h>
// You may add any other #include directives you need here, but make sure they
// compile on the reference machine!

///////////////////////////////////////////////////////////////////////////////
//                                 CONSTANTS                                 //
///////////////////////////////////////////////////////////////////////////////

/** The fixed latency of a DRAM access assumed in part B, in cycles. */
#define DELAY_SIM_MODE_B 100

/** The DRAM activation latency (ACT), in cycles. (Also known as RAS.) */
#define DELAY_ACT 45

/** The DRAM column selection latency (CAS), in cycles. */
#define DELAY_CAS 45

/** The DRAM precharge latency (PRE), in cycles. */
#define DELAY_PRE 45

/**
 * The DRAM bus latency, in cycles.
 * 
 * This is how long it takes for the DRAM to transmit the data via the bus. It
 * is incurred on every DRAM access. (In part B, assume that this is included
 * in the fixed DRAM latency DELAY_SIM_MODE_B.)
 */
#define DELAY_BUS 10

/** The row buffer size, in bytes. */
#define ROW_BUFFER_SIZE 1024

/** The number of banks in the DRAM module. */
#define NUM_BANKS 16

///////////////////////////////////////////////////////////////////////////////
//                    EXTERNALLY DEFINED GLOBAL VARIABLES                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * The current mode under which the simulation is running, corresponding to
 * which part of the lab is being evaluated.
 */
extern Mode SIM_MODE;

/** The number of bytes in a cache line. */
extern uint64_t CACHE_LINESIZE;

/** Which page policy the DRAM should use. */
extern DRAMPolicy DRAM_PAGE_POLICY;

///////////////////////////////////////////////////////////////////////////////
//                           FUNCTION DEFINITIONS                            //
///////////////////////////////////////////////////////////////////////////////

// As described in dram.h, you are free to deviate from the suggested
// implementation as you see fit.

// The only restriction is that you must not remove dram_print_stats() or
// modify its output format, since its output will be used for grading.

/**
 * Allocate and initialize a DRAM module.
 * 
 * This is intended to be implemented in part B.
 * 
 * @return A pointer to the DRAM module.
 */
DRAM *dram_new()
{
    // TODO: Allocate memory to the data structures and initialize the required
    //       fields. (You might want to use calloc() for this.)

    DRAM *dram = (DRAM *)calloc(1, sizeof(DRAM));

    return dram; // to suppress warning
}

/**
 * Access the DRAM at the given cache line address.
 * 
 * Return the delay in cycles incurred by this DRAM access. Also update the
 * DRAM statistics accordingly.
 * 
 * Note that the address is given in units of the cache line size!
 * 
 * This is intended to be implemented in parts B and C. In parts C through F,
 * you may delegate logic to the dram_access_mode_CDEF() functions.
 * 
 * @param dram The DRAM module to access.
 * @param line_addr The address of the cache line to access (in units of the
 *                  cache line size).
 * @param is_dram_write Whether this access writes to DRAM.
 * @return The delay in cycles incurred by this DRAM access.
 */
uint64_t dram_access(DRAM *dram, uint64_t line_addr, bool is_dram_write)
{
    // TODO: Update the appropriate DRAM statistics.
    // TODO: Call the dram_access_mode_CDEF() function as needed.
    // TODO: Return the delay in cycles incurred by this DRAM access.


    if (SIM_MODE ==  SIM_MODE_B) {
        if (is_dram_write)
        {
            dram->stat_write_access += 1;
            dram->stat_write_delay += DELAY_SIM_MODE_B;
        } else {
            dram->stat_read_access += 1;
            dram->stat_read_delay += DELAY_SIM_MODE_B;
        }

        return DELAY_SIM_MODE_B;
    }

    /* writing code for parts CDEF */
    return dram_access_mode_CDEF(dram, line_addr, is_dram_write);


}

/**
 * For parts C through F, access the DRAM at the given cache line address.
 * 
 * Return the delay in cycles incurred by this DRAM access. It is intended that
 * the calling function will be responsible for updating DRAM statistics
 * accordingly.
 * 
 * Note that the address is given in units of the cache line size!
 * 
 * This is intended to be implemented in part C.
 * 
 * @param dram The DRAM module to access.
 * @param line_addr The address of the cache line to access (in units of the
 *                  cache line size).
 * @param is_dram_write Whether this access writes to DRAM.
 * @return The delay in cycles incurred by this DRAM access.
 */
uint64_t dram_access_mode_CDEF(DRAM *dram, uint64_t line_addr,
                               bool is_dram_write)
{
    // Assume a mapping with consecutive lines in the same row and consecutive
    // row buffers in consecutive rows.
    // TODO: Use this function to track open rows.
    // TODO: Compute the delay based on row buffer hit/miss/empty.


    if (DRAM_PAGE_POLICY == CLOSE_PAGE) {

        if (is_dram_write) {
            /* Timing: DELAY_ACT (activate the row) + DELAY_CAS (column access) + DELAY_BUS (data transfer). */
            dram->stat_write_access += 1;
            dram->stat_write_delay += DELAY_ACT + DELAY_CAS + DELAY_BUS;
        } else {
            /* Timing: DELAY_ACT (activate the row) + DELAY_CAS (column access) + DELAY_BUS (data transfer). */
            dram->stat_read_access += 1;
            dram->stat_read_delay += DELAY_ACT + DELAY_CAS + DELAY_BUS;
        }

        return DELAY_ACT + DELAY_CAS + DELAY_BUS;
    }

    int delay = 0;
    if (DRAM_PAGE_POLICY == OPEN_PAGE) {
        /* get physical addr, calculate bank index and row index */
        uint64_t physical_addr = line_addr * CACHE_LINESIZE;
        uint64_t bank = (physical_addr/ROW_BUFFER_SIZE) % NUM_BANKS;
        uint64_t row_index = (physical_addr/ROW_BUFFER_SIZE) / NUM_BANKS;

        /* get bool conditions for row hit/miss/empty */
        bool is_row_hit = dram->row_buffers[bank].valid && (dram->row_buffers[bank].row_id == row_index);
        bool is_row_miss = dram->row_buffers[bank].valid && (dram->row_buffers[bank].row_id != row_index);
        bool is_row_empty = !dram->row_buffers[bank].valid;


        if (is_row_hit) {
             /* Row hit: The desired row is already open in the row buffer.
              * Timing: DELAY_CAS (column access) + DELAY_BUS (data transfer). */
            delay = DELAY_CAS + DELAY_BUS;
        }

        if (is_row_empty) {
             /* Row empty: No row is currently open in the row buffer.
              * Timing: DELAY_ACT (row activation) + DELAY_CAS (column access) + DELAY_BUS (data transfer). */
            dram->row_buffers[bank].row_id = row_index;  // Activate the new row.
            dram->row_buffers[bank].valid = true;
            delay = DELAY_ACT + DELAY_CAS + DELAY_BUS;
        }

        if (is_row_miss) {
            /* Row miss: A different row is currently open in the row buffer.
             * Timing: DELAY_PRE (precharge current row) + DELAY_ACT (activate new row) +
             * DELAY_CAS (column access) + DELAY_BUS (data transfer). */
            /* add the recently accessed row to row-buffer */
            dram->row_buffers[bank].row_id = row_index;
            dram->row_buffers[bank].valid = true;
            delay = DELAY_PRE + DELAY_ACT + DELAY_CAS + DELAY_BUS;
        }

        if (is_dram_write) {
            dram->stat_write_access += 1;
            dram->stat_write_delay += delay;
        } else {
            dram->stat_read_access += 1;
            dram->stat_read_delay += delay;
        }

        return delay;


    }

    return 0;
}

/**
 * Print the statistics of the DRAM module.
 * 
 * This is implemented for you. You must not modify its output format.
 * 
 * @param dram The DRAM module to print the statistics of.
 */
void dram_print_stats(DRAM *dram)
{
    double avg_read_delay = 0.0;
    double avg_write_delay = 0.0;

    if (dram->stat_read_access)
    {
        avg_read_delay = (double)(dram->stat_read_delay) /
                         (double)(dram->stat_read_access);
    }

    if (dram->stat_write_access)
    {
        avg_write_delay = (double)(dram->stat_write_delay) /
                          (double)(dram->stat_write_access);
    }

    printf("\n");
    printf("DRAM_READ_ACCESS     \t\t : %10llu\n", dram->stat_read_access);
    printf("DRAM_WRITE_ACCESS    \t\t : %10llu\n", dram->stat_write_access);
    printf("DRAM_READ_DELAY_AVG  \t\t : %10.3f\n", avg_read_delay);
    printf("DRAM_WRITE_DELAY_AVG \t\t : %10.3f\n", avg_write_delay);
}
