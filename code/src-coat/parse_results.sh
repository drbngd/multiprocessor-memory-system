#!/bin/bash

# Output CSV file
CSV_FILE="../ref/results/data.csv"

# Function to extract a value from a results file
extract_value() {
    local file=$1
    local pattern=$2
    if [ -f "$file" ]; then
        grep "$pattern" "$file" | awk '{print $NF}'
    else
        echo ""
    fi
}

# Function to calculate hit rate
calculate_hit_rate() {
    local accesses=$1
    local misses=$2
    if [ -z "$accesses" ] || [ -z "$misses" ] || [ "$accesses" = "0" ]; then
        echo "0.000"
    else
        echo "scale=3; ($accesses - $misses) / $accesses" | bc
    fi
}

# Clear the CSV file and write header
echo "WORKLOADS,TLB TYPE,CORE_0_IPC,CORE_1_IPC,DTLB_RD_ACCESSES,DTLB_RD_MISSES,DTLB_RD_HIT_RATE,DTLB_WR_ACCESSES,DTLB_WR_MISSES,DTLB_WR_HIT_RATE" > "$CSV_FILE"

# Process each results file
for tlb_type in "Base TLB" "SP-TLB" "TLBcoat" "RF-TLB"; do
    for i in "lbm" "libq" "bzip2"; do
        for j in "lbm" "libq" "bzip2"; do
            if [[ "$i" > "$j" ]]; then
                continue
            fi
            
            # Construct results filename based on TLB type
            if [ "$tlb_type" = "Base TLB" ]; then
                results_file="../ref/results/01.norm.$i.$j"
            elif [ "$tlb_type" = "SP-TLB" ]; then
                results_file="../ref/results/02.sp.$i.$j"
            elif [ "$tlb_type" = "TLBcoat" ]; then
                results_file="../ref/results/03.coat.$i.$j"
            else
                results_file="../ref/results/04.rf.$i.$j"
            fi
            
            # Skip if file doesn't exist
            if [ ! -f "$results_file" ]; then
                echo "Warning: $results_file does not exist, skipping..."
                continue
            fi
            
            # Extract values
            core0_ipc=$(extract_value "$results_file" "CORE_0_IPC")
            core1_ipc=$(extract_value "$results_file" "CORE_1_IPC")
            dtlb_rd_accesses=$(extract_value "$results_file" "DTLB_READ_ACCESSES")
            dtlb_rd_misses=$(extract_value "$results_file" "DTLB_READ_MISSES")
            dtlb_wr_accesses=$(extract_value "$results_file" "DTLB_WRITE_ACCESSES")
            dtlb_wr_misses=$(extract_value "$results_file" "DTLB_WRITE_MISSES")
            
            # Calculate hit rates
            dtlb_rd_hit_rate=$(calculate_hit_rate "$dtlb_rd_accesses" "$dtlb_rd_misses")
            dtlb_wr_hit_rate=$(calculate_hit_rate "$dtlb_wr_accesses" "$dtlb_wr_misses")
            
            # Update CSV
            workload="$i+$j"
            echo "$workload,$tlb_type,$core0_ipc,$core1_ipc,$dtlb_rd_accesses,$dtlb_rd_misses,$dtlb_rd_hit_rate,$dtlb_wr_accesses,$dtlb_wr_misses,$dtlb_wr_hit_rate" >> "$CSV_FILE"
        done
    done
done

echo "CSV file updated successfully!" 