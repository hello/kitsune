################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := C:/ccs/ccsv6/tools/compiler/arm_5.1.5

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./cc_pal.obj" \
"./device.obj" \
"./driver.obj" \
"./flowcont.obj" \
"./fs.obj" \
"./netapp.obj" \
"./netcfg.obj" \
"./nonos.obj" \
"./socket.obj" \
"./spawn.obj" \
"./wlan.obj" \
$(GEN_CMDS__FLAG) \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include exe/subdir_vars.mk
-include nonos/subdir_vars.mk
-include subdir_rules.mk
-include exe/subdir_rules.mk
-include nonos/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(INO_DEPS)),)
-include $(INO_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(PDE_DEPS)),)
-include $(PDE_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
LIB_OUTPUTS += \
C:/Users/chrisjohnson/Desktop/kitsune/simplelink/ccs/exe/simplelink_nonos.a \

LIB_OUTPUTS__QUOTED += \
"C:\Users\chrisjohnson\Desktop\kitsune\simplelink\ccs\exe\simplelink_nonos.a" \


# All Target
all: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/ccs/exe/simplelink_nonos.a

# Tool invocations
C:/Users/chrisjohnson/Desktop/kitsune/simplelink/ccs/exe/simplelink_nonos.a: $(OBJS) $(OBJ_SRCS) $(A_SRCS)
	@echo 'Building target: $@'
	@echo 'Invoking: ARM Archiver'
	"C:/ccs/ccsv6/tools/compiler/arm_5.1.5/bin/armar" r "C:/Users/chrisjohnson/Desktop/kitsune/simplelink/ccs/exe/simplelink_nonos.a" $(ORDERED_OBJS) $(A_SRCS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(LIB_OUTPUTS__QUOTED)
	-$(RM) "cc_pal.d" "device.d" "driver.d" "flowcont.d" "fs.d" "netapp.d" "netcfg.d" "nonos.d" "socket.d" "spawn.d" "wlan.d" 
	-$(RM) "cc_pal.obj" "device.obj" "driver.obj" "flowcont.obj" "fs.obj" "netapp.obj" "netcfg.obj" "nonos.obj" "socket.obj" "spawn.obj" "wlan.obj" 
	-@echo 'Finished clean'
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

