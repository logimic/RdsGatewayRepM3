set buildexp=build\\VS17_2022_x64

set currentdir=%cd%
set builddir=.\\%buildexp%
set libsdir=..\\

mkdir %builddir%

rem //get path to to Shape libs
set shape=..\\..\\shape\\%buildexp%
pushd %shape%
set shape=%cd%
popd

rem //get path to to Shapeware libs
set shapeware=..\\..\\shapeware\\%buildexp%
pushd %shapeware%
set shapeware=%cd%
popd

rem //get path to to oegw libs
set oegw=..\\..\\logimic\\oegateway\\%buildexp%
pushd %oegw%
set oegw=%cd%
popd

set vcpkg="c:/devel/vcpkg/scripts/buildsystems/vcpkg.cmake"

set ver=v1.0.0dev

rem //launch cmake to generate build environment
pushd %builddir%
cmake -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=%vcpkg% -Dshape_DIR:PATH=%shape% -Dshapeware_DIR:PATH=%shapeware% -Doegw_DIR:PATH=%oegw% -DCHSGW_VERSION:STRING=%ver% %currentdir%
popd

rem //build from generated build environment
cmake --build %builddir% --config Debug
cmake --build %builddir% --config Release
