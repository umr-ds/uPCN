µPCN USB-ZMQ-Proxy
==================

This program is used to connect to µPCN when running it on a microcontroller
connected via the USB Virtual COM Port (VCP) device.
It decodes received data packets and forwards them via a ZeroMQ publisher (PUB)
socket.
On the other hand, it accepts transmissions via a ZeroMQ reply (REP) socket
and forwards the data to the board.

To run the program manually, you have to specify the USB device, an
address for the PUB socket, and one for the REP socket.

    upcn_connect <device> <pub_sock> <rep_sock>

An example command can be found in the Makefile.
In most use-cases it should be sufficient to simply run:

    make connect

Troubleshooting
---------------

1. If you don't have the necessary permissions to run `make connect`, add
   yourself to the respective group (e.g. `dialout` or `uucp`).
   You can find out the group's name with `ls -l /dev/ttyACM*`, and
   add your user by running `sudo gpasswd -a <user> <group>`.

2. If you experience issues with messages getting dropped or the USB device not
   being reachable during tests, another process may be claiming the device.
   In particular, issues will arise if you are running ModemManager, because
   it will try to connect to the ACM device, treating it like a UMTS modem.
   To prevent that, add an udev rule in `/etc/udev/rules.d/99-mm-ignore.rules`:

        # Ignore ST devices in ModemManager
        ATTRS{idVendor}=="0483", ENV{ID_MM_DEVICE_IGNORE}="1"
