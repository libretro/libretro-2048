!IF "$(PLATFORM)"=="X64" || "$(PLATFORM)"=="x64"
ARCH=amd64
!ELSE
ARCH=x86
!ENDIF

CC=cl
RD=rd/s/q
RM=del/q
LINKER=link
TARGET=2048_libretro.dll

OBJS=\
	  libretro.obj \
	  game_shared.obj \
	  game_noncairo.obj

$(TARGET): $(OBJS)
	$(LINKER) $(LFLAGS) $(LIBS) /OUT:$a $**
