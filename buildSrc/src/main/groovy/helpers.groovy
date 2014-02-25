import org.gradle.api.Project
import org.gradle.api.Plugin
import org.gradle.api.GradleException
import java.util.regex.Pattern
import org.gradle.api.tasks.Exec
import java.util.concurrent.Callable

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

	def filePropertyOrDefault(propertyName, defaultValue) {
		return project.file(propertyOrDefault(propertyName, defaultValue));
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

	def relativePathTo(file, relativeTo) {
		return FileLinkAction.getRelativePath(file, relativeTo)
	}

	def symlink(target, link) {
		target = relativePathTo(project.file(target), project.file(link).parentFile)
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

	def nestedStringList(item) {
		if (item instanceof Callable)
			return item()
		else if (item instanceof Collection || item != null && item.class.isArray())
			return item.toList().collect { item2 ->
				nestedStringList(item2)
			}
		else if (item instanceof LazyValue)
			return nestedStringList(item.value)
		else
			return item.toString()
	}

	def stringList(list) {
		list = nestedStringList(list)
		if (!(list instanceof Collection))
			return [list]
		else
			return list.flatten()
	}

	def eachDirRecurseIncludingSelf(directory, closure) {
		closure(directory)
		directory.eachDirRecurse(closure)
	}

	def filesInGit(directory) {
		// I would use a fileTree to find .gitignore files, but fileTree seems to ignore hidden files.
		// def gitignoreFiles = fileTree(directory).include("**/.gitignore")
		directory = project.file(directory)
		return project.fileTree(directory) {
			eachDirRecurseIncludingSelf(directory) { subdir ->
				def gitignoreFile = new File(subdir, ".gitignore")
				if (gitignoreFile.exists()) {
					//println(gitignoreFile)
					def gitignorePath = (subdir.getPath() + "/").substring(directory.getPath().size()+1)
					gitignoreFile.eachLine { line ->
						if (line ==~ /^\s*#.*/ || line ==~ /^\s*$/) {
							// ignore
						} else {
							line = line.trim()
							String excludePattern
							if (line.startsWith("/")) {
								excludePattern = gitignorePath + line.substring(1)
							} else {
								excludePattern = gitignorePath + "**/" + line
							}
							//println("exclude: " + excludePattern)
							exclude excludePattern
						}
					}
				}
			}
		}
	}

	String toCamelCase(String s) {
		return s.replaceAll(/_[A-Za-z0-9]/) { it[-1].toUpperCase() }
	}

	String toPascalCase(String s) {
		return toCamelCase(s).capitalize()
	}

	def cleanGitRepository(dir) {
		if (!rootPath(dir, ".git").exists())
			throw new GradleException("'$dir' is not the root of a git repository")
		project.exec {
			commandLine "sh", "-c", "git reset --hard && git clean -f -x -d"
			workingDir rootPath(dir)
		}
	}

	def cleanWithDistclean(dir) {
		dir = escapeForShell(project.file(dir))
		sh("cd $dir ; [ -e \"Makefile\" ] && make distclean || true")
	}

	def getVariableFromShell(String name) {
		return backticks(". ${reconosConfigScriptEscaped} >/dev/null ; echo -n \"\$$name\"", "bash")
	}

	def getVariablesFromShell(cmd, variablenames) {
		variablenames.each { varname ->
			cmd += " && echo \"${varname}=\$${varname}\""
		}
		def values = [:]
		backticks(cmd, "bash").split("\n")*.split("=", 2).each { pair ->
			values[pair[0]] = pair[1]
		}
		return values
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

		// We cannot do this in CrossCompileMakeTask because we must wait until the properties plugin
		// has been applied. Fortunately, the helpers plugin is applied after the properties plugin.
		project.tasks.all { task ->
			if (task instanceof CrossCompileMakeTask)
				task.recommendedProperty "parallel_compilation_processes"
		}
	}
}

public class HelpersPlugin implements Plugin<Project> {
	void apply(Project project) {
        project.convention.plugins.put('helpers', new HelpersPluginConvention(project))
	}
}
