import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.Exec

// This will most likely only work with the :reconos project because
// it is tightly coupled to the reconosConfig task.

public class ReconosXpsTask extends DefaultTask {
	// we cannot access private variables in closures, so we do not make them private
	def xpsCommands = []
	String xps_project_dir = null
	String project_name = "system"

	public ReconosXpsTask() {
		if (project.convention.findPlugin(HelpersPluginConvention.class) == null)
			throw new GradleException("Please apply the HelpersPlugin before using ReconosPrepareTask.")
		if (project.convention.findPlugin(ReconosPluginConvention.class) == null)
			throw new GradleException("Please apply the ReconosPlugin before using ReconosPrepareTask.")

		dependsOn project.reconosConfigTaskPath
	}

	public void command(cmd) {
		xpsCommands.add(cmd)
	}

	public void projectName(name) {
		this.project_name = name
	}

	public void projectDir(dir) {
		xps_project_dir = project.file(dir)

		initInputsAndOutputs()
	}

	public String getProjectName() {
		return project_name
	}

	public String getProjectDir() {
		return xps_project_dir
	}

	@TaskAction
	def runXps() {
		if (xpsCommands.isEmpty())
			throw new GradleException("You must call 'command' at least once for ${this}.")

		if (xps_project_dir == null)
			throw new GradleException("You must call 'projectDir' for ${this}.")

		def project_file = project.path(projectDir, project_name + ".xmp")
		if (!project_file.exists())
			throw new GradleException("The project file '$project_file' doesn't exist.")

		project.runXps("system", xpsCommands.join(" ; "), projectDir)
	}

	def initInputsAndOutputs() {
		inputs.property("RECONOS_ARCH") { project.getReconosVariable("RECONOS_ARCH") }
		inputs.property("RECONOS_OS"  ) { project.getReconosVariable("RECONOS_OS")   }

		outputs.files { project.fileTree(project.path(projectDir, "implementation")) { include "*.bit" } }
	}
}
