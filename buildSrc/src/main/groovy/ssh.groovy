// provide an SshTask that uses the user's known_hosts file

import com.jcraft.jsch.JSch
import org.hidetake.gradle.ssh.api.SshService
import org.hidetake.gradle.ssh.api.SshSpec
import org.gradle.util.ConfigureUtil

@Singleton
class MySshService implements SshService {
	protected SshService inner = org.hidetake.gradle.ssh.internal.DefaultSshService.instance

	private static String userHomeDir() {
		return System.getenv()["HOME"]
	}

	MySshService() {
		inner.setProperty("jschFactory") {
			def jsch = new JSch()
			def known_hosts = [userHomeDir(), ".ssh", "known_hosts"].join(File.separator)
			if (new File(known_hosts).exists())
				jsch.setKnownHosts(known_hosts)
			return jsch
		}
	}

	@Override
	void execute(org.hidetake.gradle.ssh.api.SshSpec sshSpec) {
		inner.execute(sshSpec)
	}

	public static void sshexec(Closure configureClosure) {
		assert configureClosure != null, 'configureClosure should be set'

		def localSpec = new SshSpec()
		ConfigureUtil.configure(configureClosure, localSpec)
		//def merged = SshSpec.computeMerged(localSpec, sshSpec)
		def merged = localSpec
		if (merged.dryRun) {
			org.hidetake.gradle.ssh.internal.DryRunSshService.instance.execute(merged)
		} else {
			merged.dryRun = false
			MySshService.instance.execute(merged)
		}
	}
}

public class MySshTask extends org.hidetake.gradle.ssh.SshTask {
	protected service = MySshService.instance
}
