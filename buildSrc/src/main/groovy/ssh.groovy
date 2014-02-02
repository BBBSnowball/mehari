// provide an SshTask that uses the user's known_hosts file

import com.jcraft.jsch.JSch
import org.hidetake.gradle.ssh.api.SshService
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
}

public class MySshTask extends org.hidetake.gradle.ssh.SshTask {
	protected service = MySshService.instance
}
