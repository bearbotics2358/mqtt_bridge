/* mqtt_bridge.h

 */

#ifndef _MQTT_BRIDGE_H_
#define _MQTT_BRIDGE_H_

#include <netinet/in.h> // inet_ntoa
#include <arpa/inet.h> // inet_ntoa

struct sock_info {
	int sock; // socket
	int age; // socket age;
	int local; // flag to indicate if this connection is a local connection
	struct sockaddr_in far_addr;
} ;

#endif // _MQTT_BRIDGE_H_
