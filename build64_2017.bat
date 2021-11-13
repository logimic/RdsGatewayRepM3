set buildexp=build\\VS15_2017_x64

set currentdir=%cd%
set builddir=.\\%buildexp%
set libsdir=..\\

mkdir %builddir%

rem //launch cmake to generate build environment
pushd %builddir%
cmake -G "Visual Studio 15 2017 Win64" %currentdir%
popd

rem //build from generated build environment
cmake --build %builddir% --config Debug
cmake --build %builddir% --config Release
