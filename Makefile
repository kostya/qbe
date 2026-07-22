.POSIX:
.SUFFIXES: .o .c

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

COMMOBJ  = util.o parse.o abi.o cfg.o mem.o ssa.o alias.o load.o \
           copy.o fold.o gvn.o gcm.o simpl.o ifopt.o live.o spill.o rega.o \
           emit.o
AMD64OBJ = amd64/targ.o amd64/sysv.o amd64/isel.o amd64/emit.o amd64/winabi.o
ARM64OBJ = arm64/targ.o arm64/abi.o arm64/isel.o arm64/emit.o
RV64OBJ  = rv64/targ.o rv64/abi.o rv64/isel.o rv64/emit.o

BACKOBJ  = $(COMMOBJ) $(AMD64OBJ) $(ARM64OBJ) $(RV64OBJ)

LIBOBJ   = libqbe.o $(BACKOBJ)

CLIOBJ   = main.o

SRCALL   = main.c libqbe.c $(BACKOBJ:.o=.c)

CC       = cc
CFLAGS   = -std=c99 -g -Wall -Wextra -Wpedantic
AR       = ar
ARFLAGS  = rcs

all: qbe libqbe.a libqbe.so

qbe: $(CLIOBJ) libqbe.a
	$(CC) $(LDFLAGS) $(CLIOBJ) -L. -lqbe -o $@

libqbe.a: $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $(LIBOBJ)

libqbe.so: $(LIBOBJ)
	$(CC) -shared $(LDFLAGS) $(LIBOBJ) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(BACKOBJ): all.h ops.h
$(AMD64OBJ): amd64/all.h
$(ARM64OBJ): arm64/all.h
$(RV64OBJ): rv64/all.h
main.o: libqbe.h all.h ops.h config.h
libqbe.o: libqbe.h all.h ops.h config.h

config.h:
	@case `uname` in                               \
	*Darwin*)                                      \
		case `uname -m` in                     \
		*arm64*)                               \
			echo "#define Deftgt T_arm64_apple";\
			;;                             \
		*)                                     \
			echo "#define Deftgt T_amd64_apple";\
			;;                             \
		esac                                   \
		;;                                     \
	*)                                             \
		case `uname -m` in                     \
		*aarch64*|*arm64*)                     \
			echo "#define Deftgt T_arm64"; \
			;;                             \
		*riscv64*)                             \
			echo "#define Deftgt T_rv64";  \
			;;                             \
		*)                                     \
			echo "#define Deftgt T_amd64_sysv";\
			;;                             \
		esac                                   \
		;;                                     \
	esac > $@

install: all
	mkdir -p "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(LIBDIR)" "$(DESTDIR)$(INCLUDEDIR)"
	install -m755 qbe "$(DESTDIR)$(BINDIR)/qbe"
	install -m644 libqbe.a "$(DESTDIR)$(LIBDIR)/libqbe.a"
	install -m644 libqbe.h "$(DESTDIR)$(INCLUDEDIR)/libqbe.h"
	install -m755 libqbe.so "$(DESTDIR)$(LIBDIR)/libqbe.so"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/qbe"
	rm -f "$(DESTDIR)$(LIBDIR)/libqbe.a"
	rm -f "$(DESTDIR)$(INCLUDEDIR)/libqbe.h"
	rm -f "$(DESTDIR)$(LIBDIR)/libqbe.so"

clean:
	rm -f *.o */*.o qbe libqbe.a libqbe.so

clean-gen: clean
	rm -f config.h

check: qbe
	tools/test.sh all

check-x86_64: qbe
	TARGET=x86_64 tools/test.sh all

check-arm64: qbe
	TARGET=arm64 tools/test.sh all

check-rv64: qbe
	TARGET=rv64 tools/test.sh all

check-amd64_win: qbe
	TARGET=amd64_win tools/test.sh all

src:
	@echo $(SRCALL)

80:
	@for F in $(SRCALL);                       \
	do                                         \
		awk "{                             \
			gsub(/\\t/, \"        \"); \
			if (length(\$$0) > $@)     \
				printf(\"$$F:%d: %s\\n\", NR, \$$0); \
		}" < $$F;                          \
	done

wc:
	@wc -l $(SRCALL)

.PHONY: all clean clean-gen check check-arm64 check-rv64 check-amd64_win \
        src 80 wc install uninstall