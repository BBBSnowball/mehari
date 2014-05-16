import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.Exec
import java.util.concurrent.Callable

public class ReconosXmdTask extends DefaultTask {
	// we cannot access private variables in closures, so we do not make them private
	def xmdCommands = []
	String workingDirectory = null
	String project_name = "system"

	public ReconosXmdTask() {
		if (project.convention.findPlugin(HelpersPluginConvention.class) == null)
			throw new GradleException("Please apply the HelpersPlugin before using ReconosPrepareTask.")
		if (project.convention.findPlugin(ReconosPluginConvention.class) == null)
			throw new GradleException("Please apply the ReconosPlugin before using ReconosPrepareTask.")

		inputs.files {
			xmdCommands.collectNested({ if (it instanceof Callable) it() else it }).flatten().findAll({ it instanceof File })
		}
	}

	public void command(...cmdParts) {
		xmdCommands.add(cmdParts)
	}

	public void projectName(name) {
		this.project_name = name
	}

	public void workingDir(dir) {
		workingDirectory = project.file(dir)
	}

	public String getWorkingDir() {
		return workingDirectory
	}

	@TaskAction
	def runXmd() {
		if (xmdCommands.isEmpty())
			throw new GradleException("You must call 'command' at least once for ${this}.")

		if (workingDirectory == null)
			throw new GradleException("You must call 'workingDir' for ${this}.")

		def commands = xmdCommands.collect(this.&prepareCommand).join(" ; ")

		//println("project.runXmd(${commands.inspect()}, $workingDir)")
		project.runXmd(commands, workingDir)
	}

	String prepareCommand(cmdParts) {
		println("prepareCommand(#{cmdParts.inspect})")
		cmdParts = cmdParts.toList().collectNested({ if (it instanceof Callable) it() else it }).flatten()
		return cmdParts.collect {
			if (it instanceof File) {
				inputs.file it
				return project.escapeForShell(it)
			} else {
				//TODO Should we escape it anyway? Does this make a difference in TCL, e.g. string vs. command?
				return it
			}
		}.join(" ")
	}

	public static downloadBitstream(compileBitstreamTask) {
		project = compileBitstreamTask.project
		def bitstream_file = project.path("implementation", compileBitstreamTask.projectName + ".bit")
		command = "fpga", "-f", project.escapeForShell(bitstream_file)
		project.runXmd(command, compileBitstreamTask.projectDir)
	}
}
