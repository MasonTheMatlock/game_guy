################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
grlib/%.obj: ../grlib/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler: "$<"'
	"C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla0 --float_support=fpu32 --vcu_support=vcu0 -Ooff --include_path="C:/Users/Mason/Downloads/Ex_4_1_260909/Rectangle_Test/Rectangle_Test/game_guy/grlib" --include_path="C:/Users/Mason/Downloads/Ex_4_1_260909/Rectangle_Test/Rectangle_Test/game_guy/hal" --include_path="C:/Users/Mason/Downloads/Ex_4_1_260909/Rectangle_Test/Rectangle_Test/game_guy" --include_path="C:/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --include_path="/device_support/f2806x/common/include" --include_path="/device_support/f2806x/headers/include" --define=_INLINE --define=_RELEASE --undefine=_DEBUG --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --ramfunc=off --preproc_with_compile --preproc_dependency="grlib/$(basename $(<F)).d_raw" --obj_directory="grlib" $(GEN_OPTS__FLAG) "$<"


