./Configure linux-armv4 \
--prefix=${CURRENT_DIR}/src/install \
--openssldir=${CURRENT_DIR}/src/install/etc/ssl \
--cross-compile-prefix='' \
no-tests \
no-shared \
-static

