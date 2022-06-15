#!/bin/sh

gcc -g -w -pthread -o multicast multicast.c
gcc -g -w -pthread -lmosquitto -o act actuator.c
gcc -g -w -pthread -lmosquitto -o sen sensor.c
