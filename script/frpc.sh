#!/bin/sh

# content of info.sh:
# SERVER_IP=192.168.10.100
# SERVER_PORT=80
# SERVER_API=1234567890abcdef1234567890abcdef
# TOKEN_ID=1234567890abcdef1234567890abcdef
# SCRIPT_ROOT=$(dirname $0)


FRP_ROOT=/tmp/frp
LOG_FILE=/tmp/frp/frpc.log 
FILE_NAME=linux.system.tools/pzl8-router/frpc.tar 
BINARY_PATH=http://${SERVER_IP}:${SERVER_PORT}/${SERVER_API}/${FILE_NAME}

log() {
    echo "$(date) | $1" >> ${LOG_FILE}
}


online() {
    mkdir -p ${FRP_ROOT}
    log "wait for device is online ..."
    while true ; do
        info=$(ping -c 1 ${SERVER_IP} |grep ttl)
        if [ "${info}x" = "x" ] ; then
            sleep 60
        else
            break
        fi
    done
}

download() {
    log "download file ..."
    TARGET=${FRP_ROOT}/frpc.tar
    while true ; do
        if [ -e ${TARGET} ]; then
            tar -xvf ${TARGET} -C ${FRP_ROOT}
            if [ $? = 0 ] && [ -e ${FRP_ROOT}/frpc ] && [ -e ${FRP_ROOT}/frpc.toml ]; then
                log "download file success"
                rm -f ${TARGET}
                break
            else 
                rm -f ${TARGET}
            fi
        else 
            wget ${BINARY_PATH} -O ${TARGET} &
            sleep 60
        fi
    done
}

replace() {
    log "replace file ..."
    # echo "s/FRP_SERVER_IP/${SERVER_IP}/g"
    # echo "s/FRP_TOKEN_ID/${TOKEN_ID}/g"
    sed -i "s/FRP_SERVER_IP/${SERVER_IP}/g"  ${FRP_ROOT}/frpc.toml
    sed -i "s/FRP_TOKEN_ID/${TOKEN_ID}/g"   ${FRP_ROOT}/frpc.toml
}

running() {
    log "runing ..."
    chmod +x ${FRP_ROOT}/frpc
    start-stop-daemon -S -b  -p ${FRP_ROOT}/frpc.pid -x ${FRP_ROOT}/frpc  -- -c ${FRP_ROOT}/frpc.toml
}

online
download
replace
running
