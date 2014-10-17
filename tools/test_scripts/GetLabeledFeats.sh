#!/bin/bash

find . -name "snoring*.raw" |xargs -P4 -I{} ./pcmrunner {} {}.mel snoring
find . -name "crying*.raw" |xargs -P4 -I{} ./pcmrunner {} {}.mel not
find . -name "talking*.raw" |xargs -P4 -I{} ./pcmrunner {} {}.mel not
find . -name "vehicles*.raw" | xargs -P4 -I{} ./pcmrunner {} {}.mel not


