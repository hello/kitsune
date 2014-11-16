#!/bin/bash

rm -rf ~/workspace

~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./kitsune/main/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./simplelink/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./oslib/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./third_party/fatfs/ccs/
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./driverlib/ccs/

~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects  driverlib simplelink oslib fatfs
~/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects kitsune

