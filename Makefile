CC_RU=/opt/freescale/usr/local/gcc-4.5.55-eglibc-2.11.55/powerpc-linux-gnu/bin/powerpc-linux-gnu-gcc
CC_CU=/opt/fsl-networking/QorIQ-SDK-V1.5/sysroots/i686-fsl_networking_sdk-linux/usr/bin/ppce6500-fsl_networking-linux/powerpc-fsl_networking-linux-gcc

SRC_CAP=pkt_cap.c
SRC_GEN=pkt_gen.c

OBJ_CAP:=$(patsubst %.c,%.o,$(SRC_CAP))
OBJ_GEN:=$(patsubst %.c,%.o,$(SRC_GEN))

all: pkt_gen pkt_cap gen_host

gen_host: $(SRC_GEN)
	$(CC) -o $@ $<

pkt_cap: $(OBJ_CAP)
	$(CC_CU) -o $@ $(OBJ_CAP)

pkt_gen: $(OBJ_GEN)
	$(CC_RU) -o $@ $(OBJ_GEN)

$(OBJ_CAP): $(SRC_CAP)
	$(CC_CU) -Wall -c -o "$@" "$<"

$(OBJ_GEN): $(SRC_GEN)
	$(CC_RU) -Wall -c -o "$@" "$<"		

.PHONY: clean
clean:
	rm -f $(OBJ_GEN) $(OBJ_CAP) pkt_gen pkt_cap
