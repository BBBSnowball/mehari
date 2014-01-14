import os, re, subprocess, logging
from unipath import Path

logger = logging.getLogger(__name__)


def from_env(name, default = None):
    """Get value from an environment variable or use the default.

    You should usually use command line options instead of environment variables!
    """
    if name in os.environ:
        return os.environ[name]
    else:
        return default

def heredoc(doc):
    """remove indent from multi-line string"""
    doc = re.sub("^\n*", "", doc)
    indent = re.match("^(\s*)", doc).group(0)
    if indent:
        doc = re.sub("^"+indent, "", doc)
    #doc = re.sub("\n*$", "", doc)
    return doc

def sh(cmd):
    logger.info("sh: %r" % (cmd,))
    res = os.system(cmd)
    if res != 0:
        #TODO raise paver.tasks.BuildFailure?
        raise IOError("System command returned non-zero status (return value is %d): %r"
            % (res, cmd))

def sh_test(cmd):
    logger.info("sh?: %r" % (cmd,))
    res = os.system(cmd)
    logger.debug("  -> %s" % res)
    return res

class changed_environment(object):
    __slots__ = "vars", "_old_values"

    def __init__(self, **changed_vars):
        self.vars = changed_vars

    @staticmethod
    def change_environment(values):
        for key in values:
            if values[key] is not None:
                os.environ[key] = values[key]
            else:
                del os.environ[key]

    @staticmethod
    def save_environment(keys):
        saved = {}
        for key in keys:
            if key in os.environ:
                saved[key] = os.environ[key]
            else:
                saved[key] = None
        return saved

    def __enter__(self):
        self._old_values = self.save_environment(self.vars.keys())
        self.change_environment(self.vars)
        return None

    def __exit__(self, *exc):
        self.change_environment(self._old_values)

def backticks(cmd):
    shell = isinstance(cmd, str)
    proc = subprocess.Popen(cmd, shell=shell, stdout=subprocess.PIPE)
    (out, err) = proc.communicate()
    if proc.returncode == 0:
        return out
    else:
        raise IOError("System command returned an error (return code is %d): %r"
            % (proc.returncode, cmd))

class ShellEscaped(str):
    def __mod__(self, arg):
        if isinstance(arg, tuple):
            arg = tuple(map(escape_for_shell, arg))
        else:
            arg = escape_for_shell(arg)

        return ShellEscaped(super(ShellEscaped, self).__mod__(arg))

def escape_one_for_shell(arg):
    if isinstance(arg, ShellEscaped):
        return arg
    else:
        arg = re.sub("([\\\\'\"$])", "\\\\\\1", arg)
        return ShellEscaped('"' + arg + '"')

def escape_for_shell(args):
    if isinstance(args, (tuple, list)):
        return " ".join(map(escape_one_for_shell, args))
    else:
        return escape_one_for_shell(args)

def escape_for_regex(text):
    for special_char in "\\[].*":
        text = text.replace(special_char, "\\" + special_char)
    return text

def source_shell_file(file):
    #with changed_environment(BASH_SOURCE=file):
    shell_cmd = ShellEscaped(". %s && env") % file
    env = backticks(["bash", "-c", shell_cmd])
    for line in env.split("\n"):
        if line.find("=") < 0:
            continue
        key, value = line.split("=", 1)
        if key in ["SHLVL", "_"]:
            # this is a special variable -> ignore
            continue
        if key not in os.environ or os.environ[key] != value:
            logger.debug("changed or new env var: %s=%s" % (key, value))
        os.environ[key] = value

def touch(file):
    if not Path(file).exists():
        Path(file).write_file("")

def copy_tree(src, dst):
    sh("cp -a %s %s" % (Path(src, "."), dst))

def append_file(file, text):
    text = file.read_file() + text
    file.write_file(text)
