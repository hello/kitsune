#!/bin/bash

echo "Copying build files to $TRAVIS_HOME/output"
mkdir $TRAVIS_HOME/output
cp $TRAVIS_BUILD_DIR/kitsune/main/ccs/exe/kitsune.bin $TRAVIS_HOME/output/
cp $TRAVIS_BUILD_DIR/kitsune/main/ccs/Release/kitsune.map $TRAVIS_HOME/output/

