# ./plot.gnuplot timings.csv
set encoding utf8
set datafile separator ","
set datafile columnheaders

set terminal pngcairo size 1400,600 enhanced font "DejaVu Sans,11"
set output "sim_timings.png"

set multiplot layout 1,2 title "sim-nostat: Время на 1 шаг симуляции (ns/step)"

# --- левый: ns/step от cycles (semilogx) ---
set logscale x
set xlabel "cycles"
set ylabel "ns / step"
set grid xtics ytics mxtics mytics
set key top left box opaque
set title "ns/step (чем ровнее, тем лучше)"

# Строим график с усами min/max
plot "timings.csv" using 1:2:3:4 with yerrorbars pt 7 ps 1.2 lw 1.5 lc rgb "#1f77b4" title "median (min–max)", \
     "timings.csv" using 1:2 with lines lw 2 lc rgb "#1f77b4" notitle

# --- правый: распределение шагов (log-y) ---
set logscale y
set xlabel "cycles"
set ylabel "Количество шагов (log)"
set title "Сколько шагов сделал симулятор"
set key top left box opaque

plot "timings.csv" using 1:5 with linespoints pt 7 ps 1.2 lw 2 lc rgb "#d62728" title "steps"