BIG_INT_DIR 	= libbig_int
INCS		= -I$(BIG_INT_DIR)/include
CC		= gcc $(INCS)
LD		= ld
EXE		= omniconvert.exe
COPTS		= -O2 -Wall -s
LDFLAGS		= -mwindows
# BIG_INT library
BIG_INT_OBJS = \
	$(BIG_INT_DIR)/src/basic_funcs.o \
	$(BIG_INT_DIR)/src/bitset_funcs.o \
	$(BIG_INT_DIR)/src/memory_manager.o \
	$(BIG_INT_DIR)/src/modular_arithmetic.o \
	$(BIG_INT_DIR)/src/number_theory.o \
	$(BIG_INT_DIR)/src/service_funcs.o \
	$(BIG_INT_DIR)/src/str_funcs.o \
	$(BIG_INT_DIR)/src/low_level_funcs/add.o \
	$(BIG_INT_DIR)/src/low_level_funcs/and.o \
	$(BIG_INT_DIR)/src/low_level_funcs/andnot.o \
	$(BIG_INT_DIR)/src/low_level_funcs/cmp.o \
	$(BIG_INT_DIR)/src/low_level_funcs/div.o \
	$(BIG_INT_DIR)/src/low_level_funcs/mul.o \
	$(BIG_INT_DIR)/src/low_level_funcs/or.o \
	$(BIG_INT_DIR)/src/low_level_funcs/sqr.o \
	$(BIG_INT_DIR)/src/low_level_funcs/sub.o \
	$(BIG_INT_DIR)/src/low_level_funcs/xor.o

OBJS	= $(BIG_INT_OBJS) common.o cheat.o arcfour.o cb2_crypto.o gs3.o ar2.o armax.o translate.o crc32.o armlist.o cbc.o p2m.o scf.o omniconvert.o resource.o

all:	$(OBJS)
	$(CC) $(COPTS) -o $(EXE) $(OBJS) $(LDFLAGS)

clean:
	rm -f *.o *.exe *~

rebuild:	clean all

resource.o:	resource.rc
		windres -o resource.o resource.rc