#
# Makefile for UNIX - unrar
#
# Note: you have to 'make clean' before you can build
#	the sfx module
#

# Linux using GCC
CXX=g++
CXXFLAGS=-O2
DEFINES=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

# Linux using LCC
#CXX=lcc
#CXXFLAGS=-O2
#DEFINES=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

# IRIX using GCC
#CXX=g++
#CXXFLAGS=-O2 
#DEFINES=-DBIG_ENDIAN -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_BSD_COMPAT -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1

# IRIX using MIPSPro (experimental)
#CXX=CC
#CXXFLAGS=-O2 -mips3 -woff 1234,1156,3284 -LANG:std
#DEFINES=-DBIG_ENDIAN -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_BSD_COMPAT -DNATIVE_INT64 -DInt64=int64_t

# AIX using xlC (IBM VisualAge C++ 5.0)
#CXX=xlC
#CXXFLAGS=-O -qinline -qro -qroconst -qmaxmem=16384 -qcpluscmt
#DEFINES=-D_LARGE_FILES -D_LARGE_FILE_API
#LIBS=-lbsd

# Solaris using CC (SUN Forte Developer 7 C++)
#CXX=CC
#CXXFLAGS=-xO2 -xbuiltin=%all -xinline=%auto
#DEFINES=-DBIG_ENDIAN -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

# Tru64 5.1B using GCC3
#CXX=g++
#CXXFLAGS=-O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_XOPEN_SOURCE=500
#LDFLAGS=-rpath /usr/local/gcc/lib

##########################

COMPILE=$(CXX) $(CXXFLAGS) $(DEFINES)
LINK=$(CXX)

UNRAR_OBJ=filestr.o recvol.o rs.o scantree.o

OBJECTS=rar.o strlist.o strfn.o pathfn.o int64.o savepos.o global.o file.o filefn.o filcreat.o \
	archive.o arcread.o unicode.o system.o isnt.o crypt.o crc.o rawread.o encname.o \
	resource.o match.o timefn.o rdwrfn.o consio.o options.o ulinks.o errhnd.o rarvm.o \
	rijndael.o getbits.o sha1.o extinfo.o extract.o volume.o list.o find.o unpack.o cmddata.o

.cpp.o:
	$(COMPILE) -D$(WHAT) -c $<

all:	unrar

clean:
	@rm -f *.o *.bak *~

unrar:	WHAT=UNRAR
unrar:	$(OBJECTS) $(UNRAR_OBJ)
	@rm -f makeunrar
	$(LINK) -o unrar $(LDFLAGS) $(OBJECTS) $(UNRAR_OBJ) $(LIBS)	
	@strip unrar

sfx:	WHAT=SFX_MODULE
sfx:	$(OBJECTS)
	@rm -f default.sfx
	$(LINK) -o default.sfx $(LDFLAGS) $(OBJECTS) -DSFX_MODULE 
	strip default.sfx

