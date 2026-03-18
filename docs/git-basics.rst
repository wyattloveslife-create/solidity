####################
Basic Git Commands
####################

Git is a distributed version control system widely used in software development.
This page provides a concise reference for the most commonly used Git commands,
along with examples to help you get started.

.. _git-init:

``git init``
============

Initialises a new, empty Git repository in the current directory.
After running this command, a hidden ``.git`` folder is created that tracks all
version history for the project.

.. code-block:: bash

    # Create a new directory and initialise a repository inside it
    mkdir my-project
    cd my-project
    git init

    # Alternatively, pass a directory name to create it automatically
    git init my-project

.. _git-clone:

``git clone``
=============

Creates a local copy of an existing remote repository, including its full
history.

.. code-block:: bash

    # Clone a repository from GitHub
    git clone https://github.com/ethereum/solidity.git

    # Clone into a custom local directory name
    git clone https://github.com/ethereum/solidity.git my-solidity

.. _git-status:

``git status``
==============

Shows the current state of the working directory and staging area: which files
have been modified, which are staged for the next commit, and which are
untracked.

.. code-block:: bash

    git status

    # Example output:
    # On branch main
    # Changes not staged for commit:
    #   modified:   README.md
    # Untracked files:
    #   new-file.sol

.. _git-add:

``git add``
===========

Stages file changes so that they are included in the next commit.

.. code-block:: bash

    # Stage a single file
    git add README.md

    # Stage all changes in the current directory (recursively)
    git add .

    # Stage specific changes interactively
    git add -p README.md

.. _git-commit:

``git commit``
==============

Records the staged changes as a new commit in the repository history.
Every commit requires a message that describes *what* changed and *why*.

.. code-block:: bash

    # Commit with an inline message
    git commit -m "Add initial README"

    # Stage all tracked files and commit in one step
    git commit -am "Fix typo in README"

    # Open the default editor to write a multi-line commit message
    git commit

.. _git-push:

``git push``
============

Uploads local commits to a remote repository, making them available to other
contributors.

.. code-block:: bash

    # Push the current branch to its upstream remote
    git push

    # Push a specific branch to a remote called 'origin'
    git push origin main

    # Push and set the upstream tracking branch at the same time
    git push -u origin feature/my-feature

.. _git-pull:

``git pull``
============

Fetches changes from the remote repository and immediately merges them into the
current local branch.  It is equivalent to running ``git fetch`` followed by
``git merge``.

.. code-block:: bash

    # Pull changes from the tracked upstream branch
    git pull

    # Pull from a specific remote and branch
    git pull origin main

    # Pull using rebase instead of merge (keeps a cleaner history)
    git pull --rebase origin main
