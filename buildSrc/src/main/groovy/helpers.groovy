import org.gradle.api.Project
import org.gradle.api.Plugin
import org.gradle.api.GradleException
import java.util.regex.Pattern

class helpers {
	private Project project;

	private def getLogger() {
		return project.logger
	}

	def propertyOrDefault(propertyName, defaultValue) {
		return project.hasProperty(propertyName) ? project[propertyName] : defaultValue;
	}

	def defaultXilinxSettingsScript() {
		return "/opt/Xilinx/" + propertyOrDefault("xilinx_version", "14.6") + "/ISE_DS/settings64.sh"
	}

	def addMethodForAllTasks(methodName, methodClosure) {
		// executed for all current and future tasks: add a method using task.ext
		project.tasks.all { task ->
			task.ext[methodName] = methodClosure.curry(task)
		}
	}

	def forAllNormalTestTasks(closure) {
		// 'normal' means not for a tool
		project.allprojects {
			if (name != "tools") {
				tasks.all { task ->
					if (task.name.startsWith("test"))
						closure(task)
				}
			}
		}
	}

	/** remove indent from multi-line string */
	def heredoc(doc) {
		doc = doc.replaceFirst(/\A\n*/, "")
		def indent = (doc =~ /\A\s*/)[0]
		if (indent)
			doc = doc.replaceAll(Pattern.compile("^"+indent, Pattern.MULTILINE), "")
		//doc = doc.replaceFirst(/\n*\z/, "")
		return doc
	}

	def sh(cmd) {
		logger.info("sh: " + cmd.inspect())
		def res = ["sh", "-c", cmd].execute().waitFor()
		if (res != 0)
			throw new GradleException("System command returned non-zero status (exit status is $res): " + cmd.inspect())
	}

	def sh_test(cmd) {
		logger.info("sh?: " + cmd.inspect())
		def res = ["sh", "-c", cmd].execute().waitFor()
		logger.debug("  -> $res")
		return res
	}

	private def isString(obj) {
		return obj instanceof String || obj instanceof GString
	}

	def backticks(cmd, shell="sh") {
		def use_shell = isString(cmd)
		if (use_shell)
			cmd = [shell, "-c", cmd]
		def process = cmd.execute()
		if (process.waitFor() == 0)
			return process.text
		else
			throw new GradleException("System command returned non-zero status (exit status is $res): " + cmd.inspect())
	}

	class ShellEscaped {
		public String value;

		public ShellEscaped(String value) {
			this.value = value
		}

		public String toString() {
			return value;
		}
	}

	def escape_one_for_shell(arg) {
		if (arg instanceof ShellEscaped)
			return arg
		else {
			arg = arg.toString().replaceAll(/([\\'"$])/, /\\$1/)
			return new ShellEscaped('"' + arg + '"')
		}
	}

	def escapeForShell(args) {
		if (args instanceof Collection)
			return args.collect(escape_one_for_shell).join(" ")
		else
			return escape_one_for_shell(args)
	}

	def escape_for_regex(text) {
		for (special_char in "\\[].*")
			text = text.replace(special_char, "\\" + special_char)
		return text
	}

	def touch(file) {
		file = new File(file);
		if (!file.isFile()) {
			file.withWriter { out ->
				out.write("")
			}
		}
	}

	def copy_tree(src, dst) {
		src = escape_for_shell(new File(src, "."))
		dst = escape_for_shell(dst)
		sh("cp -a $src $dst")
	}

	def append_file(file, text) {
		new File(file).withWriterAppend { out ->
			out.write(text)
		}
	}

	def path(...segments) {
		return new File(segments*.toString().join(File.separator))
	}

	def rootPath(...segments) {
		return project.file(segments*.toString().join(File.separator))
	}


	def addExtensions() {
		for (method in this.metaClass.methods) {
			if (Object.metaClass.respondsTo(method.name) || method.name.startsWith("_")
					|| method.name.equals("getProject") || method.name.equals("addExtensions"))
				continue;
			project.extensions.extraProperties[method.name] = this.&"$method.name"
		}

		String pythonInstallPath = project.file(project.rootDir.toString() + "/tools/_install").absolutePath
		project.extensions.extraProperties.pythonInstallPath = pythonInstallPath

		addMethodForAllTasks("environmentFromConfig") { task ->
			task.recommendedProperties names: ["xilinx_version", "xilinx_settings_script"]
			task.environment "XILINX_VERSION",         propertyOrDefault("xilinx_version", "14.6")
			task.environment "XILINX_SETTINGS_SCRIPT", propertyOrDefault("xilinx_settings_script", defaultXilinxSettingsScript())
			task.environment "PYTHONPATH",             project.pythonInstallPath
		}

		addMethodForAllTasks("usesProperties") { task, String ...names ->
			names.each { name ->
				task.inputs.property(name) { hasProperty(name) ? project."$name" : null }
			}
		}
	}
}

public class HelpersPlugin implements Plugin<Project> {
	void apply(Project project) {
		def helpers_instance = new helpers(project: project);
		helpers_instance.addExtensions()
	}
}
