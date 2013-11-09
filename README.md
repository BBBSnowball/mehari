TODO: write a really good description... ;-)

Quickstart
==========

Get the code
------------

You need `git` to get the code. Please install it: `aptitude install git` or whatever is appropriate for your distribution. On Windows, you should install [`git`](http://git-scm.com/download/win) and [TortoiseGit](http://code.google.com/p/tortoisegit/downloads/list).

````
# master repository
git clone git@github.com:BBBSnowball/mehari.git mehari

# LLVM and some modules
git clone git@github.com:BBBSnowball/mehari-llvm.git             mehari/llvm
git clone git@github.com:BBBSnowball/mehari-llvm-compiler-rt.git mehari/llvm/projects/compiler-rt
git clone git@github.com:BBBSnowball/mehari-llvm-test-suite.git  mehari/llvm/projects/test-suite
git clone git@github.com:BBBSnowball/mehari-llvm-clang.git       mehari/llvm/tools/clang
( cd mehari/llvm                      ; git remote add llvm http://llvm.org/git/llvm            )
( cd mehari/llvm/projects/compiler-rt ; git remote add llvm http://llvm.org/git/compiler-rt.git )
( cd mehari/llvm/projects/test-suite  ; git remote add llvm http://llvm.org/git/test-suite.git  )
( cd mehari/llvm/tools/clang          ; git remote add llvm http://llvm.org/git/clang.git       )

# private repository
# ONLY DO THIS, IF YOU HAVE ACCESS TO THIS REPOSITORY
# You can go on without this. If you have any problems, please tell us.
#TODO add private repo
git clone TODO mehari/private
````


### Why don't you use submodules? I think this would be easier.

It would be easier, if you always clone all repositories. We don't use a submodule for the private repository because you may not have access to it. We don't want you to be annoyed by error messages. We don't use submodules for the LLVM modules because the LLVM git doesn't do that. I think they want everyone to only clone the modules they need.

TODO: I think we will want to track which version in the master repository goes with which version in the modules. If we use submodules, we get this for free. If we don't...


### Why do you have a private git?

Most examples and the libraries they use are IP of iXtronics GmbH. Most of them are based on code that is used for real mechatronic systems or demonstration systems. We won't publish them without their permission. We have prepared some examples that don't expose an confidential information. Although they are not as extensive as the private examples, you can use them to test the software and do simple performance measurements.


### Why do I need git? Can't you provide tar balls like everyone else?

Mehari is geared to developers. If you use git, you can try different versions and record your own changes. If you prefer a different version control system, you can convert the git (e.g. [to mercurial](http://mercurial.selenic.com/wiki/ConvertExtension)).

If you only want the code once (i.e. you don't want updates or make any changes), you can download ZIP files on the repository web sites (e.g. for the [mehari repository](https://github.com/bbbsnowball/mehari)). We won't cover this - you will be on your own. I heartily recommend that you use `git`.


Do something useful
-------------------

We wish there was something to write about. As of now, the repository is empty, so there is not much we can tell you. If the repository has more than a `README.md` file when you are reading this, please we forgot to update this guide. Please give us a good kick, so we won't be so careless in the future.
