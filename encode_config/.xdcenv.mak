#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = /home/plc/workdir/dm365/dvsdk_2_10_01_18/codec_engine_2_24/examples;/home/plc/workdir/dm365/dvsdk_2_10_01_18/dvsdk_demos_2_10_00_17/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/dmai_1_21_00_10/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/codec_engine_2_24/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/framework_components_2_24/packages;/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/xdais_6_24/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/linuxutils_2_24_02/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/dm365_codecs_01_00_06/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/edma3_lld_1_06_00_01/packages
override XDCROOT = /home/plc/workdir/dm365/dvsdk_2_10_01_18/xdctools_3_15_01_59
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = /home/plc/workdir/dm365/dvsdk_2_10_01_18/codec_engine_2_24/examples;/home/plc/workdir/dm365/dvsdk_2_10_01_18/dvsdk_demos_2_10_00_17/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/dmai_1_21_00_10/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/codec_engine_2_24/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/framework_components_2_24/packages;/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/xdais_6_24/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/linuxutils_2_24_02/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/dm365_codecs_01_00_06/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/edma3_lld_1_06_00_01/packages;/home/plc/workdir/dm365/dvsdk_2_10_01_18/xdctools_3_15_01_59/packages;..
HOSTOS = Linux
endif
