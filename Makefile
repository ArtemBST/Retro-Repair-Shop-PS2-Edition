EE_BIN = RetroRepairShop.elf
EE_OBJS = main.o
EE_LIBS = -ldebug -lpad -lkernel -lsifrpc -lmath3d -lm

all: $(EE_BIN)

clean:
	rm -f *.o *.elf

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
