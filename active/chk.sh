#!/bin/bash

TARGET_DIR=/home/pi/csn_raspi/active
LASTTIME=`date +'%s'`

START=${TARGET_DIR}/start.txt
ALIVE=${TARGET_DIR}/alive.txt
if [ -e ${START} ]; then
    echo ${LASTTIME} > ${ALIVE}
else
    touch ${START}
    touch ${ALIVE}
    echo ${LASTTIME} > ${START}
fi
