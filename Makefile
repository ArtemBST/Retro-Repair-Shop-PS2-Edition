EE_BIN = RetroRepairShop.elf
EE_OBJS = main.o
EE_LIBS = -ldebug -lpad -lkernel -lmath3d -lm -lgsKit -ldmaKit -lgraph

# Указываем, где лежат заголовки (include) и сами библиотеки (lib) для gsKit
EE_INCS = -I$(PS2SDK)/ports/include
EE_LIBDIRS = -L$(PS2SDK)/ports/lib

all: $(EE_BIN)

clean:
	rm -f *.o *.elf

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
