#!/bin/sh

DEFAULT_CONFIG_FILE=/tmp/.ddns_go_config.yaml

if [ -e ${SCRIPT_ROOT}/ddns/ddns.ymal ] ;  then 
	start-stop-daemon -S -b -p /tmp/ddns.pid -x ${SCRIPT_ROOT}/ddns/ddns-go -- -c ${SCRIPT_ROOT}/ddns/ddns.ymal
else
	${SCRIPT_ROOT}/ddns/ddns-go &
	while true; do 
		sleep 60
		name=$(cat ${DEFAULT_CONFIG_FILE} |grep username |awk -F':' '{print $2}')
		if [ "${name}" != "" ]; then
			cp ${DEFAULT_CONFIG_FILE} ${SCRIPT_ROOT}/ddns/ddns.ymal
			break
		fi
	done
fi 
