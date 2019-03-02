# mqtt_bridge

Bridge between a TCP connection and an MQTT broker

Messages in on the TCP connection are published to the MQTT broker
Messages from the MQTT broker are sent out on the TCP connection(s)

TCP Protocol
 <topic>,<message>

For now, it only subscribes to Vision topic PI/CV/SHOOT/DAT

This is based on a TCP hub server mainly to cover possible dropped and re-establilshed connections from the TCP client.

