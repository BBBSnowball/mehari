import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.Exec

// This will most likely only work with the :reconos project because
// it is tightly coupled to the reconosConfig task.

public class ReconosPrepareTask extends Exec {
	// we cannot access private variables in closures, so we do not make them private
	def outputExcludePatterns = []
	boolean hardwareDirWasSet = false

	def setup_script, base_design, RECONOS_ARCH, RECONOS_OS, RECONOS = null, edk_dir, used_hw_threads

	public ReconosPrepareTask() {
		if (project.convention.findPlugin(HelpersPluginConvention.class) == null)
			throw new GradleException("Please apply the HelpersPlugin before using ReconosPrepareTask.")
		if (project.convention.findPlugin(ReconosPluginConvention.class) == null)
			throw new GradleException("Please apply the ReconosPlugin before using ReconosPrepareTask.")

		dependsOn project.reconosConfigTaskPath
	}

	public void hardwareDir(dir) {
		workingDir dir
		hardwareDirWasSet = true

		initProperties()
		initInputsAndOutputs()

		// we should add doFirst handlers in the constructor, but if we did this, they would be ignored
		doFirst {
			if (!hardwareDirWasSet)
				throw new GradleException("You must call 'hardwareDir' for ${this}.")
			initCommandLine()
		}

		doFirst {
			updateDesignVersionInConfigFile()
		}

		doFirst {
			if (edk_dir.value.exists())
				project.logger.warn("The output directory '${edk_dir.value}' already exists. The task will likely fail. "
					+ "Please run clean before running this task.")
		}

		doLast {
			project.sh(project.escapeForShell(["touch", project.rootPath(edk_dir, "prepare.done")]))
		}
	}

	public excludeOutput(...patterns) {
		outputExcludePatterns.addAll(patterns)
	}

	public File getSetupScript() { return setup_script }

	public File getOutputDirectory() { return edk_dir.value }
	public File getOutputDir()       { return edk_dir.value }

	public List<String> getUsedHardwareThreads() { return used_hw_threads.asImmutable() }

	public File fileInHwDir(...segments) {
		project.rootPath(getWorkingDir(), *segments)
	}

	private def lazyValue(closure) {
		return new LazyValue(closure)
	}

	private void initProperties() {
		setup_script    = fileInHwDir("setup_zynq")
		base_design     = lazyValue { project.getReconosHwConfigFileInfo(setupScript, "base_design") }
		RECONOS_ARCH    = lazyValue { project.getReconosVariable("RECONOS_ARCH") }
		RECONOS_OS      = lazyValue { project.getReconosVariable("RECONOS_OS")   }
		RECONOS         = lazyValue { project.getReconosVariable("RECONOS")      }
		edk_dir         = lazyValue { fileInHwDir("edk_${RECONOS_ARCH.value}_${RECONOS_OS.value}") }
		used_hw_threads = lazyValue {
			def all_hwts = project.getReconosHwConfigFileInfo(setupScript, "static_hwts") + " " +
				project.getReconosHwConfigFileInfo(setupScript, "reconf_hwts")
			all_hwts.tokenize()*.replaceFirst(/#.*$/, "").unique()
		}
	}

	private void initInputsAndOutputs() {
		inputs.file setup_script
		inputs.property("RECONOS_ARCH") { RECONOS_ARCH.value }
		inputs.property("RECONOS_OS"  ) { RECONOS_OS.value   }
		inputs.property("base_design" ) { base_design.value  }
		//inputs.dir { project.path(RECONOS.value, "tools") }
		//inputs.dir { project.path(RECONOS.value, "designs", "${RECONOS_ARCH.value}_${RECONOS_OS.value}_${base_design.value}") }
		//inputs.dir { project.path(RECONOS.value, "pcores") }
		//inputs.files { project.fileTree(fileInHwDir()) { used_hw_threads.value.each { hwt -> include hwt } } }
		outputs.dir {
			project.fileTree(getOutputDirectory().toString()) {
				/*include "data", "etc", "pcores", "device_tree.dts", "ps7_init.tcl", "system.mhs", "system.xmp"

				// created by other tasks
				exclude "hdl", "implementation", "platgen.*", "revup", "synthesis", "__xps"
				exclude "system.mhs.orig", "ps_clock_registers.log", "system.log"

				outputExcludePatterns.each { pattern ->
					exclude pattern
				}

				// created by this task, but changed later
				exclude "system_incl.make", "system.make", "system.xmp"*/

				include "prepare.done"
			}
		}
	}

	void initCommandLine() {
		def setup_script_name = FileLinkAction.getRelativePath(setup_script, getWorkingDir()).replaceFirst("^\\./", "")

		commandLine "bash", "-c", ". ${project.reconosConfigScriptEscaped} && " +
			"rm -rf edk_${RECONOS_ARCH}_${RECONOS_OS} && " +
			"bash reconos_setup.sh ${project.escapeForShell(setup_script_name)}"
	}

	void updateDesignVersionInConfigFile() {
		//TODO get design for 14.7 from Christoph
		//     Then set version before executing the script.
		//     sed -i 's/zedboard_minimal_[0-9].*/zedboard_minimal_'"$XILINX_VERSION"'/' setup_zynq
		//TODO Make sure that this change doesn't cause unnecessary recompiles.
		//TODO Only change it, if the design exists for that version.
		//TODO Make sure that we use the updated value to set the inputs.
	}
}
