#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

RUNS="${1:-5}"; shift || true
SIZES=("$@")
[ ${#SIZES[@]} -eq 0 ] && SIZES=(200 2000 20000 200000)

BIN=./sim-nostat
SEED=1
CSV=timings.csv
echo "cycles,median_ns,min_ns,max_ns,median_steps" > "$CSV"

declare -a MED MIN MAX STEPS
for n in "${SIZES[@]}"; do
    times=()
    steps_arr=()
    for ((i=1; i<=RUNS; i++)); do
        out=$(taskset -c 0 $BIN $SEED "$n")
        t=$(echo "$out" | awk '/Elapsed/{print $3}')
        s=$(echo "$out" | awk '/Total steps/{print $3}')
        times+=("$t")
        steps_arr+=("$s")
    done
    
    min_t=$(printf '%s\n' "${times[@]}" | sort -g | head -1)
    max_t=$(printf '%s\n' "${times[@]}" | sort -g | tail -1)
    med_t=$(printf '%s\n' "${times[@]}" | sort -g | awk -v n="$RUNS" 'NR==int((n+1)/2){print}')
    med_s=$(printf '%s\n' "${steps_arr[@]}" | sort -g | awk -v n="$RUNS" 'NR==int((n+1)/2){print}')
    
    # Считаем наносекунды на шаг (время в секундах делим на шаги и умножаем на 1e9)
    min_ns=$(awk -v t="$min_t" -v s="$med_s" 'BEGIN{printf "%.2f", (t * 1e9) / s}')
    max_ns=$(awk -v t="$max_t" -v s="$med_s" 'BEGIN{printf "%.2f", (t * 1e9) / s}')
    med_ns=$(awk -v t="$med_t" -v s="$med_s" 'BEGIN{printf "%.2f", (t * 1e9) / s}')
    
    echo "$n,$med_ns,$min_ns,$max_ns,$med_s" >> "$CSV"
    MED+=("$med_ns"); MIN+=("$min_ns"); MAX+=("$max_ns"); STEPS+=("$med_s")
    
    printf "  [%2d/%2d] %8s: %8s ns/step  (шагов: %s)\n" \
        "${#MED[@]}" "${#SIZES[@]}" "$n" "$med_ns" "$med_s" >&2
done

echo
echo "=== Время на один шаг (ns/step, median) ==="
max_ns_val=$(printf '%s\n' "${MED[@]}" | sort -g | tail -1)
for i in "${!SIZES[@]}"; do
    # Для наглядности берем базу 0, шкала от 0 до max+5
    bar=$(awk -v v="${MED[$i]}" -v m="$max_ns_val" 'BEGIN{printf "%d", (v/m)*50+1}')
    printf "%8s | " "${SIZES[$i]}"
    printf '█%.0s' $(seq 1 "$bar")
    printf " %s ns/step\n" "${MED[$i]}"
done

echo
echo "=== Сводная таблица ==="
printf "%8s  %10s  %10s  %10s  %12s\n" \
       "cycles" "min_ns" "median_ns" "max_ns" "total_steps"
for i in "${!SIZES[@]}"; do
    printf "%8s  %10s  %10s  %10s  %12s\n" \
        "${SIZES[$i]}" "${MIN[$i]}" "${MED[$i]}" "${MAX[$i]}" "${STEPS[$i]}"
done

echo
echo "CSV сохранён: $CSV"