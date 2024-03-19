# path to your Osx cross-compiler
CC=~/opt/osxcross/bin/o64-clang

GCOM=`git  rev-parse --short HEAD`


$CC -g3 -O0 -DNO_CLOSE -DGCOM="\"g${GCOM}\"" -o afterburner_osx  src_pc/afterburner.c
