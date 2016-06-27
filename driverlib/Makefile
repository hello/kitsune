#******************************************************************************
#
# Makefile - Rules for building the driverlib.
#
#  Copyright (C) 2015 Texas Instruments Incorporated - http:#www.ti.com/
#
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#    Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#    Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the
#    distribution.
#
#    Neither the name of Texas Instruments Incorporated nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#*****************************************************************************

#
# Include the common make definitions.
#
include Makefile.defs

.PHONY: help all clean ccs ewarm gcc clean_ccs clean_ewarm clean_gcc clean_all

ifndef BUILD_TOOL
abort:
	make help
endif

#
# The rule to create the driverlib library
#
build: $(BUILDDIR) $(OBJ) .$(LIBRARY)

#
# The rule to display the available options
#
help:
	@echo Makefile to build driverlib component with CCS, IAR and GCC Tools
	@echo   goal              description
	@echo  -----------------------------------------------------------------------------
	@echo   "all		Builds driverlib using CCS, IAR and GCC"
	@echo   "ccs		Builds driverlib using CCS"
	@echo   "ewarm		Builds driverlib using IAR"
	@echo   "gcc		Builds driverlib using GCC"
	@echo   "clean_all	Cleans driverlib using CCS, IAR and GCC"
	@echo   "clean_ccs	Cleans driverlib using CCS"
	@echo   "clean_ewarm	Cleans driverlib using IAR"
	@echo   "clean_gcc	Cleans driverlib using GCC"
	@echo   "help		Displays this description"
	
#
# The rule to create the build directory
#
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

#
# The rule to clean the build and output directories
#
clean:
	@rm -rf $(BUILDDIR)/../exe
	@rm -rf $(BUILDDIR)/../Exe
	@rm -rf $(BUILDDIR)/../List
	@rm -rf $(BUILDDIR)	

#
# The rule to build the libraries using CCS, IAR and GCC tools
#
all: ccs ewarm gcc	

#
# The rule to clean the libraries using CCS, IAR and GCC tool
#
clean_all: clean_ccs clean_ewarm clean_gcc

#
# The rule to build the libraries using CCS tool
#
ccs:
	make BUILD_TOOL=ccs

#
# The rule to clean the libraries using CCS tool
#
clean_ccs:
	make BUILD_TOOL=ccs clean

#
# The rule to build the libraries using IAR tool
#
ewarm:
	make BUILD_TOOL=ewarm

#
# The rule to clean the libraries using IAR tool
#
clean_ewarm:
	make BUILD_TOOL=ewarm clean

#
# The rule to build the libraries using GCC tool
#
gcc:
	make BUILD_TOOL=gcc

#
# The rule to clean the libraries using CCS tool
#
clean_gcc:
	make BUILD_TOOL=gcc clean