################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./F2806x_CodeStartBranch.obj" \
"./F2806x_usDelay.obj" \
"./main.obj" \
"./board_support/F2806x_CpuTimers.obj" \
"./board_support/F2806x_DefaultIsr.obj" \
"./board_support/F2806x_GlobalVariableDefs.obj" \
"./board_support/F2806x_Gpio.obj" \
"./board_support/F2806x_PieCtrl.obj" \
"./board_support/F2806x_PieVect.obj" \
"./board_support/F2806x_SysCtrl.obj" \
"./drivers/joystick.obj" \
"./grlib/context.obj" \
"./grlib/display.obj" \
"./grlib/fontcm20b.obj" \
"./grlib/line.obj" \
"./grlib/rectangle.obj" \
"./grlib/string.obj" \
"./hal/HAL_F28069_KITRONIX320X240_SSD2119_SPI.obj" \
"./hal/kitronix320x240x16_ssd2119_spi.obj" \
"../28069_RAM_lnk.cmd" \
"../F2806x_Headers_nonBIOS.cmd" \
$(GEN_CMDS__FLAG) \
-llibc.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include board_support/subdir_vars.mk
-include drivers/subdir_vars.mk
-include grlib/subdir_vars.mk
-include hal/subdir_vars.mk
-include subdir_rules.mk
-include board_support/subdir_rules.mk
-include drivers/subdir_rules.mk
-include grlib/subdir_rules.mk
-include hal/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
Ex_4_1.out 

EXE_OUTPUTS__QUOTED += \
"Ex_4_1.out" 


# All Target
all: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "Ex_4_1.out"

# Tool invocations
Ex_4_1.out: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'C2000 Linker - building target: "$@"'
	"C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla0 --float_support=fpu32 --vcu_support=vcu0 --define=_DEBUG --undefine=_RELEASE -g --c99 --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --ramfunc=off -z -m"Ex_4_1.map" --heap_size=4 --stack_size=0x300 --warn_sections -i"C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/lib" -i"C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --reread_libs --diag_wrap=off --display_error_number --xml_link_info="Ex_4_1_linkInfo.xml" --rom_model -o "Ex_4_1.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXE_OUTPUTS__QUOTED)
	-$(RM) "F2806x_CodeStartBranch.obj" "F2806x_usDelay.obj" "main.obj" "board_support\F2806x_CpuTimers.obj" "board_support\F2806x_DefaultIsr.obj" "board_support\F2806x_GlobalVariableDefs.obj" "board_support\F2806x_Gpio.obj" "board_support\F2806x_PieCtrl.obj" "board_support\F2806x_PieVect.obj" "board_support\F2806x_SysCtrl.obj" "drivers\joystick.obj" "grlib\context.obj" "grlib\display.obj" "grlib\fontcm20b.obj" "grlib\line.obj" "grlib\rectangle.obj" "grlib\string.obj" "hal\HAL_F28069_KITRONIX320X240_SSD2119_SPI.obj" "hal\kitronix320x240x16_ssd2119_spi.obj" 
	-$(RM) "main.d" "board_support\F2806x_CpuTimers.d" "board_support\F2806x_DefaultIsr.d" "board_support\F2806x_GlobalVariableDefs.d" "board_support\F2806x_Gpio.d" "board_support\F2806x_PieCtrl.d" "board_support\F2806x_PieVect.d" "board_support\F2806x_SysCtrl.d" "drivers\joystick.d" "grlib\context.d" "grlib\display.d" "grlib\fontcm20b.d" "grlib\line.d" "grlib\rectangle.d" "grlib\string.d" "hal\HAL_F28069_KITRONIX320X240_SSD2119_SPI.d" "hal\kitronix320x240x16_ssd2119_spi.d" 
	-$(RM) "F2806x_CodeStartBranch.d" "F2806x_usDelay.d" 
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

