#
# Makefile for hbalinux
#

MAKE = make

HBAAPI=$(shell (cd hbaapi_src_2.2; pwd))

all:
	(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi)
	$(MAKE) HBAAPI=$(HBAAPI) -C libhbalinux

install:
	(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi install)
	$(MAKE) HBAAPI=$(HBAAPI) -C libhbalinux install

uninstall:
	(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi uninstall)
	$(MAKE) -C libhbalinux uninstall

clean:
	@(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi clean > /dev/null 2>&1)
	@$(MAKE) -C libhbalinux clean > /dev/null 2>&1

