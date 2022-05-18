#!/bin/sh

gcc -o device device.c
gcc -pthread -o controller controller.c
