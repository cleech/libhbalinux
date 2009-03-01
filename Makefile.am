AM_CFLAGS = $(HBAAPI_CFLAGS) $(PCIACCESS_CFLAGS)
AM_LDFLAGS= $(PCIACCESS_LIBS)

lib_LTLIBRARIES = libhbalinux.la
libhbalinux_la_SOURCES = adapt.c adapt_impl.h api_lib.h bind.c bind_impl.h \
fc_scsi.h fc_types.h lib.c lport.c net_types.h pci.c rport.c scsi.c sg.c \
utils.c utils.h
libhbalinux_la_LDFLAGS = -version-info 2:2:0

dist_noinst_DATA = libhbalinux.spec

