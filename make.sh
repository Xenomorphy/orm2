#!/bin/sh

gcc -o device device.c
gcc -pthread -o controller controller.c

gcc -pthread -lmosquitto -o pub mqtt_pub.c
gcc -pthread -lmosquitto -o sub mqtt_sub.c
