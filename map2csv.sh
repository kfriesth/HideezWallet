#!/bin/bash

PATH=/bin:/sbin:/usr/bin:$PATH

cat exe/lst/hideezCoin.map | grep ' Data ' | sort -k 4 -rn
