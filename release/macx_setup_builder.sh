#!/bin/sh

# Note: The structure of this package depends on the -rpath,./lib to be set at compile/link time.

version="2.1.8"
year=`date +%Y`
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
    cp share/qt/Info.plist SolarCoin-Qt.app/Contents/
    sed -e "s/@VERSION@/$version/g" -i "" SolarCoin-Qt.app/Contents/Info.plist
    sed -e "s/@YEAR@/$year/g" -i "" SolarCoin-Qt.app/Contents/Info.plist

    # Remove the old archive
    if [ -f SolarCoin_${version}_${arch}.pkg ]; then
        rm -f SolarCoin_${version}_${arch}.pkg
    fi

    # Deploy app, sign, then build the package.
    macdeployqt ./SolarCoin-Qt.app -always-overwrite

    # Sign the libs with the Developer Cert.
    for f in `find ./SolarCoin-Qt.app -type f`; do
        codesign --force --verify --verbose --sign NDU9Q48556 $f
    done
    codesign --deep --force --verify --verbose --sign NDU9Q48556 ./SolarCoin-Qt.app
    pkgbuild --analyze --root ./SolarCoin-Qt.app share/qt/SolarCoin-Qt.plist
    pkgbuild --root ./SolarCoin-Qt.app --component-plist share/qt/SolarCoin-Qt.plist --identifier com.OnsightIT.SolarCoin --install-location /Applications/SolarCoin-Qt.app SolarCoin_${version}_${arch}.pkg
    # Sign the app with the Installer Cert.
    productsign --sign R5V6NFP8XB SolarCoin_${version}_${arch}.pkg SolarCoin_${version}_${arch}.signed.pkg
    mv SolarCoin_${version}_${arch}.signed.pkg SolarCoin_${version}_${arch}.pkg
    echo "Package created in: $PWD/SolarCoin_${version}_${arch}.pkg\n"
else
    echo "Error: Missing files!\n"
    echo "Run this script from the folder containing SolarCoin-Qt.app, solarcoin.conf and README.\n"
fi

