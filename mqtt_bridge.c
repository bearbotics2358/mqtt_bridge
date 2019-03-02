/* mqtt_bridge - bridge between a TCP hub server and an MQTT broker

	 Messages in on the TCP connection are published to the MQTT broker
	 Messages from the MQTT broker are sent out on the TCP connection(s)

	 TCP Protocol
	 <topic>,<message>

	 For now, it only subscribes to Vision topic PI/CV/SHOOT/DAT

	 The TCP server receives connections from clients.  The connections to the
	 clients are kept open (up to a max MAX_BROWSER connections).
	 
	 A new connection received when there are already MAX_BROWSER
	 connections open will cause the oldest connection to be closed.
	 
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

#include <sys/select.h>
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

// for mqtt broker
#define mqtt_host "localhost"
#define mqtt_port 1883

#define CR 0x0d
#define LF 0x0a

#define QLEN 10
#define MAX_BROWSERS 25

// for TCP server
#define DEFAULT_PORT 9122

#define MAXLEN 255

#define MAX_AGE INT_MAX

// sockets are global so cleanup can close them
int serversock; // server socket that listens
struct sock_info outsocks[MAX_BROWSERS];

struct mosquitto *mosq;

// run is used to ensure cleanup of libmosquitto
// switch to normal method based on atexit() and cleanup()
static int run = 1;

int GetString(int insock, char *s, int maxlen)
{
	// return string in s without CRLF, number of characters in string
	// as return value;
	int totread = 0;
	int numread;
	int ret;
	char c;
	int fGood = false;
	char *ps = s;

	while(totread < maxlen) {
		// printf("waiting to read ...");
		numread = read(insock, &c, 1);
		// printf("(%2.2X) %c\n", c, c);
		if(numread < 0) {
			printf("error reading from input socket: %d\n", numread);
			break;
		}
		if((c == CR) || (c == LF)) {
			fGood = true;
			break;
		}
		*ps++ = c;
		totread++;
	}
	if(!fGood) {
		// clear string & report error
		ps = s;
		totread = -1;
	}
	*ps = 0;
	return totread;
}

void PutString(char *s)
{
	int i, j;
	char sout[MAXLEN + 2];
	int ret;
	int max_age_reached = 0;

	// append a CRLF
	strcpy(sout, s);
	i = strlen(s);
	sout[i] = CR;
	sout[i + 1] = LF;
	sout[i + 2] = 0;
	// Now send to all listener processes
	for(i = 0; i < MAX_BROWSERS; i++) {
		if(outsocks[i].sock) {
			// printf("Send string to [%d:%d]: *%s*\n", i, outsocks.sock[i], s);
			ret = write(outsocks[i].sock, sout, strlen(sout));
			if(ret < 0) {
				printf("error(%d) writing to socket: %s\n", ret, strerror(errno));
				CloseSocket(&outsocks[i]);
      }
			// update socket ages for the sockets older than this one
			if(++outsocks[i].age >= MAX_AGE) {
				max_age_reached = 1;
			}
		}
	}
	if(max_age_reached) {
		// decrement the age of all sockets which are in use
		for(i = 0; i < MAX_BROWSERS; i++) {
			if(outsocks[i].sock) {
				outsocks[i].age--;
			}
		}
	}
}

void ClearSocket(struct sock_info *psi)
{
	bzero(psi, sizeof(struct sock_info));
}

void CloseSocket(struct sock_info *psi)
{
	close(psi->sock);
	ClearSocket(psi);
}

void PrintSocketInfo(void)
{
	int i;

	printf("#\tsock\tage\tdest\n");
	for(i = 0; i < MAX_BROWSERS; i++) {
		printf("%d\t%d\t%d\t%s (0x%8.8X)\n", 
					 i, 
					 outsocks[i].sock, 
					 outsocks[i].age, 
					 inet_ntoa(outsocks[i].far_addr.sin_addr), 
					 outsocks[i].far_addr.sin_addr.s_addr
					 );
	}
}

void SignalHandler(int signum)
{
	printf("Caught signal %d\n", signum);
	PrintSocketInfo();
	fflush(stdout);
}

void handle_signal(int s)
{
	run = 0;
}

// Build string based on MQTT message and send to TCP client
void mqtt2tcp(const struct mosquitto_message *message)
{
	char s1[255];

	sprintf(s1, "%s,%s", message->topic, (char *)message->payload);
	
	printf("Sending '%s' to TCP client\n", s1);
	PutString(s1);
}

// Parse string into topic and msg and sent to mqtt broker
void tcp2mqtt(char * smsg)
{
	char topic[255];
	char msg[255];
	char *p1;
	int rc;

	// parse smsg, breaking into topic and msg at the ","
	p1 = strtok(smsg, ",");
	strcpy(topic, p1);
	p1 = strtok(NULL, ",");
	strcpy(msg, p1);
	printf("To mqtt broker: topic: %s  msg: %s\n", topic, msg);

	if(mosq) {
		mosquitto_publish(mosq, 0, topic, strlen(msg), msg, 0, 0);
		rc = mosquitto_loop(mosq, 0, 1);
		if(rc){
			printf("connection error!\n");
			sleep(10);
			mosquitto_reconnect(mosq);
		}
	}

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

void usage()
{
	printf("usage: mqtt_bridge [port]\n");
}

void cleanup(void)
{
	int i;
	
	if(serversock) {
		close(serversock);
		serversock = 0;
	}

	for(i = 0; i < MAX_BROWSERS; i++) {
		if(outsocks[i].sock) {
			close(outsocks[i].sock);
			ClearSocket(&outsocks[i]);
		}
	}

	if(mosq) {
		mosquitto_destroy(mosq);
	}

	mosquitto_lib_cleanup();
	mosq = 0;
}

int main(int argc, char *argv[])
{
	uint8_t reconnect = true;
	char clientid[24];
	int rc = 0;

	struct sockaddr_in fsin; // the from address of a client
	int clientport = DEFAULT_PORT;
	char *service = "daytime"; // service name or port number
	int alen; // from address length
	char scmd[MAXLEN]; // string from client
	int i, j;
	char stemp[256];
	int oldest;
	int ret;
	int sock_temp;
	char *pstr;
	struct timeval timeout;
	
	fd_set orig_fdset, fdset;
  int nfds = getdtablesize(); // number of file descriptors

	// currently arg processing came from TCP hub program
	switch(argc) {
	case 1:
		break;

	case 2:
		if((clientport = atoi(argv[1])) <= 0) {
			printf("Port number unacceptable: %d\n", clientport);
			exit(-1);
		}
		break;

	default:
		usage();
		exit(-1);
	}

	sprintf(stemp, "%d", clientport);

	// initialize sockets and mosq to 0 so that cleanup() can determine if they are assigned
	serversock = 0;
	for(i = 0; i <  MAX_BROWSERS; i++) {
		ClearSocket(&outsocks[i]);
	}
	mosq = 0;
	
	mosquitto_lib_init();

	atexit(cleanup);

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	if(signal(SIGPIPE, &SignalHandler) == SIG_ERR) {
		printf("Couldn't register signal handler for SIGPIPE\n");
	}

	if(signal(SIGHUP, &SignalHandler) == SIG_ERR) {
		printf("Couldn't register signal handler for SIGHUP\n");
	}

	// initialize mosquitto
	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mysql_log_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if(mosq){
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		mosquitto_subscribe(mosq, NULL, "PI/CV/SHOOT/DATA", 0);

	} else {
		printf("Not able to create mosquitto client\n");
		exit(-1);
	}

	
	// Initialize TCP server
	serversock = passiveTCP(stemp, QLEN);
	// make it async so that select() reuturn that is not from an outstanding connection can 
	// be ignored
	fcntl(serversock, F_SETFL, O_NONBLOCK | SO_REUSEADDR);

	FD_ZERO(&orig_fdset);
	FD_SET(serversock, &orig_fdset);

	while(run && mosq) {
		// Process mosquitto messages
		// BD changed timeout to 0 from -1 (1000ms)
		rc = mosquitto_loop(mosq, 0, 1);
		if(run && rc){
			printf("connection error!\n");
			// this should be changed for this architecture:
			sleep(10);
			mosquitto_reconnect(mosq);
		}
		// sleep(1);

		/* Restore watch set as appropriate. */
		bcopy(&orig_fdset, &fdset, sizeof(orig_fdset));
		for(i = 0; i < MAX_BROWSERS; i++) {
			if(outsocks[i].sock) {
				FD_SET(outsocks[i].sock, &fdset);
				// printf("setting socket # %d\n", outsocks[i].sock);
			}
		}

		// need to sleep sometime, so just set timeout to 100 usec
		timeout.tv_sec = 0;
		timeout.tv_usec = 100;
		ret = select(nfds, &fdset, NULL, NULL, &timeout);
		if(ret != 0) {
			printf("select returned %d\n", ret);
		}
		if(ret > 0) {
			// process input from TCP connection
			
			for(i = 0; i < MAX_BROWSERS; i++) {
				if (outsocks[i].sock && FD_ISSET(outsocks[i].sock, &fdset)) {
					printf("Client has data - fd: %d\n", outsocks[i].sock);
					if((ret = GetString(outsocks[i].sock, scmd, MAXLEN)) >= 0) {
						if(ret > 0) {
							// Parse and send to MQTT broker
							tcp2mqtt(scmd);
						}
					} else {
						printf("GetString() returned (%d) < 0\n", ret);
						FD_CLR(outsocks[i].sock, &fdset);
						CloseSocket(&outsocks[i]);
					}
				}
			}
		}

		// check for new connection
		if (FD_ISSET(serversock, &fdset)) {
			// New connection
			printf("New connection received\n");
			i = 0; // TEMP for debug
			for(i = 0; i < MAX_BROWSERS; i++) {
				if(outsocks[i].sock == 0)
					break;
			}

			// if none available, close oldest socket & use it
			// find oldest
			if(i == MAX_BROWSERS) {
				oldest = 0;
				j = 0;
				for(i = 0; i < MAX_BROWSERS; i++) {
					if(outsocks[i].sock) {
						if(outsocks[i].age > oldest) {
							oldest = outsocks[i].age;
							j = i;
						}
					}
				}
				i = j;
				printf("Closing socket # %d to make room for a new connection\n", i);
				CloseSocket(&outsocks[i]);
			}
			
			// getting stuck at following line - how did we get select return if no one waiting?
			alen = sizeof(outsocks[i].far_addr);
			outsocks[i].sock = accept(serversock, (struct sockaddr *)&outsocks[i].far_addr, &alen);
			if(outsocks[i].sock < 0) {
				if(errno == EAGAIN) {
					printf("no one there - ignore knock\n");
					ClearSocket(&outsocks[i]);
				} else {
					printf("accept failed - errno:%d %s\n", errno, strerror(errno));
					exit(1);
				}
			} else {
				pstr = inet_ntoa(outsocks[i].far_addr.sin_addr);
				printf("Connection fd: %d from %s\n", outsocks[i].sock, pstr);
				write(outsocks[i].sock, "hello,hello\r\n", 13);
			}
		}

	}
	return 0;
}

