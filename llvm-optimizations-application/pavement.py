#!/usr/bin/env python

if __name__ == '__main__':
    import sys

    # TODO is there a better way than using unipath until getting path.py ?
    from unipath import Path

    # make sure we use our extended paver and path.py
    ROOT = Path(__file__).absolute().parent
    sys.path.insert(1, Path(ROOT.parent, "tools", "paver"))    
    sys.path.insert(1, Path(ROOT.parent, "tools", "paver", "paver"))
    
    from path import path

    import paver.tasks
    sys.exit(paver.tasks.main())



import sys, os
from paver.easy import task, needs, cmdopts
import paver
from path import path

import logging


# -----------------------------------------------


ROOT        = path(__file__).abspath().parent
SRC_DIR     = path.joinpath(ROOT, "src")
NEW_SRC_DIR = path.joinpath(ROOT, "src_new")
IR_DIR      = path.joinpath(ROOT, "ir")
GRAPH_DIR   = path.joinpath(ROOT, "graph")

MEHARI_ROOT  = ROOT.parent
PRIVATE_ROOT = path.joinpath(MEHARI_ROOT, "private")

IPANEMA     = path.joinpath(PRIVATE_ROOT, "ipanema")

IPANEMA_INCLUDES_SYSLAYER = path.joinpath(IPANEMA, "include", "syslayer", "linux")
IPANEMA_INCLUDES_SIM      = path.joinpath(IPANEMA, "include", "sim")

# add private repository to python path to get helper scripts
sys.path.append(PRIVATE_ROOT)

LLVM = path.joinpath(MEHARI_ROOT, "llvm")

# TODO set llvm build configuration by command line arguement
llvm_build_debug = True

if llvm_build_debug:
    LLVM_CONFIG_OPTIONS="--disable-optimized"
    LLVM_BUILD = path.joinpath(LLVM, "Debug+Asserts")
else:
    LLVM_CONFIG_OPTIONS="--enable-optimized"
    LLVM_BUILD = path.joinpath(LLVM, "Release+Asserts")

LLVM_BIN = path.joinpath(LLVM_BUILD, "bin") 
LLVM_LIB = path.joinpath(LLVM_BUILD, "lib")

LLVM_CUSTOM_PASSES = path.joinpath(LLVM, "lib", "Transforms", "Mehari")


# TODO get example source by command line argument
# EXAMPLE = path.joinpath(SRC_DIR, "single_pendulum.c")
EXAMPLE = path.joinpath(SRC_DIR, "dc_motor.c")


MAKE_PARALLEL_PROCESSES = 4

CFLAGS_FOR_EXAMPLE = "-DDOUBLE_FLOAT -D__POSIX__ -D_DSC_EVENTQUEUE -D__RESET -DEXEC_SEQ"


# -----------------------------------------------


logger = logging.getLogger("pavement")


def heredoc(doc):
    """remove indent from multi-line string"""
    doc = re.sub("^\n*", "", doc)
    indent = re.match("^(\s*)", doc).group(0)
    if indent:
        doc = re.sub("^"+indent, "", doc)
    return doc

def sh(cmd):
    res = os.system(cmd)
    if res != 0:
        raise IOError("System command returned non-zero status (return value is %d): %r"
            % (res, cmd))


@task
def check_submodules():
    for source_dir in ["llvm", "tools", "private"]:
        if not path.isdir(path.joinpath(MEHARI_ROOT, source_dir)):
            logger.error(heredoc("""
                I need several gits with source code. At least '%s' is missing.
                  If you have received this as part of a git, init and update the submodules
                    git submodule init {llvm,tools,private}
                    git submodule update
                """ % source_dir))
            sys.exit(1)

@task
@needs("check_submodules")
def check():
    pass


@task
def clean_results():
    path.rmtree(IR_DIR)
    path.rmtree(GRAPH_DIR)
    path.rmtree(NEW_SRC_DIR)


def clean_repository(dir):
    if not path.joinpath(dir, ".git").exists():
        raise IOError("'%s' is not the root of a git repository" % dir)
    sh('cd "%s" && git reset --hard && git clean -f -x -d' % dir)


@task
@needs("check")
def clean_llvm():
    for git_dir in [".", "tools/clang", "projects/compiler-rt", "projects/test-suite"]:
        clean_repository(path.joinpath(LLVM, git_dir))


@task
@needs("check")
def configure_llvm():
    path(LLVM).chdir()
    sh("./configure %s" % LLVM_CONFIG_OPTIONS)


def check_configuration_required(path):
    if not path.joinpath(path, "config.status").exists():
        return True


def gmake_parallel():
    global MAKE_PARALLEL_PROCESSES
    sh("gmake -j%s " % MAKE_PARALLEL_PROCESSES)


@task
@needs("check")
def build_llvm():
    if check_configuration_required(LLVM):
        configure_llvm()
    path(LLVM).chdir()
    gmake_parallel()


@task
@needs("check")
def build_custom_passes():
    if check_configuration_required(LLVM):
        configure_llvm()
    for pass_dir in path(LLVM_CUSTOM_PASSES).listdir():
        path(pass_dir).chdir()
        gmake_parallel()


@task
@needs("clean_results")
@needs("clean_llvm")
def clean():
    pass


@task
@needs("build_llvm")
@needs("build_custom_passes")
def build():
    pass


@task
@needs("clean")
@needs("build")
def clean_build():
    pass


# -----------------------------------------------


@task
@needs("check")
def prepare_example():
    from prepare_example_source import remove_redundant_func_calls, extract_calculations

    path.makedirs_p(NEW_SRC_DIR)
    new_source_file = path.joinpath(NEW_SRC_DIR, ("%s_new.c" % EXAMPLE.namebase))
    path.remove_p(new_source_file)

    remove_redundant_func_calls(EXAMPLE, new_source_file)
    extract_calculations(new_source_file, NEW_SRC_DIR)


def create_ir(source_file):
    global LLVM_BIN, CFLAGS_FOR_EXAMPLE, IPANEMA_INCLUDES_SYSLAYER, IPANEMA_INCLUDES_SIM
    ir_file = path.joinpath(IR_DIR, ("%s.ll" % source_file.namebase))
    sh("%s %s -S -emit-llvm -I%s -I%s %s -o %s" % 
        (path.joinpath(LLVM_BIN, "clang"), CFLAGS_FOR_EXAMPLE, IPANEMA_INCLUDES_SYSLAYER, IPANEMA_INCLUDES_SIM, source_file, ir_file))

@task
@needs("build_llvm")
@needs("prepare_example")
def create_ir_files():
    path.makedirs_p(IR_DIR)
    for cfile in filter(lambda file: file.endswith(".c"), NEW_SRC_DIR.files()):
        create_ir(cfile)


def create_callgraph(ir_file):
    global LLVM_BIN
    pdf_file = path.joinpath(GRAPH_DIR, ("%s.pdf" % ir_file.namebase))
    sh("%s -analyze -dot-callgraph %s > /dev/null" % (path.joinpath(LLVM_BIN, "opt"), ir_file))
    sh("dot -Tpdf callgraph.dot -o %s" % pdf_file)

@task
@needs("create_ir_files")
def create_callgraphs():
    path.makedirs_p(GRAPH_DIR)
    for ir_file in IR_DIR.files():
        create_callgraph(ir_file)
