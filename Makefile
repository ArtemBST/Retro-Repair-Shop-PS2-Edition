EE_BIN = RetroRepairShop.elf
EE_OBJS = main.o
EE_LIBS = -ldebug -lpad -lkernel -lmath3d -lm -lgsKit -ldmaKit -lgraph

# Ищем заголовки gsKit во всех возможных местах установки
EE_INCS = -I$(PS2SDK)/ports/include -I$(PS2DEV)/ps2dev/ps2sdk/ports/include -I$(PS2DEV)/include
EE_LIBDIRS = -L$(PS2SDK)/ports/lib -L$(PS2DEV)/ps2dev/ps2sdk/ports/lib -L$(PS2DEV)/lib

all: $(EE_BIN)

clean:
	rm -f *.o *.elf

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
