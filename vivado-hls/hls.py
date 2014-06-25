import os, json
from lxml import etree

def mkdir_p(path):
	if not os.path.exists(path):
		os.makedirs(path)

class HLS(object):
	def __init__(self, mehari_private_path):
		self.mehari_private_path = mehari_private_path
		self.program_dir = os.path.dirname(__file__)

		self.steps = 100
		self.part = "xc7z020clg400-1"
		self.clock_period = 10
		self.directives = []

		self.next_solution = 1
		self.solutions = []

	def create_project(self, project_name, example_cfile):
		self.project_name = project_name
		self.example_cfile = example_cfile

		self.process_example_cfile(example_cfile)

	def directive(self, directive):
		self.directives.append(directive)

	def csynth_design(self, solution=None):
		if solution is None:
			solution = "solution%d" % self.next_solution
			self.next_solution += 1
		self.solution = solution

		project_script = "%s/%s/script.tcl" % (self.project_name, self.solution)
		self.write_project_script(project_script)

		self.write_solution_script("%s/%s/directives.tcl" % (self.project_name, self.solution))

		self.run_vivado_hls(project_script)

		result = HLSSolution(self)
		self.solutions.append(result)
		return result


	# internal methods

	def get_cflags(self):
		ipanema = self.mehari_private_path + "/ipanema"
		cflags = ("-DDOUBLE_FLOAT -D__POSIX__ -DDSC_EVENTQUEUE -D__RESET -DEXEC_SEQ " +
			"-I%s/include/syslayer/linux -I%s/include/sim" % (ipanema, ipanema))
		if self.steps is not None:
			cflags += " -DSTEPS=%d" % self.steps
		return cflags

	def write_project_script(self, filename):
		mkdir_p(os.path.dirname(filename))
		with open(filename, "w") as f:
			f.write("""open_project %s\n""" % self.project_name)
			f.write("""set_top run\n""")
			f.write("""add_files "%s" -cflags "%s"\n""" % (self.example_cfile_processed, self.get_cflags()))
			f.write("""open_solution "%s"\n""" % self.solution)
			f.write("""set_part {%s}\n""" % self.part)
			f.write("""create_clock -period %s -name default\n""" % self.clock_period)
			f.write("""source "./%s/%s/directives.tcl"\n""" % (self.project_name, self.solution))
			f.write("""csynth_design\n""")
			f.write("""exit\n""")

	def write_solution_script(self, filename):
		mkdir_p(os.path.dirname(filename))
		with open(filename, "w") as f:
			f.write("\n".join(self.directives) + "\n")

	def process_example_cfile(self, example_cfile):
		self.example_cfile_processed = os.path.basename(example_cfile)

		os.system("%s/process-example-for-vivado-hls.sh '%s' >'%s'"
			% (self.program_dir, example_cfile, self.example_cfile_processed))

	def run_vivado_hls(self, project_script):
		command = "vivado_hls -f \"%s\"" % project_script

		if "XILINX" not in os.environ:
			if "XILINX_SETTINGS_SCRIPT" in os.environ:
				command = "bash -c '. $XILINX_SETTINGS_SCRIPT && " + command + "'"
			else:
				raise Exception("Please run settings64.sh or set $XILINX_SETTINGS_SCRIPT before running this script!")

		if os.system(command) != 0:
			raise Exception("Vivado HLS has returned an error code!")


class HLSSolution(object):
	def __init__(self, hls):
		self.mehari_private_path = hls.mehari_private_path
		self.steps               = hls.steps
		self.part                = hls.part
		self.clock_period        = hls.clock_period
		self.directives          = hls.directives
		self.project_name        = hls.project_name
		self.example_cfile       = hls.example_cfile
		self.solution            = hls.solution

		self.parse("%s/%s/syn/report/run_csynth.xml" % (self.project_name, self.solution))
		self.save("%s/%s/syn/report/run_csynth.json" % (self.project_name, self.solution))

	def to_json(self, **args):
		return json.dumps(self.__dict__, **args)


	def parse(self, xml_report_file):
		report = etree.parse(xml_report_file)
		self.target_clock_period = report.xpath("/profile/UserAssignments/TargetClockPeriod")[0].text
		self.clock_uncertainty = report.xpath("/profile/UserAssignments/ClockUncertainty")[0].text
		self.clock_period = report.xpath("/profile/PerformanceEstimates/SummaryOfTimingAnalysis/EstimatedClockPeriod")[0].text
		self.latency_best  = report.xpath("/profile/PerformanceEstimates/SummaryOfOverallLatency/Best-caseLatency")[0].text
		self.latency_avg   = report.xpath("/profile/PerformanceEstimates/SummaryOfOverallLatency/Average-caseLatency")[0].text
		self.latency_worst = report.xpath("/profile/PerformanceEstimates/SummaryOfOverallLatency/Worst-caseLatency")[0].text
		self.used_area = report.xpath("/profile/AreaEstimates/Resources/*")
		self.available_area = report.xpath("/profile/AreaEstimates/AvailableResources/*")
		self.utilization = [round(100*float(used.text)/float(available.text))
								for used, available in zip(self.used_area, self.available_area)]
		self.used_area = [area.text for area in self.used_area]
		self.available_area = [area.text for area in self.available_area]

		self.runtime = float(self.clock_period) * int(self.latency_worst)
		self.valid = (all([0 <= u <= 1.0 for u in self.utilization])
			and float(self.clock_period) < float(self.target_clock_period) - float(self.clock_uncertainty))

	def save(self, json_file):
		with open(json_file, "w") as f:
			f.write(self.to_json(indent=4) + "\n")


if __name__ == "__main__":
	#TODO remove
	os.environ["XILINX_SETTINGS_SCRIPT"] = "/opt/Xilinx/Vivado/2013.4/settings64.sh"

	mehari_private = "/home/benny/mehari-private"
	hls = HLS(mehari_private)
	hls.create_project("blub", mehari_private + "/examples/dc_motor/dc_motor.c")
	hls.directive("""set_directive_inline -recursive "run" """)
	hls.directive("""set_directive_dataflow "run" """)
	result = hls.csynth_design()
	print result.to_json()
