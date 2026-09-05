---
layout: default
title: Git Workflow
---

This document describes the typical git workflow used by developers of this
project: how we branch off `master`, open pull requests, keep a clean history
when CI fails, and rebase topic branches that have diverged from `master`.

## Starting a Topic Branch

Before starting new work, make sure your local `master` is up to date with
upstream, then create a topic branch to work on.

```shell
# Sync master with upstream.
git checkout master
git fetch
git merge
# Create and switch to a topic branch.
git branch -m topic-name
```

Do your work on `topic-name`, committing as you go.

## Opening a Pull Request

Push the topic branch to GitHub to open a pull request. The `-u` flag sets the
upstream tracking reference so that subsequent `git push`/`git pull` invocations
know which remote branch to use.

```shell
git push -u origin topic-name
```

## Handling CI Failures

If CI fails, we address the issue and then fold the correction back into our
history so that the mistake does not appear as a separate commit. First fix the
problem and commit the fix, then use an interactive rebase to relabel the fix
commit and force-push the cleaned-up branch.

```shell
# Address the issue, then commit the fix.
git commit -a -m "Fix CI failure"
# Interactively rebase the last two commits.
git rebase -i HEAD~2
```

In the interactive rebase editor, mark the fix commit as a fixup so it is folded
into the preceding commit, hiding the mistake from the visible history. For
example, change the second line's command from `pick` to `fixup` (or `f`):

```
pick   a1b2c3d Implement topic-name feature
fixup  d4e5f6a Fix CI failure
```

Save and close the editor, then force-push the rewritten branch:

```shell
git push -u origin topic-name --force
```

## Rebasing a Diverged Branch

When maintaining a topic branch that has diverged from `master`, we prefer
rebasing over merging. First sync `master` with upstream, then check out the
topic branch and rebase it onto the updated `master`.

```shell
# Sync master with upstream.
git checkout master
git fetch
git merge
# Rebase the topic branch onto the updated master.
git checkout topic-name
git rebase master
# Update the pull request with the rebased history.
git push -u origin topic-name --force
```
