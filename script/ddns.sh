#!/bin/sh

DEFAULT_CONFIG_FILE=/tmp/.ddns_go_config.yaml

. /etc/info.sh 
# content of info.sh:
# SERVER_IP=192.168.10.100
# SERVER_PORT=80
# SERVER_API=1234567890abcdef1234567890abcdef
# TOKEN_ID=1234567890abcdef1234567890abcdef
# SCRIPT_ROOT=$(dirname $0)


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
