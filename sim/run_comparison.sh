#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

RUNS=5
SEQ=$(seq 2000 25 5000)

# 1. Baseline (glibc malloc с tcache)
echo "=========================================="
echo ">>> [1/3] Запуск Baseline (обычный glibc)"
echo "=========================================="
./plot.sh $RUNS $SEQ > /dev/null
mv timings.csv timings_baseline.csv

# 2. Отключенный tcache
echo "=========================================="
echo ">>> [2/3] Запуск без tcache"
echo "=========================================="
GLIBC_TUNABLES=glibc.malloc.tcache_count=0 ./plot.sh $RUNS $SEQ > /dev/null
mv timings.csv timings_notcache.csv

# 3. jemalloc
JEMALLOC_LIB=$(ldconfig -p | grep libjemalloc.so.2 | awk '{print $NF}' | head -n 1)
if [ -z "$JEMALLOC_LIB" ]; then
    echo "ВНИМАНИЕ: jemalloc не найден в системе! Пропускаем шаг 3."
else
    echo "=========================================="
    echo ">>> [3/3] Запуск с jemalloc ($JEMALLOC_LIB)"
    echo "=========================================="
    LD_PRELOAD="$JEMALLOC_LIB" ./plot.sh $RUNS $SEQ > /dev/null
    mv timings.csv timings_jemalloc.csv
fi

# 4. Генерация gnuplot-скрипта для сравнения
cat << 'GNUPLOT' > plot_compare.gnuplot
set encoding utf8
set datafile separator ","
set datafile columnheaders

set terminal pngcairo size 1400,500 enhanced font "DejaVu Sans,11"
set output "sim_timings_compare.png"

set logscale x
set xlabel "cycles"
set ylabel "µs per cycle"
set grid xtics ytics mxtics mytics
set title "Сравнение аллокаторов: зум 2000–5000 (шаг 25, median)"
set key top right box opaque

plot "timings_baseline.csv" using 1:5 with linespoints pt 7 ps 0.9 lw 1.5 \
        lc rgb "#1f77b4" title "Baseline (glibc)", \
     "timings_notcache.csv" using 1:5 with linespoints pt 5 ps 0.9 lw 1.5 \
        lc rgb "#ff7f0e" title "Без tcache", \
     "timings_jemalloc.csv" using 1:5 with linespoints pt 9 ps 0.9 lw 1.5 \
        lc rgb "#2ca02c" title "jemalloc"
GNUPLOT

echo "=========================================="
echo ">>> Построение общего графика..."
echo "=========================================="
gnuplot plot_compare.gnuplot

echo "Готово! Смотрите файл: sim_timings_compare.png"
