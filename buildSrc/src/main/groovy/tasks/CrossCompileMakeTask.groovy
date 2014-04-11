import org.gradle.api.DefaultTask
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.Exec

// This will most likely only work with the :reconos project because
// it is tightly coupled to the reconosConfig task.

public class CrossCompileMakeTask extends Exec {
	private ArrayList<Object> targets = []

	public CrossCompileMakeTask() {
		dependsOn project.reconosConfigTaskPath
	}

	public void target(...targets) {
		this.targets.addAll(targets)

		doFirst { updateCommandLine() }
	}

	@Input
	public List<String> getTargets() {
		return project.stringList(this.targets).findAll { item -> item != "" }
	}

	public int getParallelCompilationProcesses() {
		if (project.hasProperty("parallel_compilation_processes"))
			return project.parallel_compilation_processes
		else
			4
	}

	void updateCommandLine() {
		def targets = project.escapeForShell(getTargets())

		def parallel_processes = getParallelCompilationProcesses()
		def reconosConfig = project.getReconosConfigTask()

		commandLine "bash", "-c", ". ${reconosConfig.reconosConfigScriptEscaped} && make -j$parallel_processes $targets"
	}
}
