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

PSX_SCREEN_DUMPER_VERSION := $(shell perl -ne 'print $$1 if($$_ =~ /PSX_SCREEN_DUMPER_VERSION\s"(v[^"]+)"/)' psx_screen_dumper.c)

release: $(BINDIR)$(TARGET).ps-exe $(BINDIR)$(TARGET).iso README.md CHANGELOG.md LICENSE
	mkdir psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION)
	cp $(BINDIR)$(TARGET).ps-exe psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION)/
	cp $(BINDIR)$(TARGET).iso psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION)/
	cp README.md psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION)/
	cp CHANGELOG.md psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION)/
	cp LICENSE psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION)/
	zip -r psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION).zip psx_screen_dumper_$(PSX_SCREEN_DUMPER_VERSION)

.PHONY: nocash iso actualclean release