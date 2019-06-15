#!/bin/bash

sudo ifconfig eth0 10.23.58.26

sleep 5

sudo /usr/sbin/mosquitto -p 1183 -d

sleep 20

/home/pi/Robotics/2019/mqtt_bridge/mqtt_bridge &

sleep 5

cd /home/pi/Robotics/2019/mqtt_controller

./mqtt_controller claw &


