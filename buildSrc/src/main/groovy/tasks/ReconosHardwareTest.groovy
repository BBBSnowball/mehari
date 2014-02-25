import org.gradle.api.DefaultTask
import org.gradle.api.Project
import org.gradle.api.tasks.Exec

class ReconosHardwareTest {
	private Project project
	private String testName
	private DefaultTask cleanTask, prepareTask, compileBitstreamTask, downloadBitstreamTask

	ReconosHardwareTest(project, testName, closure=null) {
		this.project  = project
		this.testName = testName

		cleanTask             = project.task(getTaskName("clean"))
		prepareTask           = project.task(getTaskName("prepare"), type: ReconosPrepareTask)
		compileBitstreamTask  = project.task(getTaskName("compile", "Bitstream"), type: ReconosXpsTask)
		downloadBitstreamTask = project.task(getTaskName("download", "Bitstream"), type: ReconosXmdTask)

		configureTasks()

		if (closure) {
			closure.delegate = this
			closure()
		}
	}

	private String getTaskName(String task, String suffix="") {
		return task + testName.capitalize() + suffix.capitalize()
	}

	public def clean(closure=null) {
		if (closure)
			cleanTask.configure(closure)

		return cleanTask
	}

	public def getClean() {
		return cleanTask
	}

	public def prepare(closure=null) {
		if (closure)
			prepareTask.configure(closure)

		return prepareTask
	}

	public def getPrepare() {
		return prepareTask
	}

	public def compileBitstream(closure=null) {
		if (closure)
			compileBitstreamTask.configure(closure)

		return compileBitstreamTask
	}

	public def getCompileBitstream() {
		return compileBitstreamTask
	}

	public void hardwareDir(dir) {
		prepareTask.hardwareDir(dir)
		compileBitstreamTask.projectDir(prepareTask.outputDir)
		//cleanTask.delete(prepareTask.outputDir)
		downloadBitstreamTask.workingDir(prepareTask.outputDir)
	}

	private String reconosTaskPath(String taskName) {
		return project.reconosTaskPath(taskName)
	}

	private void configureTasks() {
		// we cannot use a Delete task because it cannot handle symlinks
		cleanTask << {
			project.exec {
				commandLine "rm", "-rf", prepare.outputDir
			}
		}


		prepareTask.mustRunAfter cleanTask


		compileBitstreamTask.dependsOn prepareTask

		compileBitstreamTask.inputs.files { prepareTask.outputs }
		
		compileBitstreamTask.command "run bits"
		compileBitstreamTask.projectName "system"


		downloadBitstreamTask.mustRunAfter reconosTaskPath("checkHostIp"), reconosTaskPath("updateNfsRoot")
		downloadBitstreamTask.dependsOn reconosTaskPath("checkDigilentDriver"), compileBitstreamTask
		downloadBitstreamTask.shouldRunAfter reconosTaskPath("downloadImagesToZynq")

		downloadBitstreamTask.command "fpga", "-f", { project.path("implementation", compileBitstreamTask.projectName + ".bit") }
	}
}
