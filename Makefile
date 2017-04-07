
CXX       =		g++
LD        =		g++
PKGCONFIG =		$(shell which pkg-config)
GIT 	  =		$(shell which git)
TAR 	  =		$(shell which tar)
MKDIR     =		$(shell which mkdir)
TOUCH 	  = 	$(shell which touch)
CP        = 	$(shell which cp)
RM 	      = 	$(shell which rm)
LN        =		$(shell which ln)
FIND 	  =		$(shell which find)
INSTALL   =		$(shell which install)
MKTEMP    =		$(shell which mktemp)
STRIP     =		$(shell which strip)
SED       =		$(shell which sed)
GZIP      =		$(shell which gzip)
RPMBUILD  =		$(shell which rpmbuild)


OBJDIR    =		obj
OUTDIR    =		bin
SRCDIR    =		src
TARGET    =		merged-fuse
MOUNT     =		mount.$(TARGET)
SRC	      =		$(wildcard $(SRCDIR)/*.cpp)
OBJ       =		$(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS      =		$(OBJ:$(OBJDIR)/%.o=$(OBJDIR)/%.d)

INC       =		-Iinclude
OPTS 	  =		-O2
LDFLAGS   =		$(shell $(PKGCONFIG) fuse --cflags --libs)
CFLAGS    =		-Wall \
				-std=c++11 \
				-g \
				-s \
				$(OPTS) \
				$(INC) \
				-D_FILE_OFFSET_BITS=64

PREFIX    =		/usr
EXEC_PREFIX    = $(PREFIX)
BINDIR         = $(EXEC_PREFIX)/bin
SBINDIR        = $(EXEC_PREFIX)/sbin
INSTALLBINDIR  = $(DESTDIR)$(BINDIR)
INSTALLSBINDIR = $(DESTDIR)$(SBINDIR)

# check pkgconfig
ifeq ($(PKGCONFIG),"")
$(error "pkg-config not installed")
endif

# check fuse/fuse-devel
FUSE_AVAILABLE = $(shell ! pkg-config --exists fuse; echo $$?)

ifeq ($(FUSE_AVAILABLE),0)
FUSE_AVAILABLE = $(shell test ! -e /usr/include/fuse.h; echo $$?)
endif

ifeq ($(FUSE_AVAILABLE),0)
$(error "FUSE development package doesn't appear available")
endif

# make
all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/$(TARGET): $(OBJDIR) $(OUTDIR) $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

$(OUTDIR)/$(MOUNT): $(OUTDIR)/$(TARGET)
	$(LN) -fs "$(TARGET)" "$@"

$(SRCDIR)/version.hpp:
	$(eval VERSION := $(shell $(GIT) describe --always --tags --dirty))
	@echo "#ifndef _VERSION_HPP" > $(SRCDIR)/version.hpp
	@echo "#define _VERSION_HPP" >> $(SRCDIR)/version.hpp
	@echo "static const char MERGERFS_VERSION[] = \"$(VERSION)\";" >> $(SRCDIR)/version.hpp
	@echo "#endif" >> $(SRCDIR)/version.hpp

$(OBJDIR):
	$(MKDIR) -p $(OBJDIR)
	$(TOUCH) $@

$(OUTDIR):
	$(MKDIR) -p $(OUTDIR)
	$(TOUCH) $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

install: install-base install-mount

install-base: $(OUTDIR)/$(TARGET)
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(INSTALL) -v -m 0755 "$(OUTDIR)/$(TARGET)" "$(INSTALLBINDIR)/$(TARGET)"

install-mount: $(OUTDIR)/$(MOUNT)
	$(MKDIR) -p "$(INSTALLBINDIR)"
	$(CP) -a "$<" "$(INSTALLBINDIR)/$(MOUNT)"

clean:
ifneq ($(GIT),)
ifeq  ($(shell test -e .git; echo $$?),0)
	$(RM) -f "src/version.hpp"
endif
endif
	$(RM) -rf "$(OBJDIR)"
	$(RM) -rf "$(OUTDIR)"
	$(RM) -r "$(OUTDIR)/$(TARGET)" "$(OUTDIR)/$(MOUNT)"

uninstall: uninstall-base uninstall-mount

uninstall-base:
	$(RM) -f "$(INSTALLBINDIR)/$(TARGET)"

uninstall-mount:
	$(RM) -f "$(INSTALLBINDIR)/$(MOUNT)"

.PHONY: all clean install

include $(wildcard obj/*.d)
