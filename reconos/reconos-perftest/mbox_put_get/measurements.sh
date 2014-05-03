
ITERATIONS=10000

measure "${PREFIX}hw_1_10k.perf.txt" 10   "$EXECUTABLE" 1 0  -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_1_10k.perf.txt" 10   "$EXECUTABLE" 0 1  -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_2_10k.perf.txt" 10   "$EXECUTABLE" 0 2  -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_32_10k.perf.txt" 10  "$EXECUTABLE" 0 32 -n $ITERATIONS --dont-flush
