#!/bin/sh

LIB=$1

VER_OFFSET=`readelf -V ${LIB} | grep \(.dynstr\) | cut -d' ' -f 6`
SYM_OFFSET=`readelf -V ${LIB} | grep 'Name: GLIBC_2.29' | cut -d':' -f 1`

OFFSET=$((${VER_OFFSET}+${SYM_OFFSET}+4))

printf '\002' | dd of=${LIB} bs=1 count=1 seek=${OFFSET} conv=notrunc
