#!/bin/bash

sleep 30

sudo /usr/sbin/mosquitto -p 1183 -d

sleep 30

sudo ifconfig eth0:1 10.23.58.2

sleep 30

/home/pi/Robotics/2019/mqtt_bridge/mqtt_bridge &

sleep 30

cd /home/pi/Robotics/2019/mqtt_controller

./mqtt_controller claw &

sleep 30
