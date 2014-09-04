#!/bin/bash

find . -name "snoring*.raw" |xargs -P2 -I{} ./pcmrunner {} {}.mel snoring
find . -name "crying*.raw" |xargs -P2 -I{} ./pcmrunner {} {}.mel crying
find . -name "talking*.raw" |xargs -P2 -I{} ./pcmrunner {} {}.mel talking
find . -name "vehicles*.raw" | xargs -P2 -I{} ./pcmrunner {} {}.mel vehicles


