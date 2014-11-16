#!/bin/bash

rm -rf ~/workspace


export CLASSPATH=~/hello/ti/ccsv6/ccs_base/DebugServer/packages/ti/dss/java/dss.jar:~/hello/ti/ccsv6/ccs_base/DebugServer/packages/ti/dss/java:$CLASSPATH

~/hello/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./kitsune/main/ccs/
~/hello/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./simplelink/ccs/
~/hello/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./oslib/ccs/
~/hello/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./third_party/fatfs/ccs/
~/hello/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectImport -ccs.location ./driverlib/ccs/

~/hello/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects  driverlib simplelink oslib fatfs
~/hello/ti/ccsv6/eclipse/eclipse -noSplash -data ~/workspace/ -application com.ti.ccstudio.apps.projectBuild -ccs.projects kitsune

