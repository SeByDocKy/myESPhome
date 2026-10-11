#!/bin/bash
# usage: build_sim.sh <workdir> <yaml> <sim_main.cpp>   (workdir: the yaml + "ext" dir made by make_ext.py ext)
set -e
W=$(readlink -f "$1"); Y=$2; M=$(readlink -f "$3")
ESPH=${ESPHOME:-esphome}
AJ=${ARDUINOJSON_SRC:?set ARDUINOJSON_SRC to the ArduinoJson src directory}
cd "$W"
rm -rf .esphome obj; mkdir -p obj
$ESPH compile --only-generate "$Y" > gen.log 2>&1 || { tail -30 gen.log; exit 1; }
SRC=$(ls -d .esphome/build/*/src)
FLAGS="-std=gnu++20 -DUSE_HOST -DESPHOME_LOG_LEVEL=ESPHOME_LOG_LEVEL_VERBOSE -fno-exceptions -Wno-sign-compare -I$SRC -I$AJ -O1 -w"
N=0
for f in $(find $SRC -name '*.cpp' ! -name main.cpp); do
  o=obj/$(echo $f | tr '/.' '__').o
  X=""; case $f in *host/core.cpp) X="-Dmain=esphome_host_main";; esac
  g++ $FLAGS $X -c $f -o $o &
  N=$((N+1)); if [ $((N % 8)) -eq 0 ]; then wait; fi
done
wait
g++ $FLAGS -I. -c "$M" -o obj/sim_main.o
g++ obj/*.o -o sim -lpthread
echo BUILD OK
