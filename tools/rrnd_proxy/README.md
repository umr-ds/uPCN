
RRND Host and Proxy Application
===============================

This tool provides a **ZeroMQ** interface for testing the RRND
discovery component.
It is able to _locally host_ an instance of RRND as well as to _connect_ to
the discovery component integrated with _µPCN_ via `upcn_connect`.

Building
--------

The simplest way to build `rrnd_proxy` is via the `Makefile`:

    make tools

This will also compile the other tools (`upcn_connect`, `upcn_receive`, ...).

Locally Hosted RRND
-------------------

Run `rrnd_proxy` with the _ZeroMQ socket_ it should be listening at as single
argument. Per default this is `tcp://127.0.0.1:7763`:

    rrnd_proxy tcp://127.0.0.1:7763

You can also use the `Makefile`:

    make nd-local

Remote Connection (µPCN)
------------------------

Like in local mode you have to supply a listening socket. Besides that, the
_ZeroMQ addresses_ of the `upcn_connect` _publisher_ and _request/reply_ sockets
are required. Per default these are `tcp://127.0.0.1:8726` respective
`tcp://127.0.0.1:8727`:

    rrnd_proxy tcp://127.0.0.1:7763 tcp://127.0.0.1:8726 tcp://127.0.0.1:8727

Again the `Makefile` has this command built in:

    make nd-remote

API
---

A simple JSON-based API is provided via **ZeroMQ**.
The socket type of `rrnd_proxy` is **REP**.
All requests are composed of a _command_ and command-specific
_additional data_.
Timestamps are expressed as integral _UNIX-Time_,
seconds since 1970-01-01 00:00.

A result is returned after the command has been processed in the form of a
JSON object. The `result` code consists of the flags defined in
`enum rrnd_status` in `rrnd.h`. See also `rrnd_test/nd/status.py`.
The returned JSON has the following format:

    {"result": <int>}

For the `get_gs` command it may also contain a `gs` key, containing
discovered information regarding a specific ground station.

The following commands are available:

### 1. Test Connectivity

Perform an end-to-end test of connectivity.

    {"command": "test"}

### 2. Reset

Reset all stored data (ground stations, ...) to default.

    {"command": "reset"}

### 3. Initialize

Set the orbital parameters for position determination.

This command has to be provided with the _Two-Line-Elements_ to be used and
the _current timestamp_ for determining the age of stored data.

    {"command": "init", "time": <int>, "tle": <string>}

### 4. Process Beacon

Request processing of a discovery beacon.

This command triggers _discovery beacon_ processing. The beacon has to be
supplied as a _JSON array_.

    {"command": "process", "time": <int>, "eid": <string>, "beacon": <array>}

The reception time is specified by `time` and the _Endpoint Identifier_
of the node sending the beacon by `eid`. Additional information is encoded
within the `beacon` array. It has the following format:

    [seqnum, period, tx_bitrate, rx_bitrate, flags, avail_duration, eids]

All values except the last (`eids`) are represented as long integers.
They translate directly into the beacon structure defined in `rrnd.h`.
The `eids` field is another array, consisting of the EIDs of multi-hop
neighbors advertised with the beacon:

    ["<EID 1>", "<EID 2>", ...]

If only single-hop discovery is needed or no additional neighbors are known,
an empty array (`[]`) should be sent.

### 5. Infer Contact

Request the prediction of a new contact.

This command triggers _contact inference_. The discovery component will try to
generate a contact prediction and store it with the ground station data.
The specified `time` value serves as start timestamp.

    {"command": "next_contact", "time": <int>, "eid": <string>}

### 6. Query GS Information

Get all stored information regarding a specific ground station.

This command returns stored ground station data as JSON object.
It is the only command returning more than just a numeric `result` value.
See `rrnd_io.c` on what is returned.

    {"command": "get_gs", "eid": <string>}

Result:

    {"result": <int>, "gs": { ... }}

### 7. Perf Data Management

When running against uPCN, this collects CPU runtimes of various tasks.
To store and view:

    {"command": "store_perf"}

To reset stats:

    {"command": "reset_perf"}

The output is printed to the console.
