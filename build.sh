#!/bin/bash

~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace_v6_03/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects  driverlib simplelink oslib fatfs
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace_v6_03/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects kitsune

