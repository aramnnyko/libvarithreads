# ./plot_zoom.gnuplot timings_zoom_25.csv
set encoding utf8
set datafile separator ","
set datafile columnheaders

set terminal pngcairo size 1400,500 enhanced font "DejaVu Sans,11"
set output "sim_timings_zoom.png"

set logscale x
set xlabel "cycles"
set ylabel "µs per cycle"
set grid xtics ytics mxtics mytics
set title "Зум: переход 2000–5000, шаг 25 (10 прогонов, median)"
set key top right box opaque
set yrange [29.5:35.5]

# асимптота линейного режима
set arrow from 2000,32.0 to 5000,32.0 nohead dt 2 lw 1 lc rgb "gray50"

plot "timings_zoom_25.csv" using 1:5 with linespoints pt 7 ps 0.9 lw 1.5 \
        lc rgb "#d62728" title "median µs/cycle", \
     "" using 1:5:($3/$1*1e6):($4/$1*1e6) with yerrorbars pt 0 ps 0 \
        lw 1 lc rgb "#d62728" notitle

unset output