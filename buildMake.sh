#!/bin/bash
# Script for building oegw on Linux machine

#expected build dir structure
buildexp=build/Unix_Makefiles

LIB_DIRECTORY=..
currentdir=$PWD
builddir=./${buildexp}

mkdir -p ${builddir}

#launch cmake to generate build environment
pushd ${builddir}
pwd
cmake -G "Unix Makefiles" ${currentdir}
popd

#build from generated build environment
cmake --build ${builddir} --config Debug --target install

