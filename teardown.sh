#!/bin/bash

echo "Copying build files to $TRAVIS_BUILD_DIR/build"
mkdir $TRAVIS_BUILD_DIR/build
cp $TRAVIS_BUILD_DIR/kitsune/main/ccs/exe/kitsune.bin $TRAVIS_BUILD_DIR/build
cp $TRAVIS_BUILD_DIR/kitsune/main/ccs/Release/kitsune.map $TRAVIS_BUILD_DIR/build

