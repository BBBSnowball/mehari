import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.Exec
import org.gradle.api.file.FileTree

public class ReconosMeasureTask extends DefaultTask {
	// we cannot access private variables in closures, so we do not make them private
	String test_name
	public Object binaryForTarget, binaryOnTarget, bitstreamForTarget, bitstreamOnTarget,
		measurements, measurementsOnTarget, measureScript, measureScriptOnTarget,
		resultsTarOnTarget, resultsTarOnHost, resultsDirOnHost

	public ReconosMeasureTask() {
		if (project.convention.findPlugin(HelpersPluginConvention.class) == null)
			throw new GradleException("Please apply the HelpersPlugin before using ReconosMeasureTask.")
		if (project.convention.findPlugin(ReconosPluginConvention.class) == null)
			throw new GradleException("Please apply the ReconosPlugin before using ReconosMeasureTask.")

		measurements          = project.rootPath("measurements.sh")
		measureScript         = project.toolsFile("perf", "measure.sh")
		measureScriptOnTarget = "/tmp/measure.sh"

		dependsOn project.reconosTaskPath("prepareSsh")
	}

	public void testName(name) {
		test_name = name

		if (binaryOnTarget == null)
			binaryOnTarget       = "/tmp/${test_name}"
		if (bitstreamOnTarget == null)
			bitstreamOnTarget    = "/tmp/${test_name}.bin"
		if (measurementsOnTarget == null)
			measurementsOnTarget = "/tmp/${test_name}_measurements.sh"
		if (resultsTarOnTarget == null)
			resultsTarOnTarget   = "/tmp/${test_name}_results.tar"
		if (resultsTarOnHost == null)
			resultsTarOnHost     = project.file("${test_name}_results.tar")
		if (resultsDirOnHost == null)
			resultsDirOnHost     = project.file("${test_name}_results")
	}

	public String getTestName() {
		return test_name
	}

	public void reconosTestTasks(tasks) {
		dependsOn tasks.compileLinuxProgram
		dependsOn tasks.compileBitstream

		if (binaryForTarget == null)
			binaryForTarget    = tasks.compileLinuxProgram.binaryForTarget
		if (bitstreamForTarget == null)
			bitstreamForTarget = tasks.compileBitstream.binFile
	}

	@TaskAction
	def runMeasurements() {
		MySshService.sshexec {
			session(project.remotes.board) {
				put(binaryForTarget   .toString(), binaryOnTarget       .toString())
				put(bitstreamForTarget.toString(), bitstreamOnTarget    .toString())
				put(measurements      .toString(), measurementsOnTarget .toString())
				put(measureScript     .toString(), measureScriptOnTarget.toString())

				execute("chmod +x $binaryOnTarget $measureScriptOnTarget")
				execute("sh $measureScriptOnTarget $name")

				get(resultsTarOnTarget.toString(), resultsTarOnHost.toString())

				exec {
					commandLine "tar", "-C", file("."), "-xf", resultsTarOnHost
				}
			}
		}

		project.exec {
			commandLine project.toolsFile("perf", "evaluate-measurement.py")
			workingDir resultsDirOnHost
		}
	}
}
