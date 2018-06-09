#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	int r;
	while (1) {
		if ((r = ipc_recv(NULL, &nsipcbuf, NULL)) < 0)
			panic("Output.c: ipc receive error %e", r);
		
		if (r == NSREQ_OUTPUT)
			while((r = sys_net_try_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) < 0) {
				if (r != -E_TX_FULL)
					panic("Output.c: send packet error %e", r);
			}
	}
}
