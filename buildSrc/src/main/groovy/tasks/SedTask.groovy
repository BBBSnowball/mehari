import org.gradle.api.DefaultTask
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.Input

public class SedTask extends DefaultTask {
	private Object input = null, output = null
	private ArrayList<Object> expressions = []

	public void expression(...expressions) {
		this.expressions.addAll(expressions)
	}

	public void workOn(file) {
		workOn(file, file)
	}

	private boolean isSameFile(file1, file2) {
		return project.file(file1).canonicalPath == project.file(file2).canonicalPath
	}

	private boolean isInputSameAsOutput() {
		return isSameFile(this.input, this.output)
	}

	public void workOn(input, output) {
		assert this.input == null && this.output == null, "You must call workOn exactly once."

		this.input = input
		this.output = output
		
		def inplaceMode = isInputSameAsOutput()
		this.inputs.files {
			if (!inplaceMode)
				[project.file(input)]
			else
				[]
		}
		this.outputs.file output
	}

	@Input
	public List<String> getExpressions() {
		return project.stringList(this.expressions)
	}

	@TaskAction
	void process() {
		assert input != null && output != null, "Did you forget to call workOn?"

		def exprs = getExpressions()
		if (exprs.size > 0) {
			if (exprs.size > 1) {
				exprs = exprs.collect { e -> "-e" + e }
			}

			def inplaceMode = isInputSameAsOutput()

			def cmd = ["sed"] + (inplaceMode ? ["-i"] : "") + exprs + [project.file(input)]

			project.exec {
				commandLine cmd

				if (!inplaceMode)
					standardOutput = project.file(this.output).newOutputStream()
			}
		} else {
			logger.warn("SedTask without expressions won't change the file: $this")

			if (!isInputSameAsOutput()) {
				project.file(this.output).write(project.file(this.input).text)
			}
		}
	}
}
