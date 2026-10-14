#!/bin/bash

function uncompress_files()  {
    mkdir -p src/install 
    tar -zxvf ${CURRENT_DIR}/tools/arm-linux-musleabi-cross.tar.gz -C ${CURRENT_DIR}
    tar -zxvf ${CURRENT_DIR}/tools/nginx-1.14.2.tar.gz -C ${CURRENT_DIR}/src
    tar -zxvf ${CURRENT_DIR}/tools/openssl-1.1.1w.tar.gz -C ${CURRENT_DIR}/src
    tar -zxvf ${CURRENT_DIR}/tools/pcre-8.45.tar.gz -C ${CURRENT_DIR}/src


    # tar -zxvf ${CURRENT_DIR}/tools/readline_8.0.orig.tar.gz -C ${CURRENT_DIR}/src
    # tar -zxvf ${CURRENT_DIR}/tools/socat-1.7.4.4.tar.gz -C ${CURRENT_DIR}/src
}


function build_pcre(){
    cd ${CURRENT_DIR}/src/pcre-8.45
    cp ${CURRENT_DIR}/configure/pcre.sh ./
    bash ./pcre.sh && make -j6 && make install 
    cd ${CURRENT_DIR}
}

function build_openssl(){
    cd ${CURRENT_DIR}/src/openssl-1.1.1w
    cp ${CURRENT_DIR}/configure/openssl.sh ./
    bash ./openssl.sh && make -j6 && make install
    cd ${CURRENT_DIR}
}

function build_nginx(){
    cd ${CURRENT_DIR}/src/nginx-1.14.2
    patch -p2 < ${CURRENT_DIR}/configure/patch/nginx.patch
    cp ${CURRENT_DIR}/configure/nginx.sh ./
    bash ./nginx.sh && make -j6 && make install
}

uncompress_files
build_pcre
build_openssl
build_nginx