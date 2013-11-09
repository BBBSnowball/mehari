TODO: write a really good description... ;-)

Quickstart
==========

Get the code
------------

You need `git` to get the code. Please install it: `aptitude install git` or whatever is appropriate for your distribution. On Windows, you should install [`git`](http://git-scm.com/download/win) and [TortoiseGit](http://code.google.com/p/tortoisegit/downloads/list).

Some part of our examples are in a private git. If you do have access to this git, you can `git init` all submodules. If you don't have access to it, don't worry. You don't need it to work with Mehari.

````
# master repository
git clone git@github.com:BBBSnowball/mehari.git mehari

# clone submodules
# If you do have access to the private git, add 'private' at the end of this command.
cd mehari
git submodule init llvm
git submodule update
cd ..

# clone submodules of LLVM
cd mehari/llvm
git submodule init
git submodule update
cd ../..

# add original origin for LLVM gits
# (Later, you can use `git fetch llvm && git merge llvm/master` to get upstream updates.)
( cd mehari/llvm                      ; git remote add llvm http://llvm.org/git/llvm            )
( cd mehari/llvm/projects/compiler-rt ; git remote add llvm http://llvm.org/git/compiler-rt.git )
( cd mehari/llvm/projects/test-suite  ; git remote add llvm http://llvm.org/git/test-suite.git  )
( cd mehari/llvm/tools/clang          ; git remote add llvm http://llvm.org/git/clang.git       )
````


### Why do you use submodules?

We need more than one git for several reasons:

- The LLVM source code is split into several repositories. We copy that structure, so we can easily merge updates and send patches.
- We are not allowed to publish part of the examples, so we cannot put them into the public repository.

If you don't want to (or cannot) clone all the submodules, you must be careful not to `git init` them. In that case, git will simply ignore them (tested with git 1.7.9.5 on Ubuntu 12.04). This means you shouldn't run `git init` without any arguments, as this would initialize all submodules. We are very sorry for the inconvenience that may cause. However, there is an important advantage of submodules: Git will track the versions of them, so you always get a consistent state. Without this information, building and testing the program would involve a lot of guesswork.

If you ever `git init` a submodule by accident, please try this procedure: `git stash` to save changes in `.gitmodules` (if there are any), completely remove the submodule (e.g. according to [this guide](http://davidwalsh.name/git-remove-submodule)) but DO NOT COMMIT, `git checkout HEAD .gitmodules` to remove any changes in the `.gitmodules` files, `git stash pop` to restore your changes that were saved by `git stash`.


### Why do you have a private git?

Most examples and the libraries they use are IP of iXtronics GmbH. Most of them are based on code that is used for real mechatronic systems or demonstration systems. We won't publish them without their permission. We have prepared some examples that don't expose an confidential information. Although they are not as extensive as the private examples, you can use them to test the software and do simple performance measurements.


### Why do I need git? Can't you provide tar balls like everyone else?

Mehari is geared to developers. If you use git, you can try different versions and record your own changes. If you prefer a different version control system, you can convert the git (e.g. [to mercurial](http://mercurial.selenic.com/wiki/ConvertExtension)).

If you only want the code once (i.e. you don't want updates or make any changes), you can download ZIP files on the repository web sites (e.g. for the [mehari repository](https://github.com/bbbsnowball/mehari)). We won't cover this - you will be on your own. I heartily recommend that you use `git`.


Do something useful
-------------------

We wish there was something to write about. As of now, the repository is empty, so there is not much we can tell you. If the repository has more than a `README.md` file when you are reading this, please we forgot to update this guide. Please give us a good kick, so we won't be so careless in the future.
