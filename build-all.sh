#!/bin/bash
# Build script for rebuilding everything
set echo on

echo "Building everything..."

# pushd app
# source build.sh
# popd

bear --append -- make -f "Makefile.app.linux.mak" all
ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ]
then
echo "Error:"$ERRORLEVEL && exit
fi

echo "All assemblies built successfully."
