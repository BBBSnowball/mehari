import org.gradle.api.DefaultTask
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input

// This will most likely only work with the :reconos project because
// it is tightly coupled to the reconosConfig task.

//TODO support everything that Exec support, e.g. extend Exec and only change commandLine

public class CrossCompileMakeTask2 extends DefaultTask {
	private ArrayList<Object> targets = []
	private Object dir = null

	public CrossCompileMakeTask2() {
		//recommendedProperty "parallel_compilation_processes"

		dependsOn "reconosConfig"
	}

	public void target(...targets) {
		this.targets.addAll(targets)
	}

	public void workingDir(dir) {
		this.dir = dir
	}

	@Input
	public List<String> getTargets() {
		return project.stringList(this.targets).findAll { item -> item != "" }
	}

	public File getWorkingDir() {
		return project.file(this.dir)
	}

	public int getParallelCompilationProcesses() {
		if (project.hasProperty("parallel_compilation_processes"))
			return project.parallel_compilation_processes
		else
			4
	}

	@TaskAction
	void process() {
		def targets = project.escapeForShell(getTargets())

		def parallel_processes = getParallelCompilationProcesses()
		def dir = this.dir
		def reconosConfig = project.tasks["reconosConfig"]

		project.exec {
			commandLine "bash", "-c", ". ${reconosConfig.reconosConfigScriptEscaped} && make -j$parallel_processes $targets"

			if (dir != null)
				workingDir dir
		}
	}
}
