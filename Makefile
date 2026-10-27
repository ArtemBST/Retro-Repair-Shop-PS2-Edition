EE_BIN = RetroRepairShop.elf
EE_OBJS = main.o
EE_LIBS = -ldebug -lpad -lkernel -lmath3d -lm -lgsKit -ldmaKit -lgraph


# Указываем компилятору, где лежат заголовки gsKit
EE_INCS = -I$(PS2SDK)/ports/include

all: $(EE_BIN)

clean:
	rm -f *.o *.elf

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
