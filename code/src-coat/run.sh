#!/bin/bash

# Original code commented out
# TRACES=("lbm" "libq" "bzip2")
# for i in "${TRACES[@]}"; do
#     for j in "${TRACES[@]}"; do
#         if [[ "$i" > "$j" ]]; then
#             continue
#         fi
#         echo "Running $i + $j..."
#         ./sim -mode 4 "../traces/$i.mtr.gz" "../traces/$j.mtr.gz" > "../ref/results/00.norm.$i.$j"
#     done
# done

# Define trace files
TRACES=("ligra.BFS.com.champsimtrace.xz" "ligra.PC.com.champsimtrace.xz" "ligra.TC.com.champsimtrace.xz")

# Run each trace pair with itself for each TLB type
for trace in "${TRACES[@]}"; do
    # Extract short name for results file
    short_name=$(echo $trace | cut -d'.' -f2)  # Gets BFS, PC, or TC
    
    # Run with Base TLB (mode 4)
    echo "Running Base TLB: $short_name + $short_name..."
    ./sim -mode 4 "../traces/$trace" "../traces/$trace" > "../ref/results/03.coat.$short_name.$short_name"
    
done

echo "All combinations completed!"