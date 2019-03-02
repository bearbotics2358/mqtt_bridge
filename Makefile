
##
## 
##

INCLUDES=
# INCLUDES=-Iinclude/

CROSS=$(TARGET)-
CC = cc
AR = ar
LD = ld
STRIP = strip

DEBUG_OPTION =
OPT = -g
LIBS=-lmosquitto 
CFLAGS += -Wall $(OPT) 

OBJECTS = 	mqtt_bridge.o \
	inetlib.o \
	putsock.o

APP = mqtt_bridge

all: $(APP)

# disp_inodes: inode.o superblock.o grouptable.o disp_inodes.o $(LIBS) 
# 	$(CC)  $(CFLAGS) inode.o superblock.o grouptable.o disp_inodes.o $(LIBS) -o  $@ 
# 	@echo "make $@ finished on `date`"

mqtt_bridge: $(OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(APP)
