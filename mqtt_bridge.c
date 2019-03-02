/* mqtt_bridge - bridge between a TCP hub server and an MQTT broker

	 Messages in on the TCP connection are published to the MQTT broker
	 Messages from the MQTT broker are sent out on the TCP connection(s)

	 TCP Protocol
	 <topic>,<message>

	 For now, it only subscribes to Vision topic PI/CV/SHOOT/DAT

*/

#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h> // random()
#include <errno.h>

#include <mosquitto.h>

#define mqtt_host "localhost"
#define mqtt_port 1883

static int run = 1;

// one enum to cover commands and active processes
// invalid only applies to commands, not processes
enum processes {
	invalid = -1,
	off = 0,
	view = 1,
	vision = 2,
} ;

struct process_status {
	enum processes current_process;
	int pid;
} ;

struct process_status claw_process;
struct process_status cargo_process;

// strings for printing
const char *INVALID = "INVALID";
const char *OFF = "off";
const char *VIEW = "view";
const char *VISION = "vision";

enum processes cmd_check(char * command)
{
	enum processes ret = invalid;

	if(!strcmp(command, "off")) {
		ret = off;
	} else if(!strcmp(command, "view")) {
		ret = view;
	} else if(!strcmp(command, "vision")) {
		ret = vision;
	}

	return ret;
}

const char * str_cmd(enum processes proc)
{
	const char * ret;
	
	switch(proc) {
	case off:
		ret = OFF;
		break;

	case view:
		ret = VIEW;
		break;

	case vision:
		ret = VISION;
		break;

	default:
	case invalid:
		ret = INVALID;
		break;
	}
	return ret;
}

// this function starts computer vision by forking
// the parent returns from the fork call with the pid of the child
// the child returns with a zero and launches the task we wish to start
void claw_start_cv()
{
	pid_t ret;
	
	ret = fork();
	printf("fork returned %d\n", ret);
	switch(ret) {
	case -1:
		// error
		printf("unable to fork - %s\n", strerror(errno));
		exit(-1);

	case 0:
		// child
		// exec new process
		execl("useless.sh", "useless.sh", "vision", (char *) NULL);
		// never reach this point
		
	default:
		// parent
		claw_process.pid = ret;
		break;
	}
}

// this function starts computer vision by forking
// the parent returns from the fork call with the pid of the child
// the child returns with a zero and launches the task we wish to start
void claw_start_view()
{
	pid_t ret;
	
	ret = fork();
	printf("fork returned %d\n", ret);
	switch(ret) {
	case -1:
		// error
		printf("unable to fork - %s\n", strerror(errno));
		exit(-1);

	case 0:
		// child
		// exec new process
		execl("useless.sh", "useless.sh", "view", (char *) NULL);
		// never reach this point
		
	default:
		// parent
		claw_process.pid = ret;
		break;
	}
}

void claw_process_change(char *command)
{
	enum processes cmd;
	int ret;
	
	cmd = cmd_check(command);
	if(cmd == invalid) {
		return;
	}

	// are we already running this command?
	if(claw_process.current_process == cmd) {
		// nothing to do
		printf("Claw already in %s\n", str_cmd(cmd));
		return;
	}

	// are we running something else that must be shutdown?
	if(claw_process.current_process != off) {
		printf("Kill claw pid %d\n", claw_process.pid);
		if(claw_process.pid != 0) {
			ret = kill(claw_process.pid, SIGTERM);
			printf("Kill returned %d\n", ret);
		}
		claw_process.current_process = off;
	}
	if(cmd == off) {
		// done
		return;
	}
	
	// start new process
	if(cmd == vision) {
		claw_start_cv();
	} else if(cmd == view) {
		claw_start_view();
	}
	claw_process.current_process = cmd;
	printf("Starting new claw process pid %d\n", claw_process.pid);
	
}

void cargo_process_change(char *command)
{
	enum processes cmd;

	cmd = cmd_check(command);
	if(cmd == invalid) {
		return;
	}

	// are we already running this command?
	if(cargo_process.current_process == cmd) {
		// nothing to do
		printf("Cargo already in %s\n", str_cmd(cmd));
		return;
	}

	// are we running something else that must be shutdown?
	if(cargo_process.current_process != off) {
		printf("Kill cargo pid %d\n", cargo_process.pid);
		cargo_process.current_process = off;
	}
	if(cmd == off) {
		// done
		return;
	}
	
	// start new process
	cargo_process.current_process = cmd;
	cargo_process.pid = (int) random();
	printf("Starting new cargo process pid %d\n", cargo_process.pid);
	
}

void handle_signal(int s)
{
	run = 0;
}

void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	printf("connect callback, rc=%d\n", result);
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	bool match = 0;

	printf("got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);

	mosquitto_topic_matches_sub("/camera/controls/claw/+", message->topic, &match);
	if(match) {
		printf("got message for claw camera controls topic\n");
		claw_process_change(message->payload);
	}

	mosquitto_topic_matches_sub("/camera/controls/cargo/+", message->topic, &match);
	if(match) {
		printf("got message for cargo camera controls topic\n");
		cargo_process_change(message->payload);
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

	// initialize processes
	claw_process.current_process = off;
	claw_process.pid = -1;
	cargo_process.current_process = off;
	cargo_process.pid = -1;

	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mysql_log_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if(mosq){
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

	    rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		mosquitto_subscribe(mosq, NULL, "/camera/controls/+/+", 0);

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

