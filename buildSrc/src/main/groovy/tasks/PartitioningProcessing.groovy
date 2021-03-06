import org.gradle.api.DefaultTask
import org.gradle.api.Project
import org.gradle.api.tasks.Copy
import org.gradle.api.tasks.Exec
import org.gradle.api.GradleException
import org.gradle.api.InvalidUserDataException

class PartitioningProcessing {
	private Project project
	private File example
	private String exampleName
	private String partitioningMethod
	private DefaultTask prepareTask, createIRTask, prepareInliningTask, applyInliningTask, speedUpAnalysisTask, createGraphsTask,
	prepareCodeGenerationTask, copyMehariSourcesTask, partitioningTask, compilePartitioningResultTask, testPartitioningResultTask
	private ReconosHardwareTest hardwareTasks

	PartitioningProcessing(project, example, partitioningMethod, closure=null) {
		this.project  = project
		this.example = example
		this.partitioningMethod = partitioningMethod
		this.exampleName = example.name.lastIndexOf('.').with {it != -1 ? example.name[0..<it] : example.name}

		prepareTask = project.task(getTaskName("prepare"), type: Exec) {
			doFirst {
				project.OUTPUT_DIR.mkdirs()
			}

			def target_file = project.file("${project.OUTPUT_DIR}/$exampleName"+".c")

			inputs.file example
			outputs.file target_file

			// remove redundant function calles from example
			commandLine "bash", "-c", """sed '/^\\s*if\\s*(\\s*!\\s*(\\*status.*\$/ d' $example > $target_file"""
		}

		createIRTask = project.task(getTaskName("create", "IR"), type: Exec) {
			dependsOn project.installLLVM, prepareTask

			def tmp_source_file = project.file("${project.OUTPUT_DIR}/$exampleName"+".c")
			def irfile = project.file("${project.OUTPUT_DIR}/$exampleName"+".ll")

			def ipanema_includes_syslayer = project.file("${project.IPANEMA}/include/syslayer/linux")
			def ipanema_includes_sim = project.file("${project.IPANEMA}/include/sim")

			inputs.file tmp_source_file
			outputs.file irfile

			commandLine "bash", "-c", "${project.LLVM_BIN}/clang " +
			"${project.CFLAGS_FOR_EXAMPLE} " +
			"-S -emit-llvm " +
			"-I$ipanema_includes_syslayer -I$ipanema_includes_sim " + 
			"$tmp_source_file " +
			"-o $irfile"		
		}

		prepareInliningTask = project.task(getTaskName("prepare", "inlining"), type: Exec) {
			dependsOn project.installLLVMPasses, createIRTask

			def sourcefile = project.file("${project.OUTPUT_DIR}/$exampleName"+".ll")
			def targetfile = project.file("${project.OUTPUT_DIR}/$exampleName-prepare-inline"+".ll")

			commandLine "bash", "-c", "${project.LLVM_BIN}/opt " +
			"-load ${project.LLVM_PASSES_LIB} " + 
			"-add-attr-always-inline " +
			"-inline-functions \"evalParameterCouplings\" " +
			"-S $sourcefile > $targetfile"
		}
		
		applyInliningTask = project.task(getTaskName("apply", "inlining"), type: Exec) {
			dependsOn prepareInliningTask

			def sourcefile = project.file("${project.OUTPUT_DIR}/$exampleName-prepare-inline"+".ll")
			def targetfile = project.file("${project.OUTPUT_DIR}/$exampleName-inlined"+".ll")

			commandLine "bash", "-c", "${project.LLVM_BIN}/opt -always-inline -S $sourcefile > $targetfile"
		}
		
		speedUpAnalysisTask = project.task(getTaskName("analyze", "speedup"), type: Exec) {
			dependsOn project.installLLVMPasses, applyInliningTask

			doFirst {
				project.OUTPUT_GRAPH_DIR.mkdirs()
			}

			def targetfile = project.file("${project.OUTPUT_DIR}/$exampleName-inlined"+".ll")

			commandLine "bash", "-c", "${project.LLVM_BIN}/opt " +
			"-load ${project.LLVM_PASSES_LIB} " +
			"-speedup " +
			"-dot " +
			"-speedup-functions \"${project.PARTITIONING_TARGET_FUNCTIONS}\" " +
			"-speedup-output-dir \"${project.OUTPUT_GRAPH_DIR}\" " +
			"-S $targetfile > /dev/null"
		}

		createGraphsTask = project.task(getTaskName("create", "graphs"), type: Exec) {
			dependsOn project.installLLVMPasses, applyInliningTask

			doFirst {
				project.OUTPUT_GRAPH_DIR.mkdirs()
			}

			def targetfile = project.file("${project.OUTPUT_DIR}/$exampleName-inlined"+".ll")

			commandLine "bash", "-c", "${project.LLVM_BIN}/opt " +
			"-load ${project.LLVM_PASSES_LIB} " +
			"-dfg " +
			"-graph-functions \"${project.PARTITIONING_TARGET_FUNCTIONS}\" " +
			"-irgraph-output-dir \"${project.OUTPUT_GRAPH_DIR}\" " +
			"-S $targetfile > /dev/null"
		}

		prepareCodeGenerationTask = project.task(getTaskName("prepare", "codeGeneration"), type: Exec) {
			dependsOn prepareTask

			def partitioningLinux = project.file("${project.PARTITIONING_RESULTS_DIR}/$exampleName/linux")

			doFirst {
				partitioningLinux.mkdirs()
			}

			def inputfile = project.file("${project.OUTPUT_DIR}/$exampleName"+".c")
			def targetName = "$exampleName"+"_partitioned_"+"$partitioningMethod"
			def targetFile = project.file("$partitioningLinux/$targetName"+".c")

			commandLine "python", project.CODEGEN_PREPARATION_SCRIPT, inputfile, targetFile, project.PARTITIONING_TARGET_FUNCTIONS
		}

		copyMehariSourcesTask = project.task(getTaskName("copyMehariSourcesTo"), type: SymLink) {
			from project.MEHARI_SOURCES
			into project.file("${project.PARTITIONING_RESULTS_DIR}/$exampleName/linux")
		}

		partitioningTask = project.task(getTaskName("partition"), type: Exec) {
			dependsOn project.installLLVMPasses, applyInliningTask, prepareCodeGenerationTask, copyMehariSourcesTask

			doFirst {
				project.OUTPUT_GRAPH_DIR.mkdirs()
			}
			
			def targetfile = project.file("${project.OUTPUT_DIR}/$exampleName-inlined"+".ll")

			doFirst {
				commandLine "bash", "-c", "${project.LLVM_BIN}/opt " +
				"-load ${project.LLVM_PASSES_LIB} " +
				"-partitioning " +
				"-template-dir \"${project.TEMPLATE_DIR}\" " +
				"-partitioning-methods \"$partitioningMethod\" " +
				"-partitioning-functions \"${project.PARTITIONING_TARGET_FUNCTIONS}\" " +
				"-partitioning-devices \"${project.PARTITIONING_DEVICES}\" " +
				"-partitioning-output-dir \"${project.PARTITIONING_RESULTS_DIR}/$exampleName\" " +
				"-partitioning-graph-output-dir \"${project.OUTPUT_GRAPH_DIR}\" " +
				"-S $targetfile > /dev/null"
			}
		}

		compilePartitioningResultTask = project.task(getTaskName("compile", "Software"), type: Exec) {
			dependsOn partitioningTask
			def partitioningTargetName = "$exampleName"+"_partitioned_"+"$partitioningMethod"
			commandLine project.PARTITIONED_EXAMPLE_BUILD_SCRIPT, project.file("${project.PARTITIONING_RESULTS_DIR}/$exampleName/linux/$partitioningTargetName"+".c")
		}

		def compilePartitioningResultTaskAll = project.task(getTaskName("compile", "result")) {
			dependsOn compilePartitioningResultTask
		}

		testPartitioningResultTask = project.task(getTaskName("test", "result"), type: Exec) {
			dependsOn compilePartitioningResultTask, ":private:compile"

			environment "PYTHONPATH", project.IPANEMA_PYTHON_LIB_DIR

			def partitioningTargetName = "$exampleName"+"_partitioned_"+"$partitioningMethod"

			workingDir project.PRIVATE_BUILD_DIR
			commandLine "python", 
				project.PARTITIONED_EXAMPLE_TEST_SCRIPT,
				project.rootPath(project.PRIVATE_BUILD_DIR, exampleName),
				project.rootPath(project.PRIVATE_BUILD_DIR, partitioningTargetName),
				project.EXAMPLE_SIMULATION_CONFIG
		}

		def withHardwareThread = project.PARTITIONING_DEVICES.contains("xc7")

		if (withHardwareThread) {
			def singlePendulumDir = project.rootProject.file(project.project(":single-pendulum-vhdl").projectDir)
			def hwtFloatLibraryDir = project.path(singlePendulumDir, "reconos", "hw", "float_library_v1_00_a")
			def hwtMehariDir = project.file("${project.PARTITIONING_RESULTS_DIR}/$exampleName/hw/hwt_mehari_v1_00_a")
			//TODO use symlink for symlinkNetlistFiles and symlinkBbdFile, but make sure that it really works
			def symlinkNetlistFiles = project.task(getTaskName("symlink", "Netlists"), type: Copy) {
				from hwtFloatLibraryDir
				into hwtMehariDir
				include "netlist/*.ngc"

				dependsOn ":single-pendulum-vhdl:createFloatLibraryMetaFiles"
			}

			def symlinkBbdFile = project.task(getTaskName("symlink", "BlackBoxDescription"), type: SymLink) {
				from project.path(hwtFloatLibraryDir, "data")
				into project.path(hwtMehariDir, "data")

				include "float_library_v2_1_0.bbd"
				rename  "float_library_v2_1_0.bbd", "hwt_mehari_v2_1_0.bbd"

				dependsOn ":single-pendulum-vhdl:createFloatLibraryMetaFiles"
			}

			hardwareTasks = project.reconosHardwareTest(getTaskName("", "Hardware")) {
				def hwDir = "${project.PARTITIONING_RESULTS_DIR}/$exampleName/hw"
				hardwareDir hwDir

				prepare {
					dependsOn ":single-pendulum-vhdl:createFloatLibraryMetaFiles",
						symlinkNetlistFiles, symlinkBbdFile,
						partitioningTask

					doLast {
						project.exec {
							commandLine "ln", "-s", hwtFloatLibraryDir.toString(), "."
							workingDir project.rootPath(hwDir, "edk_${RECONOS_ARCH}_${RECONOS_OS}", "pcores")
						}
					}
				}

				downloadBitstream {
					mustRunAfter ":reconos:runDemo"
				}
			}

			compilePartitioningResultTaskAll.dependsOn hardwareTasks.compileBitstream
		}


		configureTasks()

		if (closure) {
			closure.delegate = this
			closure()
		}
	}

	private String getTaskName(String task, String suffix="") {
		return task + exampleName.capitalize() + partitioningMethod.capitalize() + suffix.capitalize()
	}

	public def prepareSource(closure=null) {
		if (closure)
			prepareTask.configure(closure)
		return prepareTask
	}

	public def getPrepare() {
		return prepareTask
	}

	public def createIR(closure=null) {
		if (closure)
			createIRTask.configure(closure)
		return createIRTask
	}

	public def getCreateIR() {
		return createIRTask
	}

	public def prepareInlining(closure=null) {
		if (closure)
			prepareInliningTask.configure(closure)
		return prepareInliningTask
	}

	public def getPrepareInlining() {
		return prepareInliningTask
	}

	public def applyInlining(closure=null) {
		if (closure)
			applyInliningTask.configure(closure)
		return applyInliningTask
	}

	public def getApplyInlining() {
		return applyInliningTask
	}

	public def analyzeSpeedup(closure=null) {
		if (closure)
			speedUpAnalysisTask.configure(closure)
		return speedUpAnalysisTask
	}

	public def getAnalyzeSpeedup() {
		return speedUpAnalysisTask
	}

	public def createGraphs(closure=null) {
		if (closure)
			createGraphsTask.configure(closure)
		return createGraphsTask
	}

	public def getCreateGraphs() {
		return createGraphsTask
	}

	public def prepareCodeGeneration(closure=null) {
		if (closure)
			prepareCodeGenerationTask.configure(closure)
		return prepareCodeGenerationTask
	}

	public def getPrepareCodeGeneration() {
		return prepareCodeGenerationTask
	}

	public def partition(closure=null) {
		if (closure)
			partitioningTask.configure(closure)
		return partitioningTask
	}

	public def getPartition() {
		return partitioningTask
	}

	public def compileResult(closure=null) {
		if (closure)
			compilePartitioningResultTask.configure(closure)
		return compilePartitioningResultTask
	}

	public def getCompileResult() {
		return compilePartitioningResultTask
	}

	public def testResult(closure=null) {
		if (closure)
			testPartitioningResultTask.configure(closure)
		return testPartitioningResultTask
	}

	public def getTestResult() {
		return testPartitioningResultTask
	}

	private void configureTasks() {

	}
	
}
