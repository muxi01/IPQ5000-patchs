./configure \
--prefix=${CURRENT_DIR}/src/install/nginx \
--sbin-path=${CURRENT_DIR}/src/install/nginx \
--conf-path=${CURRENT_DIR}/src/install/nginx/nginx.conf \
--error-log-path=/var/log/nginx/error.log \
--http-log-path=/var/log/nginx/access.log \
--pid-path=/var/run/nginx.pid \
--lock-path=/var/run/nginx.lock \
--with-cc=${CC} \
--with-cpp=${CXX} \
--with-cc-opt="-I${CURRENT_DIR}/src/install/include -O2" \
--with-ld-opt="-L${CURRENT_DIR}/src/install/lib -static" \
--with-http_ssl_module \
--with-stream \
--with-stream_ssl_module \
--without-http_gzip_module \
--without-http_autoindex_module \
--without-http_ssi_module \
--without-http_userid_module \
--without-http_geo_module \
--without-http_map_module \
--without-http_split_clients_module \
--without-http_referer_module \
--without-http_fastcgi_module \
--without-http_uwsgi_module \
--without-http_scgi_module \
--without-http_memcached_module \
--without-http_limit_conn_module \
--without-http_limit_req_module \
--without-http_empty_gif_module \
--without-http_browser_module \
--without-poll_module \
--without-select_module



