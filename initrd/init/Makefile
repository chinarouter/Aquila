#ARCH=i686
#SYSROOT= ../../build-tools/sysroot
#CC= $(SYSROOT)/bin/$(ARCH)-aquila-gcc
#CFLAGS= -I$(SYSROOT)/usr/$(ARCH)-aquila/include
#
#LD= $(CC)
#LDFLAGS= -nostdlib -ffreestanding -Wl,-Ttext=0x1000
#LDLIBS= $(SYSROOT)/usr/$(ARCH)-aquila/lib/crt0.o -L$(SYSROOT)/usr/$(ARCH)-aquila/lib -lc -lpthread -lm -lgcc 

all: init

init: init.c
	$(SYSCC) $< -o $@

clean:
	rm -rf init

.PHONY: all clean
