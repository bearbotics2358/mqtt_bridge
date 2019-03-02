/* mqtt_bridge - bridge between a TCP hub server and an MQTT broker

	 Messages in on the TCP connection are published to the MQTT broker
	 Messages from the MQTT broker are sent out on the TCP connection(s)

	 TCP Protocol
	 <topic>,<message>

	 For now, it only subscribes to Vision topic PI/CV/SHOOT/DAT

	 The TCP server receives connections from clients.  The connections to the
	 clients are kept open (up to a max MAX_BROWSER connections).
	 
	 A new connection received when there are already MAX_BROWSER
	 connections open will cause the oldest non-local connection to be closed.
	 
	 This process defaults to port 9122.  If a port is provided on
	 the command line, this port will be used for all connections.
	 
*/

#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h> // strerror()
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h> // random(), exit
#include <errno.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h> // inet_ntoa
#include <arpa/inet.h> // inet_ntoa
#include <fcntl.h> // fcntl
#include <limits.h> // INT_MAX
#include "inetlib.h"
#include "mqtt_bridge.h"

#include <mosquitto.h>

#define mqtt_host "localhost"
#define mqtt_port 1883

static int run = 1;

void handle_signal(int s)
{
	run = 0;
}

// Build string based on MQTT message and send to TCP client
void mqtt2tcp(const struct mosquitto_message *message)
{
	char s1[255];

	sprintf(s1, "%s,%s", message->topic, message->payload);
	
	printf("Sending '%s' to TCP client\n", s1);
}

void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	printf("connect callback, rc=%d\n", result);
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	bool match = 0;

	printf("got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);

	mosquitto_topic_matches_sub("PI/CV/SHOOT/DATA", message->topic, &match);
	if(match) {
		printf("got message from cv topic\n");
		mqtt2tcp(message);
	}

}

int main(int argc, char *argv[])
{
	uint8_t reconnect = true;
	char clientid[24];
	struct mosquitto *mosq;
	int rc = 0;

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mysql_log_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if(mosq){
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

	    rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		mosquitto_subscribe(mosq, NULL, "PI/CV/SHOOT/DATA", 0);

		while(run){
			// BD changed timeout to 0 from -1 (1000ms)
			rc = mosquitto_loop(mosq, 0, 1);
			if(run && rc){
				printf("connection error!\n");
				sleep(10);
				mosquitto_reconnect(mosq);
			}
			sleep(1);
		}
		mosquitto_destroy(mosq);
	}

	mosquitto_lib_cleanup();

	return rc;
}

