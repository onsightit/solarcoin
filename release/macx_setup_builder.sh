#!/bin/sh

# Note: The structure of this package depends on the -rpath,./lib to be set at compile/link time.

version="2.1.3"
arch=`uname -m`

if [ "${arch}" = "x86_64" ]; then
    arch="64bit"
else
    arch="32bit"
fi

if [ -f SolarCoin-Qt.app/Contents/MacOS/SolarCoin-Qt ] && [ -f solarcoin.conf ] && [ -f README ]; then
    echo "Building SolarCoin_${version}_${arch}.pkg ...\n"
    cp solarcoin.conf SolarCoin-Qt.app/Contents/MacOS/
    cp README SolarCoin-Qt.app/Contents/MacOS/
    cp doc/SolarCoinEula.* SolarCoin-Qt.app/Contents/MacOS/

    # Remove the old archive
    if [ -f SolarCoin_${version}_${arch}.pkg ]; then
        rm -f SolarCoin_${version}_${arch}.pkg
    fi

    # Deploy the app, create the plist, then build the package.
    macdeployqt ./SolarCoin-Qt.app -always-overwrite
    pkgbuild --analyze --root ./SolarCoin-Qt.app share/qt/SolarCoin-Qt.plist
    pkgbuild --root ./SolarCoin-Qt.app --component-plist share/qt/SolarCoin-Qt.plist --identifier org.solarcoin.SolarCoin-Qt --install-location /Applications/SolarCoin-Qt.app SolarCoin_${version}_${arch}.pkg
    echo "Package created in: $PWD/SolarCoin_${version}_${arch}.pkg\n"
else
    echo "Error: Missing files!\n"
    echo "Run this script from the folder containing SolarCoin-Qt.app, solarcoin.conf and README.\n"
fi

