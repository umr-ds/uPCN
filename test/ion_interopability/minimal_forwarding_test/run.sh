#!/bin/bash

set -o errexit

# This assumes you are running the command from within the "upcn" directory.
UPCN_DIR="$(pwd)"

ION_VER="ion-3.6.2"

cd /tmp

# Download and build ION
wget -O "$ION_VER.tar.gz" "https://sourceforge.net/projects/ion-dtn/files/$ION_VER.tar.gz/download?use_mirror=netcologne&ts=$(date +%s)"
tar xvf "$ION_VER.tar.gz"
cd "$ION_VER"
./configure
make
make install
ldconfig

cd "$UPCN_DIR"

# Compile uPCN and create Python venv
make
make virtualenv || true
source .venv/bin/activate

exit_handler() {
    cd "$UPCN_DIR"

    echo "Terminating ION and uPCN..."
    ionstop || true
    kill -KILL $(ps ax | grep upcn | tr -s ' ' | sed 's/^ *//g' | cut -d ' ' -f 1) 2> /dev/null || true

    echo
    echo ">>> ION LOGFILE"
    cat "ion.log" || true
    echo
    echo ">>> uPCN1 LOGFILE"
    cat "/tmp/upcn1.log" || true
    echo
    echo ">>> uPCN2 LOGFILE"
    cat "/tmp/upcn2.log" || true
    echo
}

trap exit_handler EXIT

rm -f ion.log
rm -f /tmp/*.log

# Start first uPCN instance (uPCN1)
stdbuf -oL "$UPCN_DIR/build/posix/upcn" -a 4242 -c "tcpclv3:127.0.0.1,4555" -b 6 -e "dtn://upcn1.dtn" > /tmp/upcn1.log 2>&1 &

# Start second uPCN instance (uPCN2)
stdbuf -oL "$UPCN_DIR/build/posix/upcn" -a 4243 -c "tcpclv3:127.0.0.1,4554" -b 6 -e "dtn://upcn2.dtn" > /tmp/upcn2.log 2>&1 &

# Start ION instance
ionstart -I test/ion_interopability/minimal_forwarding_test/ionstart.rc

# Configure a contact to ION in uPCN1 which allows to reach uPCN2
sleep 0.5
python "$UPCN_DIR/tools/aap/aap_config.py" --schedule 1 3600 10000 --reaches "dtn://upcn2.dtn" ipn:1.0 tcpclv3:127.0.0.1:4556

# Send a bundle to uPCN1, addressed to uPCN2
PAYLOAD="THISISTHEBUNDLEPAYLOAD"
python "$UPCN_DIR/tools/aap/aap_send.py" --agentid source "dtn://upcn2.dtn/sink" "$PAYLOAD" &

timeout 10 stdbuf -oL python "$UPCN_DIR/tools/aap/aap_receive.py" --port 4243 --agentid sink --count 1 --verify-pl "$PAYLOAD"
