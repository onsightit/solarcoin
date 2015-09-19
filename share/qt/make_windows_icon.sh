#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/solarcoin.ico

convert ../../src/qt/res/icons/solarcoin-16.png ../../src/qt/res/icons/solarcoin-32.png ../../src/qt/res/icons/solarcoin-48.png ${ICON_DST}
