import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.Exec
import org.gradle.api.file.FileTree

public class ReconosMeasureTask extends DefaultTask {
	// we cannot access private variables in closures, so we do not make them private
	String test_name
	public Object binaryForTarget, binaryOnTarget,
		measurements, measurementsOnTarget, measureScript, measureScriptOnTarget,
		resultsTarOnTarget, resultsTarOnHost, resultsDirOnHost
	public Object compileBitstreamTask

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

		compileBitstreamTask = tasks.compileBitstream

		if (binaryForTarget == null)
			binaryForTarget    = tasks.compileLinuxProgram.binaryForTarget
	}


	boolean done

	@TaskAction
	public void measurePerformance() {
		// Instanitate MySshService because it adds the known_hosts file
		// to the DefaultSshService. We cannot tell the plugin to use our
		// service, but fortunately it will work nonetheless.
		MySshService.instance;

		if (compileBitstreamTask)
			ReconosXmdTask.downloadBitstream(compileBitstreamTask)

		// done is used in a weird way to work through all the levels
		// of closures...
		def my_task = this
		my_task.done = false
		while (!my_task.done) {
			project.sshexec {
				session(project.remotes.board) {
					try {
						put(binaryForTarget   .toString(), binaryOnTarget       .toString())
						put(measurements      .toString(), measurementsOnTarget .toString())
						put(measureScript     .toString(), measureScriptOnTarget.toString())

						execute("chmod +x $binaryOnTarget $measureScriptOnTarget")
						execute("sh $measureScriptOnTarget $test_name")

						get(resultsTarOnTarget.toString(), resultsTarOnHost.toString())

						project.exec {
							commandLine "tar", "-C", project.file("."), "-xf", resultsTarOnHost
						}

						my_task.done = true
					} catch (com.jcraft.jsch.SftpException e) {
						if (e.toString().equals("4: java.io.IOException: channel is broken")) {
							// It sometimes fails with this error. This seems to be a bug in
							// either Dropbear or the SSH library, so we ignore it and try again.
							System.err.println("WARN: Channel is broken. Trying again.");
							my_task.done = false
						} else {
							// another error -> don't ignore
							throw new RuntimeException(e);
						}
					}
				}
			}
		}

		project.exec {
			commandLine project.toolsFile("perf", "evaluate-measurement.py")
			workingDir resultsDirOnHost
		}
	}
}
