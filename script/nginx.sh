#!/bin/sh

# content of info.sh:
# SERVER_IP=192.168.10.100
# SERVER_PORT=80
# SERVER_API=1234567890abcdef1234567890abcdef
# TOKEN_ID=1234567890abcdef1234567890abcdef
# SCRIPT_ROOT=$(dirname $0)

. /etc/info.sh

mkdir -p /var/log/nginx
cp -rf ${SCRIPT_ROOT}/nginx /tmp/nginx
rm -rf /tmp/nginx/sbin/nginx

${SCRIPT_ROOT}/nginx/sbin/nginx -p /tmp/nginx -c /tmp/nginx/nginx.conf

