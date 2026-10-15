#!/bin/sh

# content of info.sh:
# SERVER_IP=192.168.10.100
# SERVER_PORT=80
# SERVER_API=1234567890abcdef1234567890abcdef
# TOKEN_ID=1234567890abcdef1234567890abcdef
# SCRIPT_ROOT=$(dirname $0)

. /etc/info.sh

cp  ${SCRIPT_ROOT}/nginx/conf/nginx.conf /tmp/nginx/conf/


