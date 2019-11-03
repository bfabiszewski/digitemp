#
# Makefile for DigiTemp
#
# Copyright 1996-2018 by Brian C. Lane <bcl@brianlane.com>
# See COPYING for GNU General Public License
#
# Please note that this Makefile *needs* GNU make. BSD make won't do.
#

VERSION=$(shell awk '/Version:/ { print $$2 }' digitemp.spec)

SRCDIR	= $(CURDIR)
VPATH	= $(SRCDIR)

# May be overridden by the command line
CFLAGS ?= -O2 -Wall # -g

# Mandatory additions to CFLAGS
EXTRACFLAGS	= -I$(SRCDIR)/src -I$(SRCDIR)/userial -I/usr/include/mysql
LIBS    += -L/usr/lib/mysql -lmysqlclient -lm -lz
override CFLAGS	+= $(EXTRACFLAGS)

OBJS		=	src/digitemp.o src/device_name.o src/ds2438.o
HDRS		= 	src/digitemp.h src/device_name.h

# Common userial header/source
HDRS		+=	userial/ownet.h userial/owproto.h userial/ad26.h \
			src/device_name.h src/digitemp.h
OBJS		+=	userial/crcutil.o userial/ioutil.o userial/swt1f.o \
			userial/owerr.o userial/cnt1d.o userial/ad26.o

# DS9097 passive adapter support source
DS9097OBJS	=	userial/ds9097/ownet.o userial/ds9097/linuxlnk.o \
			userial/ds9097/linuxses.o userial/ds9097/owtran.o \
                        src/ds9097.o

# DS9097-U adapter support source
DS9097UOBJS	=	userial/ds9097u/ds2480ut.o userial/ds9097u/ownetu.o \
			userial/ds9097u/owllu.o userial/ds9097u/owsesu.o \
			userial/ds9097u/owtrnu.o userial/ds9097u/linuxlnk.o \
                        src/ds9097u.o

# DS2490 adapter support
DS2490OBJS	=	userial/ds2490/ownet.o userial/ds2490/owtran.o \
			userial/ds2490/usblnk.o userial/ds2490/usbses.o \
			src/ds2490.o

# -----------------------------------------------------------------------
# Sort out what operating system is being run and modify CFLAGS and LIBS
#
# If you add a new OSTYPE here please email it to me so that I can add
# it to the distribution in the next release
# -----------------------------------------------------------------------
SYSTYPE := $(shell uname -s)

ifneq (, $(findstring CYGWIN,$(SYSTYPE)))
  EXTRACFLAGS += -DCYGWIN
  LIBS   += -static -static-libgcc
endif

ifeq ($(SYSTYPE), SunOS)
  EXTRACFLAGS += -DSOLARIS
  LIBS   += -lposix4
endif

ifeq ($(SYSTYPE), FreeBSD)
  EXTRACFLAGS += -DFREEBSD
endif

ifeq ($(SYSTYPE), Darwin)
  EXTRACFLAGS += -DDARWIN
endif

ifeq ($(SYSTYPE), AIX)
  EXTRACFLAGS += -DAIX
endif


# USB specific flags
ds2490:  EXTRACFLAGS += -DOWUSB
ds2490:  LIBS   += -lusb


help:
	@echo "  SYSTYPE = $(SYSTYPE)"
	@echo "  CFLAGS = $(CFLAGS)"
	@echo "  LIBS   = $(LIBS)"
	@echo ""
	@echo "Pick one of the following targets:"
	@echo -e "\tmake ds9097\t- Build version for DS9097 (passive)"
	@echo -e "\tmake ds9097u\t- Build version for DS9097U"
	@echo -e "\tmake ds2490\t- Build version for DS2490 (USB) (edit Makefile) (BROKEN)"
	@echo " "
	@echo ""
	@echo "Please note: You must use GNU make to compile digitemp"
	@echo ""

all:		help


# Build the Linux executable
ds9097:		$(OBJS) $(HDRS) $(ONEWIREOBJS) $(ONEWIREHDRS) $(DS9097OBJS)
		$(CC) $(OBJS) $(ONEWIREOBJS) $(DS9097OBJS) -o digitemp_DS9097 $(LDFLAGS) $(LIBS)

ds9097u:	$(OBJS) $(HDRS) $(ONEWIREOBJS) $(ONEWIREHDRS) $(DS9097UOBJS)
		$(CC) $(OBJS) $(ONEWIREOBJS) $(DS9097UOBJS) -o digitemp_DS9097U $(LDFLAGS) $(LIBS)

ds2490:		$(OBJS) $(HDRS) $(ONEWIREOBJS) $(ONEWIREHDRS) $(DS2490OBJS)
		$(CC) $(OBJS) $(ONEWIREOBJS) $(DS2490OBJS) -o digitemp_DS2490 $(LDFLAGS) $(LIBS)


# Clean up the object files and the sub-directory for distributions
clean:
		rm -f *~ src/*~ userial/*~ userial/ds9097/*~ userial/ds9097u/*~ userial/ds2490/*~
		rm -f $(OBJS) $(ONEWIREOBJS) $(DS9097OBJS) $(DS9097UOBJS) $(DS2490OBJS)
		rm -f core *.asc 
		rm -f perl/*~ rrdb/*~ .digitemprc .digitemprc_mysql digitemp-$(VERSION)-1.spec
		rm -rf digitemp-$(VERSION)

# Sign the binaries using gpg (www.gnupg.org)
# My key is available from the keyservers or
# https://www.brianlane.com/0xD29845A70F5017DE.txt
sign:
		gpg -ba digitemp_DS*
		echo

tag:
		git tag -s -u 0x3085CEE24BECD24B -m "Tag as v$(VERSION)" v$(VERSION)

# Install digitemp into /usr/local/bin
install:	digitemp
		install -b -o root -g bin digitemp /usr/bin

# Build the archive of everything
archive:	clean
		git archive --format=tar --prefix=digitemp-$(VERSION)/ v$(VERSION) > v$(VERSION).tar
		gzip -9 v$(VERSION).tar
		@echo "The archive is in v$(VERSION).tar.gz"

rpmlog:
		@git log --pretty="format:- %s (%ae)" v$(VERSION).. |sed -e 's/@.*)/)/' | grep -v "Merge pull request"

bumpver:
		@NEWSUBVER=$$((`echo $(VERSION) |cut -d . -f 3` + 1)) ; \
		NEWVERSION=`echo $(VERSION).$$NEWSUBVER |cut -d . -f 1,2,4` ; \
		DATELINE="* `date "+%a %b %d %Y"` `git config user.name` <`git config user.email`>  - $$NEWVERSION-1"  ; \
		cl=`grep -n %changelog digitemp.spec |cut -d : -f 1` ; \
		tail --lines=+$$(($$cl + 1)) digitemp.spec > speclog ; \
		(head -n $$cl digitemp.spec ; echo "$$DATELINE" ; make --quiet rpmlog 2>/dev/null ; echo ""; cat speclog) > digitemp.spec.new ; \
		mv digitemp.spec.new digitemp.spec ; rm -f speclog ; \
		sed -i "s/Version:.*$(VERSION)/Version:           $$NEWVERSION/" digitemp.spec ; \
		sed -i "s/$(VERSION)/$$NEWVERSION/" README ; \
		sed -i "s/$(VERSION)/$$NEWVERSION/" COPYRIGHT ; \
		sed -i "s/$(VERSION)/$$NEWVERSION/" ./src/digitemp.h

# Build the source distribution
source:		archive

dist:		ds9097 ds9097u ds2490 sign archive

dist_ds9097:	ds9097 sign archive
		cd .. && mv digitemp-$(VERSION).tar.gz digitemp-$(VERSION)-ds9097.tar.gz

dist_ds9097u:	ds9097u sign archive
		cd .. && mv digitemp-$(VERSION).tar.gz digitemp-$(VERSION)-ds9097u.tar.gz

dist_ds2490:	ds2490 sign archive
		cd .. && mv digitemp-$(VERSION).tar.gz digitemp-$(VERSION)-ds2490.tar.gz
