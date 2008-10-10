#
# Makefile for hbalinux
#

MAKE = make

HBAAPI=$(shell (cd hbaapi_src_2.2; pwd))

all:
	(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi)
	$(MAKE) HBAAPI=$(HBAAPI) -C src

install:
	(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi install)
	$(MAKE) HBAAPI=$(HBAAPI) -C src install

uninstall:
	(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi uninstall)
	$(MAKE) -C src uninstall

clean:
	@(cd $(HBAAPI); $(MAKE) -f ../Makefile.hbaapi clean > /dev/null 2>&1)
	@$(MAKE) -C src clean > /dev/null 2>&1

