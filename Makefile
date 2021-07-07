TARGET = psx_screen_dumper
TYPE = ps-exe

SRCS = psx_screen_dumper.c \
crc.c \
thirdparty/nugget/common/crt0/crt0.s \

CPPFLAGS += -Ithirdparty/nugget/psyq/include
LDFLAGS += -Lthirdparty/nugget/psyq/lib
LDFLAGS += -Wl,--start-group
LDFLAGS += -lapi
LDFLAGS += -lc
LDFLAGS += -lc2
LDFLAGS += -lcard
LDFLAGS += -lcomb
LDFLAGS += -lds
LDFLAGS += -letc
LDFLAGS += -lgpu
LDFLAGS += -lgs
LDFLAGS += -lgte
LDFLAGS += -lgun
LDFLAGS += -lhmd
LDFLAGS += -lmath
LDFLAGS += -lmcrd
LDFLAGS += -lmcx
LDFLAGS += -lpad
LDFLAGS += -lpress
LDFLAGS += -lsio
LDFLAGS += -lsnd
LDFLAGS += -lspu
LDFLAGS += -ltap
LDFLAGS += -Wl,--end-group

include thirdparty/nugget/common.mk

# extension must be .exe for no$psx
$(BINDIR)$(TARGET).exe: $(BINDIR)$(TARGET).ps-exe
	cp $(BINDIR)$(TARGET).ps-exe $(BINDIR)$(TARGET).exe

$(BINDIR)$(TARGET).objdump.txt: $(BINDIR)$(TARGET).elf
	mipsel-linux-gnu-objdump -D $(BINDIR)$(TARGET).elf > $(BINDIR)$(TARGET).objdump.txt

nocash: $(BINDIR)$(TARGET).exe $(BINDIR)$(TARGET).objdump.txt	

$(BINDIR)$(TARGET).iso: $(BINDIR)$(TARGET).ps-exe psx_screen_dumper.xml
	mkpsxiso psx_screen_dumper.xml

iso: $(BINDIR)$(TARGET).iso

actualclean: clean
	rm -f $(BINDIR)$(TARGET).iso
	rm -f $(BINDIR)$(TARGET).objdump.txt
	
.PHONY: nocash iso actualclean