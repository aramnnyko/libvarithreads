#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

# Сиды, которые будем сравнивать
SEEDS=(1 2 42 1337)
RUNS=5
SEQ=$(seq 2000 25 5000)
BIN=./sim-nostat

for seed in "${SEEDS[@]}"; do
    echo "=========================================="
    echo ">>> Запуск Seed $seed..."
    echo "=========================================="
    CSV="timings_seed_${seed}.csv"
    echo "cycles,median_s,min_s,max_s,us_per_cycle" > "$CSV"

    for n in $SEQ; do
        times=()
        for ((i=1; i<=RUNS; i++)); do
            t=$(taskset -c 0 $BIN $seed "$n" | awk '/Elapsed/{print $3}')
            times+=("$t")
        done
        min=$(printf '%s\n' "${times[@]}" | sort -g | head -1)
        max=$(printf '%s\n' "${times[@]}" | sort -g | tail -1)
        med=$(printf '%s\n' "${times[@]}" | sort -g | awk -v n="$RUNS" 'NR==int((n+1)/2){print}')
        uspc=$(awk -v t="$med" -v n="$n" 'BEGIN{printf "%.2f", t*1e6/n}')
        echo "$n,$med,$min,$max,$uspc" >> "$CSV"
    done
done

# Генерируем gnuplot-скрипт
cat << 'GNUPLOT' > plot_seeds.gnuplot
set encoding utf8
set datafile separator ","
set datafile columnheaders

set terminal pngcairo size 1400,600 enhanced font "DejaVu Sans,11"
set output "sim_timings_seeds.png"

set logscale x
set xlabel "cycles"
set ylabel "µs per cycle"
set grid xtics ytics mxtics mytics
set title "Сравнение разных сидов: зум 2000–5000 (шаг 25)"
set key top right box opaque

plot "timings_seed_1.csv" using 1:5 with linespoints pt 7 ps 0.9 lw 1.5 \
        lc rgb "#1f77b4" title "Seed 1", \
     "timings_seed_2.csv" using 1:5 with linespoints pt 5 ps 0.9 lw 1.5 \
        lc rgb "#ff7f0e" title "Seed 2", \
     "timings_seed_42.csv" using 1:5 with linespoints pt 9 ps 0.9 lw 1.5 \
        lc rgb "#2ca02c" title "Seed 42", \
     "timings_seed_1337.csv" using 1:5 with linespoints pt 11 ps 0.9 lw 1.5 \
        lc rgb "#d62728" title "Seed 1337"
GNUPLOT

echo "=========================================="
echo ">>> Построение графика..."
echo "=========================================="
gnuplot plot_seeds.gnuplot

echo "Готово! Смотрите файл: sim_timings_seeds.png"
