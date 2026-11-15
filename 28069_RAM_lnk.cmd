
/*
//#############################################################################
// FILE:    28069_RAM_lnk.cmd
//
// TITLE:   Game Guy - Flash-Based Debug Linker Command File
// DEVICE:  TMS320F28069M
//
// DESCRIPTION:
//   Flash-based execution with Debug symbols and RAM-resident functions.
//
//   FLASH A-E: Program code and constants
//   FLASH F:   Splash screen and additional constants
//   FLASH G/H: Reserved for Game Guy settings and high scores
//
//   RAML0-L4: RAM-resident functions
//   RAML5-L6: Global and static variables
//   RAML7:    Heap
//   RAML8:    C I/O
//#############################################################################
*/

MEMORY
{
PAGE 0 : /* PROGRAM MEMORY */

    RAML0_4     : origin = 0x008000, length = 0x004000

    OTP         : origin = 0x3D7800, length = 0x000400

    /* Flash G and H intentionally reserved for drivers/save.c */

    FLASHF      : origin = 0x3E0000, length = 0x004000
    FLASHE      : origin = 0x3E4000, length = 0x004000

    FLASHCD     : origin = 0x3E8000, length = 0x008000

    FLASHB      : origin = 0x3F0000, length = 0x004000

    FLASHA      : origin = 0x3F4000, length = 0x003F80

    CSM_RSVD    : origin = 0x3F7F80, length = 0x000076
    BEGIN       : origin = 0x3F7FF6, length = 0x000002
    CSM_PWL_P0  : origin = 0x3F7FF8, length = 0x000008

    FPUTABLES   : origin = 0x3FD860, length = 0x0006A0
    IQTABLES    : origin = 0x3FDF00, length = 0x000B50
    IQTABLES2   : origin = 0x3FEA50, length = 0x00008C
    IQTABLES3   : origin = 0x3FEADC, length = 0x0000AA

    ROM         : origin = 0x3FF3B0, length = 0x000C10
    RESET       : origin = 0x3FFFC0, length = 0x000002
    VECTORS     : origin = 0x3FFFC2, length = 0x00003E


PAGE 1 : /* DATA MEMORY */

    BOOT_RSVD   : origin = 0x000000, length = 0x000050
    RAMM0       : origin = 0x000050, length = 0x0003B0
    RAMM1       : origin = 0x000400, length = 0x000400

    RAML5       : origin = 0x00C000, length = 0x002000
    RAML6       : origin = 0x00E000, length = 0x002000
    RAML7       : origin = 0x010000, length = 0x002000
    RAML8       : origin = 0x012000, length = 0x002000

    USB_RAM     : origin = 0x040000, length = 0x000800
}


/*#############################################################################
   SECTION ALLOCATION
#############################################################################*/

SECTIONS
{
    /* Program initialization */
    .cinit          : > FLASHCD, PAGE = 0
    .pinit          : > FLASHCD, PAGE = 0

    /* Executable code distributed across Flash */
    .text           : { *(.text) } >> FLASHA | FLASHB | FLASHCD, PAGE = 0

    /* Flash boot entry */
    codestart       : > BEGIN, PAGE = 0

    /* RAM-resident functions loaded from Flash */
#ifdef __TI_COMPILER_VERSION__
    #if __TI_COMPILER_VERSION__ >= 15009000

    .TI.ramfunc     : {}
        LOAD = FLASHCD,
        RUN = RAML0_4,
        LOAD_START(_RamfuncsLoadStart),
        LOAD_END(_RamfuncsLoadEnd),
        RUN_START(_RamfuncsRunStart),
        LOAD_SIZE(_RamfuncsLoadSize),
        PAGE = 0

    #else

    ramfuncs        :
        LOAD = FLASHCD,
        RUN = RAML0_4,
        LOAD_START(_RamfuncsLoadStart),
        LOAD_END(_RamfuncsLoadEnd),
        RUN_START(_RamfuncsRunStart),
        LOAD_SIZE(_RamfuncsLoadSize),
        PAGE = 0

    #endif
#endif

    /* CSM */
    csmpasswds      : > CSM_PWL_P0, PAGE = 0
    csm_rsvd        : > CSM_RSVD, PAGE = 0


    /* Runtime data */
    .stack          : > RAMM0, PAGE = 1

    .ebss           : >> RAML5 | RAML6, PAGE = 1

    .esysmem        : > RAML7, PAGE = 1

    .cio            : > RAML8, PAGE = 1


    /* Game Guy assets */
    .splash         : > FLASHF, PAGE = 0

    .econst         : >> FLASHE | FLASHF, PAGE = 0

    .switch         : > FLASHF, PAGE = 0


    /* IQMath */
    IQmath          : > FLASHE, PAGE = 0

    IQmathTables    : > IQTABLES, PAGE = 0, TYPE = NOLOAD


    /* FPU tables */
    FPUmathTables   : > FPUTABLES, PAGE = 0, TYPE = NOLOAD


    /* DMA RAM */
    DMARAML5        : > RAML5, PAGE = 1
    DMARAML6        : > RAML6, PAGE = 1
    DMARAML7        : > RAML7, PAGE = 1
    DMARAML8        : > RAML8, PAGE = 1


    /* Reset and interrupt vectors */
    .reset          : > RESET, PAGE = 0, TYPE = DSECT
    vectors         : > VECTORS, PAGE = 0, TYPE = DSECT
}

/*
//#############################################################################
// END OF FILE
//#############################################################################
*/
