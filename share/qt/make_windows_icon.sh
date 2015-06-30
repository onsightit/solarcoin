#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/vericoin.ico

convert ../../src/qt/res/icons/vericoin-16.png ../../src/qt/res/icons/vericoin-32.png ../../src/qt/res/icons/vericoin-48.png ${ICON_DST}
