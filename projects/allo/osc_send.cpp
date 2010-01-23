/* allocore */
#include "system/al_mainloop.h"
#include "system/al_time.h"
#include "types/al_pq.h"
#include "types/al_tube.h"
#include "types/al_types.h"

#include "protocol/al_OSCAPR.hpp"

/* Apache Portable Runtime */
#include "apr_general.h"
#include "apr_errno.h"
#include "apr_pools.h"
#include "apr_network_io.h"

/* oscpack */
#include "protocol/oscpack/osc/OscOutboundPacketStream.h"

#include "stdlib.h"

#define MAX_MESSAGE_LEN (4096)

/* APR utility */
static apr_status_t check_apr(apr_status_t err) {
	char errstr[1024];
	if (err != APR_SUCCESS) {
		apr_strerror(err, errstr, 1024);
		fprintf(stderr, "apr error: %s\n", errstr);
	}
	return err;
}

int main (int argc, char * argv[]) {
	apr_status_t err;
	// init APR:
	check_apr(apr_initialize());
	atexit(apr_terminate);	// ensure apr tear-down
	// create mempool:
	apr_pool_t * pool;
	check_apr(apr_pool_initialize());
	check_apr(apr_pool_create(&pool, NULL));
	apr_allocator_max_free_set(apr_pool_allocator_get(pool), 1024);
	
	osc::Send * sender = new osc::Send(pool, "localhost", 7009);
	
	char data[MAX_MESSAGE_LEN];
	osc::OutboundPacketStream packet(data, MAX_MESSAGE_LEN);
	for (int i=0; i<10; i++) {
	
		packet << osc::BeginMessage("/foo");
		packet << i;
		packet << osc::EndMessage;
		
		printf("sent %d bytes\n", sender->send(packet));
		packet.Clear();
		
		al_sleep(0.1);
	}
	
	// program end:
	delete sender;
	apr_pool_destroy(pool);
	return 0;
}

//int main (int argc, char * argv[]) {
//
//
//	apr_status_t err;
//	apr_pool_t * pool;
//	
//	// init APR:
//	check_apr(apr_initialize());
//	atexit(apr_terminate);	// ensure apr tear-down
//	
//	// create mempool:
//	check_apr(apr_pool_initialize());
//	check_apr(apr_pool_create(&pool, NULL));
//	apr_allocator_max_free_set(apr_pool_allocator_get(pool), 1024);
//
//	/* @see http://dev.ariel-networks.com/apr/apr-tutorial/html/apr-tutorial-13.html */
//	apr_sockaddr_t * sa;
//	apr_socket_t * sock;
//	apr_port_t port = 7009;
//	const char * host = "localhost";
//	check_apr(apr_sockaddr_info_get(&sa, host, APR_INET, port, 0, pool));
//	// for TCP, use SOCK_STREAM and APR_PROTO_TCP instead
//	check_apr(apr_socket_create(&sock, sa->family, SOCK_DGRAM, APR_PROTO_UDP, pool));
//	check_apr(apr_socket_opt_set(sock, APR_SO_NONBLOCK, 1));
//	
//	check_apr(apr_socket_connect(sock, sa));
//	
//	char data[MAX_MESSAGE_LEN];
//	osc::OutboundPacketStream packet(data, MAX_MESSAGE_LEN);
//	for (int i=0; i<10; i++) {
//	
//		packet << osc::BeginMessage("/foo");
//		packet << i;
//		packet << osc::EndMessage;
//		
//		apr_size_t size = packet.Size();
//		apr_socket_send(sock, packet.Data(), &size);
//		packet.Clear();
//		printf("sent %d bytes\n", size);
//		al_sleep(0.1);
//	}
//	
//	// program end:
//	check_apr(apr_socket_close(sock));
//	apr_pool_destroy(pool);
//	return 0;
//}