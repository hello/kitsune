#!/bin/bash

cp -r kitsune/main/ccs/exe ./
cp kitsune/main/ccs/Release/kitsune.map ./exe
tar -zcf build.tar.gz exe

