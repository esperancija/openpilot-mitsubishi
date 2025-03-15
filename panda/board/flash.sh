#!/usr/bin/env sh
set -e

scons -u -j$(nproc)

PYTHONPATH=.. python3 -c "from python import Panda; Panda().reset(enter_bootstub=True); Panda().reset(enter_bootloader=True)" || true
sleep 1
$DFU_UTIL -d bbaa:ddcc -a 0 -s 0x08004000 -D obj/panda.bin.signed
$DFU_UTIL -d bbaa:ddcc -a 0 -s 0x08000000:leave -D obj/bootstub.panda.bin

#PYTHONPATH=.. python3 -c "from python import Panda; Panda().flash('obj/panda.bin.signed')"
