#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/roco.png
ICON_DST=../../src/qt/res/icons/roco.ico
convert ${ICON_SRC} -resize 16x16 roco-16.png
convert ${ICON_SRC} -resize 32x32 roco-32.png
convert ${ICON_SRC} -resize 48x48 roco-48.png
convert roco-16.png roco-32.png roco-48.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/roco_testnet.png
ICON_DST=../../src/qt/res/icons/roco_testnet.ico
convert ${ICON_SRC} -resize 16x16 roco-16.png
convert ${ICON_SRC} -resize 32x32 roco-32.png
convert ${ICON_SRC} -resize 48x48 roco-48.png
convert roco-16.png roco-32.png roco-48.png ${ICON_DST}
rm roco-16.png roco-32.png roco-48.png
