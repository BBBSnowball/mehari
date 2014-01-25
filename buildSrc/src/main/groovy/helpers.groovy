import org.gradle.api.Project
import org.gradle.api.Plugin

class helpers {
	Project project;

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
}

public class HelpersPlugin implements Plugin<Project> {
    void apply(Project project) {
    	def helpers_instance = new helpers(project: project);
    	for (method in helpers_instance.metaClass.methods) {
    		if (Object.metaClass.respondsTo(method.name) || method.name.startsWith("_") || method.name.equals("getProject"))
    			continue;
    		project.extensions.extraProperties[method.name] = helpers_instance.&"$method.name"
    	}

		project.extensions.extraProperties.pythonInstallPath = project.file(project.rootDir.toString() + "/tools/_install").absolutePath

		helpers_instance.addMethodForAllTasks("environmentFromConfig") { task ->
			task.recommendedProperties names: ["xilinx_version", "xilinx_settings_script"]
			task.environment "XILINX_VERSION",         project.propertyOrDefault("xilinx_version", "14.6")
			task.environment "XILINX_SETTINGS_SCRIPT", project.propertyOrDefault("xilinx_settings_script", project.defaultXilinxSettingsScript())
			task.environment "PYTHONPATH",             project.pythonInstallPath
		}
    }
}
