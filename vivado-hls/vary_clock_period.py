from hls import HLS
from hls_examples import examples, clean_for_filename, mehari_private
import os, json

example = "DC Motor"

hls = HLS(mehari_private)
hls.create_project(clean_for_filename(example + "clock-frequency"), examples[example])
hls.directive("""set_directive_inline -recursive "run" """)
hls.directive("""set_directive_dataflow "run" """)


results = []

for steps in [1]: #[1, 2, 10, 100]:
	print "STEPS = %d" % steps
	hls.steps = steps

	for clock_period in range(1, 3): # 21) + [30, 50, 100, 200, 500]:
		hls.clock_period = clock_period
		result = hls.csynth_design()
		print "%3d  %6.2f  (%s)" % (clock_period, result.runtime, result.solution)

		results.append([result.example_cfile, result.steps, result.clock_period, result.runtime, result.solution])

for example_cfile, steps, clock_period, runtime, solution in results:
	print "%s:    %4d  %6.2f  %8.2f  (%s)" % (example_cfile, steps, float(clock_period), runtime, solution)

with open("vary-clock-period.json", "w") as f:
	json.dump(results, f, indent=4)
