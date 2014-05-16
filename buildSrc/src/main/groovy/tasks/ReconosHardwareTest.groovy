import org.gradle.api.DefaultTask
import org.gradle.api.Project
import org.gradle.api.tasks.Exec
import org.gradle.api.GradleException

class ReconosHardwareTest {
	private Project project
	private String testName
	private DefaultTask cleanTask, prepareTask, compileBitstreamTask, downloadBitstreamTask,
						compileLinuxProgramTask

	ReconosHardwareTest(project, testName, closure=null) {
		this.project  = project
		this.testName = testName

		cleanTask               = project.task(getTaskName("clean", "Hardware"))
		prepareTask             = project.task(getTaskName("prepare", "Hardware"),
									type: ReconosPrepareTask)
		compileBitstreamTask    = project.task(getTaskName("compile", "HardwareBitstream"),
									type: ReconosXpsTask)
		downloadBitstreamTask   = project.task(getTaskName("download", "HardwareBitstream"),
									type: ReconosXmdTask)
		compileLinuxProgramTask = null

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

	public def downloadBitstream(closure=null) {
		if (closure)
			downloadBitstreamTask.configure(closure)

		return downloadBitstreamTask
	}

	public def getDownloadBitstream() {
		return downloadBitstreamTask
	}

	public def getCompileLinuxProgram() {
		return compileLinuxProgramTask
	}

	public def compileLinuxProgram(closure=null) {
		if (closure)
			compileLinuxProgramTask.configure(closure)

		return compileLinuxProgramTask
	}

	public void hardwareDir(dir) {
		prepareTask.hardwareDir(dir)
		compileBitstreamTask.projectDir(prepareTask.outputDir)
		//cleanTask.delete(prepareTask.outputDir)
		downloadBitstreamTask.workingDir(prepareTask.outputDir)
	}

	public void softwareTarget(...targets) {
		makeSureSoftwareTaskExists()

		compileLinuxProgramTask.target(*targets)
	}

	public void softwareDir(dir) {
		makeSureSoftwareTaskExists()

		compileLinuxProgramTask.workingDir(dir)
	}

	public def getSoftwareDir() {
		return compileLinuxProgramTask.workingDir
	}

	private makeSureSoftwareTaskExists() {
		if (!compileLinuxProgramTask) {
			compileLinuxProgramTask = project.task(getTaskName("compile", "Linux"),
										type: CrossCompileMakeTask)
			compileLinuxProgramTask.dependsOn(project.reconosTaskPath("compileReconosLib"))
		}
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
		compileBitstreamTask.doLast {
			def bitstream_file = project.path(compileBitstreamTask.projectDir,
				"implementation", compileBitstreamTask.projectName + ".bit")
			if (!bitstream_file.exists())
				throw new GradleException("XPS didn't generate the bitstream file.")
		}
		compileBitstreamTask.onlyIf {
			def bitstream_file = project.path(compileBitstreamTask.projectDir,
				"implementation", compileBitstreamTask.projectName + ".bit")
			
			return !bitstream_file.exists()
		}


		downloadBitstreamTask.mustRunAfter reconosTaskPath("checkHostIp"), reconosTaskPath("updateNfsRoot")
		downloadBitstreamTask.dependsOn reconosTaskPath("checkDigilentDriver"), compileBitstreamTask
		downloadBitstreamTask.shouldRunAfter reconosTaskPath("downloadImagesToZynq")

		downloadBitstreamTask.command "fpga", "-f", { project.path("implementation", compileBitstreamTask.projectName + ".bit") }
	}
}
