#include <cla_management.h>
#include <cla.h>
#include <cla_defines.h>
#include <cla_io.h>
#include "upcn/init.h"
#include <cla_contact_rx_task.h>
#include <cla_contact_tx_task.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "upcn/sdnv.h"
