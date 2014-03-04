import org.gradle.api.DefaultTask
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input

public class FetchAndExtractSourceTask extends DefaultTask {
	private String configurationName = null
	private Object targetDir = null
	private LazyValue tarFile = null

	public def artifact(dependencyNotation) {
		assert configurationName == null, "Please call artifact only once."

		this.configurationName = "feast_" + this.name
		def configuration = project.configurations.create configurationName
		project.dependencies.add configurationName, dependencyNotation

		this.tarFile = project.lazyValue {
			def sourceFiles = configuration.resolvedConfiguration.resolvedArtifacts.collect { artifact ->
				artifact.file
			}
			assert sourceFiles.size == 1, "There must be exactly one source file for ${configurationName} configuration!"

			sourceFiles[0]
		}
	}

	public def into(targetDir) {
		this.targetDir = targetDir

		def filesInTar = project.lazyValue {
			// '--strip-components=1' doesn't work here, so we remove the first component ourselves
			["tar", "-t${compressionFlag}f", tarFile].execute().text.readLines()*.split("/").collect { segments ->
				def strippedSegments = [project.file(targetDir)] + (segments.size() > 1 ? segments[1..-1] : [])
				project.path(*strippedSegments)
			}
		}

		inputs.files { tarFile.toString() }
		outputs.files { filesInTar.value }
	}

	public File getTargetDir() {
		assert targetDir != null
		return project.file(targetDir)
	}

	public File getTarFile() {
		assert tarFile != null
		return tarFile.value
	}

	public String getCompressionFlag() {
		if (tarFile ==~ /.*\.tar\.gz$|\.tgz$/)
			return "z"
		else if (tarFile ==~ /.*\.tar\.bz2$/)
			return "j"
		else if (tarFile ==~ /.*\.tar$/)
			return ""
		else {
			project.logger.warn("Couldn't determine compression of '$tarFile'. You should use the default extension, e.g. something.tar.gz")
			return "z"
		}
	}

	@TaskAction
	def extract() {
		project.file(targetDir).mkdirs()

		project.exec {
			commandLine "tar", "-C", project.file(this.targetDir), "--strip-components=1", "-x${compressionFlag}f", tarFile
		}
	}
}
