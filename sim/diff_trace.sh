#!/usr/bin/env bash
# diff_trace.sh <out.txt> <trace.csv>
#
# 1. Из статистики (out.txt) берутся строки потоков с Diff != 0
# 2. По их id из trace.csv вытаскиваются все строки
# 3. Печатается: сводка по потокам, потом трейс для каждого id

set -euo pipefail

OUT="${1:?usage: $0 <out.txt> <trace.csv>}"
TRACE="${2:?usage: $0 <out.txt> <trace.csv>}"

# ---------------------------------------------------------------------------
# 1. Из out.txt — строки потоков с Diff != 0. Сохраняем id.
# ---------------------------------------------------------------------------
awk 'NF == 13 && $1 ~ /^[0-9]+$/ && $NF != 0 { print $1 }' "$OUT" \
    | sort -n > /tmp/dt_ids.txt

N=$(wc -l < /tmp/dt_ids.txt)

echo "##############"
echo "# STAT rows  #"
echo "##############"
echo

awk 'NF == 13 && $1 ~ /^[0-9]+$/ && $NF != 0' "$OUT"

echo
echo "##############"
echo "# TRACE rows #"
echo "##############"
echo

if [[ "$N" -eq 0 ]]; then
    echo "(no interesting ids)"
    exit 0
fi

# ---------------------------------------------------------------------------
# 2. Из trace.csv — строки для этих id, отсортированные по id, потом по cycle,R
# ---------------------------------------------------------------------------
awk -F, '
    NR==FNR { want[$1]=1; next }
    ($4 in want) { print }
' /tmp/dt_ids.txt "$TRACE" \
    | sort -t, -k4,4n -k1,1n -k2,2n