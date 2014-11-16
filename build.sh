#!/bin/bash

rm -rf ~/workspace

~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ~/kitsune/kitsune/main/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ~/kitsune/simplelink/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ~/kitsune/oslib/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ~/kitsune/third_party/fatfs/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ~/kitsune/driverlib/ccs/

~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects  driverlib simplelink oslib fatfs
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects kitsune

