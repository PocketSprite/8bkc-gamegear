#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component.mk. By default, 
# this will take the sources in this directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

COMPONENT_ADD_INCLUDEDIRS := .

COMPILEDATE:=\"$(shell date "+%d %b %Y")\"
GITREV:=\"$(shell git rev-parse HEAD | cut -b 1-10)\"

CFLAGS += -O3 
CFLAGS += -DCOMPILEDATE="$(COMPILEDATE)" -DGITREV="$(GITREV)"

COMPONENTS_EXTRA_CLEAN := graphics.inc graphics.rgba
menu.o: graphics.inc

graphics.inc: $(COMPONENT_PATH)/graphics.xcf
	convert $^ -background none -layers flatten -crop 80x324+0+0 graphics.rgba
	cat graphics.rgba | xxd -i > graphics.inc
