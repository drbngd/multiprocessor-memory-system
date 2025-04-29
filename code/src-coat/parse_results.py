#!/usr/bin/env python3

import csv
import os
import re

# Constants
RESULTS_DIR = "../ref/results"
OUTPUT_CSV = "../ref/results/parsed_results.csv"

def extract_value(file_path, pattern):
    """Extract a value from a results file using regex pattern."""
    if not os.path.exists(file_path):
        return None
    
    with open(file_path, 'r') as f:
        content = f.read()
        match = re.search(f"{pattern}\s*:\s*(\S+)", content)
        return match.group(1) if match else None

def calculate_hit_rate(accesses, misses):
    """Calculate hit rate from accesses and misses."""
    if not accesses or not misses or accesses == "0":
        return "0.000"
    try:
        return f"{(int(accesses) - int(misses)) / int(accesses):.3f}"
    except (ValueError, ZeroDivisionError):
        return "0.000"

def get_results_file(tlb_type, workload1, workload2):
    """Get the results file path based on TLB type and workloads."""
    prefix = {
        "Base TLB": "01.norm",
        "TLBcoat": "03.coat"
    }.get(tlb_type)
    
    if not prefix:
        return None
    
    return os.path.join(RESULTS_DIR, f"{prefix}.{workload1}.{workload2}")

def main():
    # Define workloads
    workloads = ["libq", "lbm", "bzip2"]
    workload_combinations = []
    for w1 in workloads:
        for w2 in workloads:
            workload_combinations.append(f"{w1}+{w2}")
    
    # Define TLB types to process
    tlb_types = ["Base TLB", "TLBcoat"]
    
    # Create header
    header = ["WORKLOADS", "TLB TYPE", "CORE_0_IPC", "CORE_1_IPC", 
              "DTLB_READ_ACCESSES", "DTLB_READ_MISSES", "DTLB_READ_HIT_RATE",
              "DTLB_WRITE_ACCESSES", "DTLB_WRITE_MISSES", "DTLB_WRITE_HIT_RATE"]
    
    # Create rows
    rows = []
    for workload in workload_combinations:
        for tlb_type in tlb_types:
            workload1, workload2 = workload.split('+')
            results_file = get_results_file(tlb_type, workload1, workload2)
            
            # Initialize row with workload and TLB type
            row = [workload, tlb_type] + [""] * 8  # 8 empty fields for metrics
            
            if results_file and os.path.exists(results_file):
                # Extract values
                core0_ipc = extract_value(results_file, "CORE_0_IPC")
                core1_ipc = extract_value(results_file, "CORE_1_IPC")
                dtlb_rd_accesses = extract_value(results_file, "DTLB_READ_ACCESSES")
                dtlb_rd_misses = extract_value(results_file, "DTLB_READ_MISSES")
                dtlb_wr_accesses = extract_value(results_file, "DTLB_WRITE_ACCESSES")
                dtlb_wr_misses = extract_value(results_file, "DTLB_WRITE_MISSES")
                
                # Calculate hit rates
                dtlb_rd_hit_rate = calculate_hit_rate(dtlb_rd_accesses, dtlb_rd_misses)
                dtlb_wr_hit_rate = calculate_hit_rate(dtlb_wr_accesses, dtlb_wr_misses)
                
                # Update row with values
                if core0_ipc: row[2] = core0_ipc
                if core1_ipc: row[3] = core1_ipc
                if dtlb_rd_accesses: row[4] = dtlb_rd_accesses
                if dtlb_rd_misses: row[5] = dtlb_rd_misses
                if dtlb_rd_hit_rate: row[6] = dtlb_rd_hit_rate
                if dtlb_wr_accesses: row[7] = dtlb_wr_accesses
                if dtlb_wr_misses: row[8] = dtlb_wr_misses
                if dtlb_wr_hit_rate: row[9] = dtlb_wr_hit_rate
            else:
                print(f"Warning: {results_file} does not exist, creating empty row...")
            
            rows.append(row)
    
    # Write to new CSV file
    with open(OUTPUT_CSV, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)
    
    print(f"Results written to {OUTPUT_CSV}")

if __name__ == "__main__":
    main() 