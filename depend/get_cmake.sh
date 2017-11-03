#!/bin/sh
#
# Copyright © 2015-2016, Prosoft Engineering, Inc. (A.K.A "Prosoft")
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Prosoft nor the names of its contributors may be
#       used to endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL PROSOFT ENGINEERING, INC. BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e

while : ; do
	case $# in 0) break ;; esac
    
	case "$1" in
		--prefix )
			shift
			INSTALL_DIR="$1"
			shift
		;;
		--cmake )
			shift
			CMAKE_VER="$1"
			shift
		;;
		* )
		    echo "Unknown argument: ${1}"
		    break
		;;
	esac
done

# Could not find a link/feed to automatically find the latest version, so we just hardcode it.
if [ -z $CMAKE_VER ]; then
CMAKE_VER=3.7.0
fi

VROOT=`echo ${CMAKE_VER} | grep -oE '[0-9]\.[0-9]'`
# cmake.org (as of 2015-11) returns a 301 error for HTTP pointing to HTTPS.
URL=https://cmake.org/files/v${VROOT}/cmake-${CMAKE_VER}.tar.gz

cd ${TMPDIR}
if [ `uname -s` == 'Darwin' ]; then
curl -O ${URL}
elif [ `uname -s` != 'Linux' ]; then
# Note: recent versions of CuRL ship with TLS basically disabled since they do not provide a root cert trust.
# See:
# http://curl.haxx.se/docs/sslcerts.html
# http://unitstep.net/blog/2009/05/05/using-curl-in-php-to-access-https-ssltls-protected-sites/
# https://github.com/Homebrew/homebrew/issues/6103
# So (unfortunately) we disable SSL verification with -k
curl -k -O ${URL}
else
# Linux may also not have a certificate store (Travis)
wget --no-check-certificate ${URL}
fi

BUILD_DIR=cmake-tmp-$$
mkdir ${BUILD_DIR}
cd ${BUILD_DIR}
tar -xzf ../cmake-${CMAKE_VER}.tar.gz --strip-components 1

if [ -z $INSTALL_DIR ]; then
./bootstrap && make && sudo make install
else
./bootstrap --prefix="${INSTALL_DIR}" && make && make install
fi

if [ $? -eq 0 ]; then
cd ..
rm -rf ${BUILD_DIR}
fi
