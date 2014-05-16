
ITERATIONS=10000

measure "${PREFIX}hw_10k.perf.txt"                   10  "$EXECUTABLE" 1 0 -n $ITERATIONS
measure "${PREFIX}sw_10k.perf.txt"                   10  "$EXECUTABLE" 0 1 -n $ITERATIONS
measure "${PREFIX}hw_10k_wo_memory.perf.txt"         10  "$EXECUTABLE" 1 0 -n $ITERATIONS --without-memory
measure "${PREFIX}sw_100k_wo_mbox.perf.txt"          10  "$EXECUTABLE" 0 1 -m $((10*ITERATIONS))
measure "${PREFIX}hw_100k_wo_mbox.perf.txt"          10  "$EXECUTABLE" 1 0 -m $((10*ITERATIONS))
measure "${PREFIX}hw_10k_noflush.perf.txt"           10  "$EXECUTABLE" 1 0 -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_10k_noflush.perf.txt"           10  "$EXECUTABLE" 0 1 -n $ITERATIONS --dont-flush
measure "${PREFIX}hw_10k_wo_memory_noflush.perf.txt" 10  "$EXECUTABLE" 1 0 -n $ITERATIONS --dont-flush --without-memory
