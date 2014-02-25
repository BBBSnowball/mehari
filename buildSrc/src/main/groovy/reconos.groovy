import org.gradle.api.Project
import org.gradle.api.Plugin
import org.gradle.api.GradleException
import java.util.regex.Pattern
import org.gradle.api.tasks.Exec
import java.util.concurrent.Callable
import org.gradle.api.Task
import org.gradle.api.tasks.TaskState

class ReconosPluginConvention {
	private Project project

	private HelpersPluginConvention helpers

	public ReconosPluginConvention(Project project) {
		this.project = project
		this.helpers = project.convention.getPlugin(HelpersPluginConvention.class)

		this.addExtensions()
	}

	private def getLogger() {
		return project.logger
	}


	private boolean findXilinxDone = false
	private File xilinx_settings_script, xil_tools
	private String xilinx_version = null

	private File getXilinxPathFromSettingsScript(File xilinx_settings_script) {
		def xilinx_settings_script_escaped = helpers.escapeForShell(xilinx_settings_script)
		def xilinx_path = helpers.backticks(". $xilinx_settings_script_escaped >/dev/null ; echo \$XILINX", "bash")
		return new File(xilinx_path).parentFile.parentFile
	}

	protected def findXilinx() {
		if (findXilinxDone)
			return

		def env = System.getenv()

		if (project.hasProperty("xilinx_version"))
			xilinx_version = project.xilinx_version
		else if ("XILINX_VERSION" in env)
			xilinx_version = env["XILINX_VERSION"]
		else
			xilinx_version = null

		if (project.hasProperty("xilinx_settings_script")) {
			xilinx_settings_script = project.file(project.xilinx_settings_script)
			xil_tools = null
		} else if ("xil_tools" in env)
			xil_tools = new File(env["xil_tools"])
		else if ("XILINX" in env)
			xil_tools = new File(env["XILINX"]).parentFile.parentFile
		else if ("XILINX_SETTINGS_SCRIPT" in env) {
			xilinx_settings_script = new File(env["XILINX_SETTINGS_SCRIPT"])
			xil_tools = null
		} else {
			if (!xilinx_version)
				xilinx_version = "14.6"
			xil_tools = helpers.path("/opt/Xilinx", xilinx_version)
		}

		if (!xilinx_settings_script && xil_tools)
			xilinx_settings_script = helpers.path(xil_tools, "ISE_DS", "settings64.sh")

		if (!xil_tools)
			xil_tools = getXilinxPathFromSettingsScript(xilinx_settings_script)

		if (!xil_tools.isDirectory()) {
			logger.error(helpers.heredoc("""
				ERROR: Couldn't find the directory with Xilinx tools. Please install Xilinx ISE_DS
				  (Webpack will do) and tell me about the location. You have several options:
				- Set the environment variable XILINX=/path/to/ISE_DS/ISE
				- Set the environment variable XILINX_VERSION, if the tools live in /opt/Xilinx/\$XILINX_VERSION
				- Set the environment variable xil_tools to the directory that contains the ISE_DS directory
				  (This may not work with other scripts, so you may want to choose one of the other options.)
				"""))
			throw new GradleException("Couldn't find the directory with Xilinx tools.")
		}

		findXilinxDone = true
	}

	public void checkXilinx() {
		findXilinx()
	}

	public File getXilinxSettingsScript() {
		findXilinx()
		return xilinx_settings_script
	}

	public String getXilinxSettingsScriptEscaped() {
		return helpers.escapeForShell(getXilinxSettingsScript())
	}

	public File getXilinxDirectory() {
		findXilinx()
		return xil_tools
	}

	public File xilinxFile(...pathElements) {
		return helpers.path(getXilinxDirectory(), *pathElements)
	}

	public File getXilinxGnutools() {
		return helpers.filePropertyOrDefault("gnutools",
			xilinxFile("ISE_DS", "EDK", "gnu", "arm", "lin", "bin", "arm-xilinx-linux-gnueabi-"))
	}

	public File getXilinxLibcDir() {
		return helpers.filePropertyOrDefault("libc_dir",
			helpers.path(getXilinxGnutools().parentFile.parentFile, "arm-xilinx-linux-gnueabi", "libc"))
	}

	def getXilinxVersion() {
		if (!xilinx_version)
			xilinx_version = helpers.backticks(
				". ${getXilinxSettingsScriptEscaped()} ; " +
				/xps -help|sed -n 's#^Xilinx EDK \([0-9]\+\.[0-9]\+\)\b.*$#\1#p'/, "bash")

		return xilinx_version
	}

	def runXps(project_name, commands, workingDir=null) {
		project_name = helpers.escapeForShell(project_name)
		commands = helpers.escapeForShell(commands + " ; exit")
		def cmd = "echo $commands | xps -nw $project_name"
		cmd = ". ${project.reconosConfigScriptEscaped} && " + cmd
		if (workingDir)
			cmd = "cd " + helpers.escapeForShell(workingDir) + " && " + cmd

		// This sometimes fails with a segfault (prints "Segmentation fault" and
		// exits with status 139). We run it again in that case.
		// (For that reason, we cannot use exec { ... })

		// try at most 3 times
		for (i in [1,2,3]) {
			def res = helpers.sh_test(cmd, "bash")
			if (res == 0)
				return
			else if (res == 139)
				logger.warn("xps failed. This may be due to a segfault, so we try again.")
			else
				break
		}
		// Either it didn't fail with a segfault or it failed too often
		throw new GradleException("System command returned non-zero status (return value is $res): ${cmd.inspect()}")
	}

	def runXmd(commands) {
		commands = escapeForShell(commands.toString() + " ; exit")
		sh(". ${project.reconosConfigScriptEscaped} && echo $commands | xmd", "bash")
	}

	private final String reconosProjectPath = ":reconos"
	private boolean reconosConfigHasRun = false

	public String reconosTaskPath(String taskName) {
		return "${reconosProjectPath}:${taskName}"
	}

	public String getReconosConfigTaskPath() {
		return reconosTaskPath("reconosConfig")
	}

	public void checkAfterReconosConfig() {
		if (!reconosConfigHasRun)
			throw new GradleException("You cannot do this before the reconosConfig task has been executed. "
				+ "You should add 'dependsOn reconosConfigTaskPath' to your task. In addition, "
				+ "you may have to defer some operations until the task is executed (especially don't do it at "
				+ "configuration time).")
	}

	public File reconosFile(...pathElements) {
		//return project.rootProject.file(helpers.path("reconos", "reconos", *pathElements))
		def reconosProjectPath = project.project(reconosProjectPath).projectDir
		return project.rootProject.file(helpers.path(reconosProjectPath, "reconos", *pathElements))
	}

	public File getReconosRootDir() {
		return reconosFile()
	}

	public File getDefaultReconosConfig() {
		return reconosFile("tools", "environment", "default.sh")
	}

	public File getReconosConfigScript() {
		return reconosFile("tools", "environment", "setup_reconos_toolchain.sh")
	}

	public String getReconosConfigScriptEscaped() {
		return helpers.escapeForShell(getReconosConfigScript())
	}

	private Map<String, String> reconosVariables = null

	private String getReconosVariable(varname=null) {
		if (reconosVariables == null) {
			def cmd = ". ${reconosConfigScriptEscaped} >/dev/null"
			reconosVariables = helpers.getVariablesFromShell(cmd, ["RECONOS", "RECONOS_ARCH", "RECONOS_OS", "CROSS_COMPILE"])
		}

		if (varname == null)
			return reconosVariables
		else if (reconosVariables.containsKey(varname))
			return reconosVariables[varname]
		else
			throw new GradleException("invalid reconOS variable name: $varname")
	}

	public getCrosscompileEnvvarAfterReconosConfig() {
		def value = getReconosVariable("CROSS_COMPILE")

		if (value == "")
			throw new GradleException("\$CROSS_COMPILE is empty after running the reconOS config script")

		return value
	}

	def getCrosscompileHostSpecAfterReconosConfig() {
		// get CROSS_COMPILE environment variable and remove path and trailing minus
		def crosscompile = new File(getCrosscompileEnvvarAfterReconosConfig()).getName().trim()
		return (crosscompile =~ /-$/).replaceFirst("")
	}

	def getCrosscompileToolchainDirAfterReconosConfig() {
		return new File(getCrosscompileEnvvarAfterReconosConfig()).getParentFile()
	}

	private Map<String, Map<String, String>> reconosHwConfigVariables = [:]

	def getReconosHwConfigFileInfo(configfile, varname=null) {
		configfile = new File(configfile.toString()).canonicalPath
		if (!reconosHwConfigVariables.containsKey(configfile)) {
			def cmd = ". ${reconosConfigScriptEscaped} >/dev/null && . ${helpers.escapeForShell(configfile)} >/dev/null"
			def interesting_vars = ["base_design", "num_static_hwts", "static_hwts", "num_reconf_regions",
				"num_reconf_hwts", "reconf_hwts"]
			reconosHwConfigVariables[configfile] = helpers.getVariablesFromShell(cmd, interesting_vars)
		}

		if (varname == null)
			return reconosHwConfigVariables[configfile]
		else if (reconosHwConfigVariables[configfile].containsKey(varname))
			return reconosHwConfigVariables[configfile][varname]
		else
			throw new GradleException("invalid reconOS hardware config variable name: $varname")
	}

	ReconosHardwareTest reconosHardwareTest(String name, Callable closure=null) {
		return new ReconosHardwareTest(project, name, closure)
	}


	private def addExtensions() {
		project.gradle.taskGraph.afterTask { Task task, TaskState state ->
			if (task.path == reconosProjectPath + ":reconosConfig") {
				reconosConfigHasRun = true
			}
		}
	}
}

public class ReconosPlugin implements Plugin<Project> {
	void apply(Project project) {
		if (project.convention.findPlugin(HelpersPluginConvention.class) == null)
			throw new GradleException("Please apply the HelpersPlugin before the ReconosPlugin.")

        project.convention.plugins.put('reconos', new ReconosPluginConvention(project))
	}
}
