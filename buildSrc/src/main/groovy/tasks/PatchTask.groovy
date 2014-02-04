import org.gradle.api.DefaultTask
import org.gradle.api.tasks.TaskAction

public class PatchTask extends DefaultTask {
	private Object patchFile = null
	private String patchType = null
	private Object dir = null
	private List<String> patchedPathsCache = null

	public void patch(patchFile, type="-p1") {
		assert this.patchFile == null, "You can only use patch once per task."
		assert type ==~ /^-p[0-9]+$/, "Invalid or unsupported patch type"
		this.patchFile = patchFile
		this.patchType = type


		inputs.file { project.file(patchFile) }
		inputs.property "patchType", type
		outputs.files { this.patchedFiles }
	}

	public void workingDir(dir) {
		this.dir = dir
	}

	public void resetWithGit() {
		// reset patched files before patching, so the patch won't fail, if we run the task again
		this.doFirst {
			def dir = this.workingDir
			project.exec {
				commandLine("git", "checkout", *patchedPaths)
				workingDir dir
			}
		}
	}

	public File getWorkingDir() {
		return project.file(this.dir)
	}

	public Collection<String> getPatchedPaths() {
		assert this.patchFile != null, "Call patch(...) before accessing patchedFiles."

		if (patchedPathsCache == null) {
			def skipPathSegments = patchType[2..-1].toInteger()

			def patchedPaths = []
			def patchFirstLine = null
			project.file(patchFile).eachLine { line, lineNumber ->
				if (line.startsWith("--- ")) {
					patchFirstLine = line[4..-1]
				} else if (line.startsWith("+++ ") && patchFirstLine) {
					def patchSecondLine = line[4..-1]

					def path1 = patchFirstLine.split("/")[skipPathSegments..-1]
					def path2 = patchSecondLine.split("/")[skipPathSegments..-1]
					if (path1 != path2) {
						if (patchFirstLine == "/dev/null")
							path1 = path2
						else if (patchSecondLine == "/dev/null")
							path2 = path1
						else {
							logger.error("Inconsistent paths in patch file: ${path1.join("/")} vs. ${path2.join("/")}")

							// patch uses the first one - so do we
							path2 = path1
						}
					}

					def path = path1.join(File.separator)
					patchedPaths.add(path)
				} else
					patchFirstLine = null
			}
			patchedPathsCache = patchedPaths.asImmutable()
		}

		return patchedPathsCache
	}

	public List<File> getPatchedFiles() {
		def baseDir = project.file(this.dir)
		return getPatchedPaths().collect { path -> new File(baseDir, path) }
	}

	@TaskAction
	void doPatch() {
		assert patchFile, "Did you forget to call patch?"
		assert dir, "Did you forget to call workingDir?"

		def cmd = ["patch", "-N", patchType]
		def baseDir = project.file(dir)
		def input = patchFile.newInputStream()

		project.exec {
			commandLine cmd
			workingDir baseDir
			standardInput = input
		}
	}
}
