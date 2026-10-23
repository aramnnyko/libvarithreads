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
