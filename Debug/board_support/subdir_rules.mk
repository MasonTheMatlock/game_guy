################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
board_support/%.obj: ../board_support/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla0 --float_support=fpu32 --vcu_support=vcu0 --include_path="C:/Users/Mason/Downloads/Ex_4_1_260909/Rectangle_Test/Rectangle_Test/Ex_4_1/hal" --include_path="C:/Users/Mason/Downloads/Ex_4_1_260909/Rectangle_Test/Rectangle_Test/Ex_4_1" --include_path="C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --include_path="C:/ti/c2000/C2000Ware_26_01_00_00/device_support/f2806x/common/include" --include_path="C:/ti/c2000/C2000Ware_26_01_00_00/device_support/f2806x/headers/include" --include_path="C:/Users/Mason/Downloads/Ex_4_1_260909/Rectangle_Test/Rectangle_Test/Ex_4_1/grlib" --define=_DEBUG --undefine=_RELEASE -g --c99 --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --ramfunc=off --preproc_with_compile --preproc_dependency="board_support/$(basename $(<F)).d_raw" --obj_directory="board_support" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


