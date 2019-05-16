
RRND Test Suite
===============

This Python 3 scripts allow testing of the RRND neighbor discovery component.
They connect with `rrnd_proxy` to support both local and remote (on-device)
testing. See the `rrnd_proxy` documentation for further information.

Running Tests
-------------

To be able to execute `rrnd_test` first you have to install all dependencies
from `requirements.txt`. You might want to use a virtual environment.
Make sure you are running a recent version of Python 3.
Afterwards install the requirements via `pip`:

    pip install -U -r rrnd_test/requirements.txt

The `Makefile` provides a command to automate creation of the virtual
environment and installing/updating dependencies. Make sure to have
`virtualenvwrapper.sh` installed and within your `PATH` when running the
following command:

    make nd-create-virtualenv

An instance of `rrnd_proxy` has to be started to be able to connect with the
discovery component. The according commands are supplied with the `Makefile`:

    make nd-{local,remote}

For remote testing you first need to connect to the microcontroller running
µPCN using `upcn_connect`. See the documentation of `rrnd_proxy` and
`upcn_connect` on how to achieve this.

You can now run two types of tests:

### 1. Unit Tests

These tests check basic functionality such as communication and conformance
of all components to the applied protocols.
Run them via `nosetests`:

    nosetests rrnd_test/test

### 2. Test Scenarios

For integration-testing the discovery component several scenarios have
been implemented. You can run them easily using the `Makefile`:

    make nd-test-scenario-<scenario>

Or call the module directly:

    python -m rrnd_test -h

The log level (verbosity) can be influenced by setting the environment variable
`RRND_LOGLEVEL` to one of the following values: `debuig`, `info`, `warning`.
Additionally it can be specified by the `-v` command-line switch.

When using the commands from the `Makefile`, an output file is generated
in the project's root directory, containing collected data about ground
stations and inferred contacts. This can then be used for statistical evaluation
and to create graphs.
For example you can analyze the accuracy of location inference depending on the
contact index by running:

    rrnd_test/eval.py ndtest_1_xxx.out -pv -s location -o contacts

Consult the help of the `eval.py` script to see further options:

    rrnd_test/eval.py -h
