/* mqtt_bridge.h

 */

#ifndef _MQTT_BRIDGE_H_
#define _MQTT_BRIDGE_H_

#include <netinet/in.h> // inet_ntoa
#include <arpa/inet.h> // inet_ntoa
#include <mosquitto.h>

struct sock_info {
	int sock; // socket
	int age; // socket age;
	struct sockaddr_in far_addr;
} ;

int GetString(int insock, char *s, int maxlen);
void PutString(char *s);
void ClearSocket(struct sock_info *psi);
void CloseSocket(struct sock_info *psi);
void PrintSocketInfo(void);
void SignalHandler(int signum);
void handle_signal(int s);
void mqtt2tcp(const struct mosquitto_message *message);
void tcp2mqtt(char * smsg);
void connect_callback(struct mosquitto *mosq, void *obj, int result);
void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
void usage();
void cleanup(void);


#endif // _MQTT_BRIDGE_H_
