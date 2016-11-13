#!/bin/sh

# This script depends on the GNU script makeself.sh found at: http://megastep.org/makeself/
# Note: The structure of this package depends on the -rpath,./lib to be set at compile/link time.

version="2.1.8"
arch=`uname -i`

if [ "${arch}" = "x86_64" ]; then
    arch="64bit"
    QtLIBPATH="${HOME}/Qt/5.4/gcc_64"
else
    arch="32bit"
    QtLIBPATH="${HOME}/Qt/5.4/gcc"
fi

if [ -f solarcoin-qt ] && [ -f solarcoin.conf ] && [ -f README.md ]; then
    echo "Building SolarCoin_${version}_${arch}.run ...\n"
    if [ -d SolarCoin_${version}_${arch} ]; then
        rm -fr SolarCoin_${version}_${arch}/
    fi
    mkdir SolarCoin_${version}_${arch}
    mkdir SolarCoin_${version}_${arch}/libs
    mkdir SolarCoin_${version}_${arch}/platforms
    mkdir SolarCoin_${version}_${arch}/imageformats
    cp solarcoin-qt SolarCoin_${version}_${arch}/
    cp solarcoin.conf SolarCoin_${version}_${arch}/
    cp README.md SolarCoin_${version}_${arch}/
    cp SolarCoinEula.* SolarCoin_${version}_${arch}/
    ldd solarcoin-qt | grep libssl | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    ldd solarcoin-qt | grep libdb_cxx | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    ldd solarcoin-qt | grep libboost_system | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    ldd solarcoin-qt | grep libboost_filesystem | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    ldd solarcoin-qt | grep libboost_program_options | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    ldd solarcoin-qt | grep libboost_thread | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    ldd solarcoin-qt | grep libminiupnpc | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    ldd solarcoin-qt | grep libqrencode | awk '{ printf("%s\0", $3); }' | xargs -0 -I{} cp {} SolarCoin_${version}_${arch}/libs/
    cp ${QtLIBPATH}/lib/libQt*.so.5 SolarCoin_${version}_${arch}/libs/
    cp ${QtLIBPATH}/lib/libicu*.so.53 SolarCoin_${version}_${arch}/libs/
    cp ${QtLIBPATH}/plugins/platforms/lib*.so SolarCoin_${version}_${arch}/platforms/
    cp ${QtLIBPATH}/plugins/imageformats/lib*.so SolarCoin_${version}_${arch}/imageformats/
    strip SolarCoin_${version}_${arch}/solarcoin-qt
    echo "Enter your sudo password to change the ownership of the archive: "
    sudo chown -R nobody:nogroup SolarCoin_${version}_${arch}

    # now build the archive
    if [ -f SolarCoin_${version}_${arch}.run ]; then
        rm -f SolarCoin_${version}_${arch}.run
    fi
    makeself.sh --notemp SolarCoin_${version}_${arch} SolarCoin_${version}_${arch}.run "\nCopyright (c) 2014-2015 The SolarCoin Developers\nSolarCoin will start when the installation is complete...\n" ./solarcoin-qt \&
    sudo rm -fr SolarCoin_${version}_${arch}/
    echo "Package created in: $PWD/SolarCoin_${version}_${arch}.run\n"
else
    echo "Error: Missing files!\n"
    echo "Copy this file to a setup folder along with solarcoin-qt, solarcoin.conf and README.md.\n"
fi

