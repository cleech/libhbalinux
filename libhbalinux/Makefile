#
# Makefile for libhbalinux
#

#
# The target shared library to be built
#
LIB = libhbalinux.so

#
# List of legal build component names
#
LEGAL_ARCH = i386 i486 i586 i686 x86_64
LEGAL_OS = linux Linux

#
# Default build directory
#
BUILD_DIR := .

#
# Commands used for build
#
CC = cc 
RM = rm
SED = sed
INSTALL = install

#
#
# Validating OS and the machine architecture.
#
ifneq "$(filter-out $(LEGAL_ARCH), $(shell uname -i))" ""
    $(error bad build architecture $(shell uname -i))
else
ifneq "$(filter-out $(LEGAL_OS), $(shell uname -s))" ""
    $(error bad build OS $(shell uname -s))
else

ifeq ($(shell uname -i),x86_64)
    CFLAGS += -m64
    .LIBPATTERNS = /usr/lib64/lib%.so /usr/lib64/lib%.a
    INSTALL_DIR := /usr/local/lib64
else
    .LIBPATTERNS = /usr/lib/lib%.so \
                   /usr/lib/lib%.a \
                   /usr/local/lib/lib%.a \
                   /usr/local/lib/lib%.so
    INSTALL_DIR := /usr/local/lib
endif

LIBRARIES = -lrt -lpciaccess

#
# Set up the C compiler flags
#
#DATE=`date "+%D-%T"`
DATE=`date "+%Y/%m/%d %T %Z"`
CFLAGS += -DBUILD_DATE="\"${DATE}\""
CFLAGS += -fPIC -O0 -g -Wall -Wall -Werror
CFLAGS += -I.
CFLAGS += -I../include
CFLAGS += -I../hbaapi_src_2.2

#
# C files to be compiled
#
SOURCES += \
	lib.c \
	adapt.c \
	lport.c \
	rport.c \
	bind.c \
	pci.c \
	scsi.c \
	sg.c \
	utils.c \
	$(NULL)

#
# The make rules
#

all: $(LIB)

LIB_SO := $(filter %.so, $(LIB))
PICS := $(basename $(SOURCES))
PICS := $(PICS:%=$(BUILD_DIR)/%.o)

$(LIB_SO): $(PICS)
	@echo '       LINK' $@; \
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ $^ $(LIBRARIES)

$(BUILD_DIR)/%.o: %.c
	@echo '       CC PIC' $<; \
	$(CC) -MM $(CFLAGS) -fpic $< | \
		( $(SED) 's,$*\.o[ :]*,$@: ,g' > $(BUILD_DIR)/$*.d || \
		$(RM) -f $(BUILD_DIR)/$*.d ); \
	$(CC) -c $(CFLAGS) -fpic -o $@ $<

clean:
	@$(RM) -f *.o *.d $(LIB) > /dev/null 2>&1

install: libhbalinux.so
	@echo '       INSTALL' $<
	@$(INSTALL) libhbalinux.so $(INSTALL_DIR)

uninstall:
	@$(RM) -f $(INSTALL_DIR)/libhbalinux.so > /dev/null 2>&1

-include $(PICS:%.o=%.d)

endif
endif
