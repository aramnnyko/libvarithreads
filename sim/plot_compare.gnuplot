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
