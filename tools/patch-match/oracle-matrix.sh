#!/usr/bin/env bash
# oracle-matrix.sh — 6 engines × 4 seeds self-recovery (the 24-run bar).
# Usage: bash tools/patch-match/oracle-matrix.sh [pm-binary] [label] [extra-pm-flags...]
# Writes tools/patch-match/results/<label>.txt and a one-line summary.
set -euo pipefail
cd "$(dirname "$0")/../.."
PM="${1:-build/pm}"
LABEL="${2:-run}"
shift 2 || true
ENGINES=(SAW FM ORGAN PD PIANO EPIANO)
SEEDS=(1 2 3 7)
OUTDIR=tools/patch-match/results
mkdir -p "$OUTDIR"
OUT="$OUTDIR/${LABEL}.txt"
: > "$OUT"
echo "# oracle matrix  $(date -u +%Y-%m-%dT%H:%M:%SZ)  pm=$PM  extra=$*" | tee -a "$OUT"
top3=0 first=0 n=0
for e in "${ENGINES[@]}"; do
  for s in "${SEEDS[@]}"; do
    n=$((n+1))
    echo "" | tee -a "$OUT"
    echo "===== $n/24  $e  seed $s =====" | tee -a "$OUT"
    logfile="$OUTDIR/${LABEL}-${e}-s${s}.log"
    set +e
    "$PM" --selftest "$e" --quick --jobs 4 --seed "$s" --out "build/patch-match/oracle-${LABEL}-${e}-s${s}" "$@" > "$logfile" 2>&1
    rc=$?
    set +e
    # Prefer the oracle block; --stage1 only prints the race table + a rank line.
    # grep returning 1 is "not found", not a script failure (pipefail would abort).
    rank=$(grep -E 'race rank' "$logfile" | tail -1 | awk '{print $3}')
    if [ -z "${rank:-}" ]; then
      rank=$(grep -E "^[[:space:]]*[0-9]+\. INSTR_${e}[[:space:]]" "$logfile" | head -1 | awk '{print $1}' | tr -d '.')
    fi
    winner=$(grep -E 'winner' "$logfile" | tail -1 | sed -E 's/.*winner[[:space:]]+//' | sed -E 's/[[:space:]]+.*//')
    if [ -z "${winner:-}" ]; then
      winner=$(grep -E '^[[:space:]]*1\. INSTR_' "$logfile" | tail -1 | awk '{print $2}')
    fi
    loss=$(grep -E 'loss[[:space:]]+' "$logfile" | grep -v voice | tail -1 | awk '{print $2}')
    if [ -z "${loss:-}" ]; then
      loss=$(grep -E "^[[:space:]]*[0-9]+\. INSTR_${e}[[:space:]]" "$logfile" | head -1 | awk '{print $3}')
    fi
    set -e
    echo "  rc=$rc  race_rank=${rank:-?}  winner=${winner:-?}  loss=${loss:-?}" | tee -a "$OUT"
    if [ -n "${rank:-}" ] && [ "$rank" -le 3 ] 2>/dev/null; then top3=$((top3+1)); fi
    echo "$winner" | grep -qi "$e" && first=$((first+1)) || true
    tail -20 "$logfile" >> "$OUT"
  done
done
echo "" | tee -a "$OUT"
echo "SUMMARY  top3=${top3}/${n}  first=${first}/${n}  (first = winner engine matches truth)" | tee -a "$OUT"
