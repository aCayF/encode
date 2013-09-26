## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: linker.cmd

linker.cmd: \
  package/cfg/encode_x470MV.o470MV \
  package/cfg/encode_x470MV.xdl
	$(SED) 's"^\"\(package/cfg/encode_x470MVcfg.cmd\)\"$""\"/home/plc/workdir/dm365/dvsdk_2_10_01_18/dvsdk_demos_2_10_00_17/dm365/encode/encode_config/\1\""' package/cfg/encode_x470MV.xdl > $@
