import org.gradle.api.Project
import org.gradle.api.Plugin
import org.gradle.api.GradleException
import java.util.regex.Pattern
import org.gradle.api.tasks.Exec

class HelpersPluginConvention {
	private Project project

	public final String pythonInstallPath

	public HelpersPluginConvention(Project project) {
		this.project = project
		this.pythonInstallPath = project.file(project.rootDir.toString() + "/tools/_install").absolutePath

		this.addExtensions()
	}

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

	def addMethodForSomeTasks(methodName, conditionClosure, methodClosure) {
		// executed for all current and future tasks: add a method using task.ext
		project.tasks.all { task ->
			if (conditionClosure(task))
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

	def sh(cmd, shell="sh") {
		logger.info(shell + ": " + cmd.inspect())
		def p = [shell, "-c", cmd].execute()
		p.waitForProcessOutput(System.out, System.err)
		def res = p.waitFor()
		if (res != 0)
			throw new GradleException("System command returned non-zero status (exit status is $res): " + cmd.inspect())
	}

	def sh_test(cmd, shell="sh") {
		logger.info(shell + "?: " + cmd.inspect())
		def p = [shell, "-c", cmd].execute()
		p.waitForProcessOutput(System.out, System.err)
		def res = p.waitFor()
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
		def res = process.waitFor()
		if (res == 0)
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

	def escapeOneForShell(arg) {
		if (arg instanceof ShellEscaped)
			return arg
		else {
			arg = arg.toString().replaceAll(/([\\'"$])/, /\\$1/)
			return new ShellEscaped('"' + arg + '"')
		}
	}

	def escapeForShell(args) {
		if (args != null && args.class.isArray())
			args = args.toList()
		if (args instanceof Collection)
			return args.collect(this.&escapeOneForShell).join(" ")
		else
			return escapeOneForShell(args)
	}

	def escapeForRegex(text) {
		for (special_char in "\\[].*")
			text = text.replace(special_char, "\\" + special_char)
		return text
	}

	def copyTree(src, dst) {
		src = escapeForShell(new File(src, "."))
		dst = escapeForShell(dst)
		sh("cp -a $src $dst")
	}

	def copyFile(src, dst) {
		src = escapeForShell(src)
		dst = escapeForShell(dst)
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

	def userHomeDir() {
		return System.getenv()["HOME"]
	}

	def useDefaultIdentityFile(it) {
		if (project.hasProperty("ssh_identity"))
			it.identity = project.file((project.ssh_identity =~ /^~/).replaceFirst(userHomeDir()))
		else {
			def ssh_identity = path(userHomeDir(), ".ssh", "id_rsa")
			if (ssh_identity.exists())
				it.identity = ssh_identity
			else {
				logger.info("I won't add '~/.ssh/id_rsa' to $it because I cannot find it.")
			}
		}
	}

	def lazyValue(closure) {
		return new LazyValue(closure)
	}

	def symlink(target, link) {
		target = FileLinkAction.getRelativePath(project.file(target), project.file(link).parentFile)
		sh("ln -s ${escapeForShell(target)} ${escapeForShell(link)}")
	}

	def touch(file) {
		file.withWriterAppend { out -> out.write("") }
	}

	def chmod(file, permissions) {
		permissions = String.format("0%o", permissions)
		sh("chmod $permissions ${escapeForShell(file)}")
	}

	def doesFileBelongToRoot(file) {
		return backticks("stat -c %u ~/.cse").trim() == "0"
	}


	private def addExtensions() {
		addMethodForSomeTasks("environmentFromConfig", { task-> task instanceof Exec }) { task ->
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

		addMethodForSomeTasks("prependPATH", { task-> task instanceof Exec }) { task, ...paths ->
			def oldPath = System.getenv()["PATH"]
			task.environment "PATH", paths.collect(project.&file).findAll().join(File.pathSeparator) \
				+ File.pathSeparator + oldPath
		}
	}
}

public class HelpersPlugin implements Plugin<Project> {
	void apply(Project project) {
        project.convention.plugins.put('helpers', new HelpersPluginConvention(project))
	}
}
