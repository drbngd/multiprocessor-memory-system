#!/bin/bash

# Define trace files
TRACES=("lbm" "libq" "bzip2")

# Create all combinations
for i in "${TRACES[@]}"; do
    for j in "${TRACES[@]}"; do
        # Skip if first trace is lexicographically greater than second
        if [[ "$i" > "$j" ]]; then
            continue
        fi
        
        # Run the simulation
        echo "Running $i + $j..."
        ./sim -mode 4 "../traces/$i.mtr.gz" "../traces/$j.mtr.gz" > "../ref/results/00.norm.$i.$j"
    done
done

echo "All combinations completed!"