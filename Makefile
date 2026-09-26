INC = -Iinclude
LIB = -lpthread

SRC = src
OBJ = obj
INCLUDE = include

CC = gcc
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG) -DMM_PAGING
LFLAGS = -Wall $(DEBUG)

vpath %.c $(SRC)
vpath %.h $(INCLUDE)

MAKE = $(CC) $(INC) 

# Object files needed by modules
MEM_OBJ = $(addprefix $(OBJ)/, paging.o mem.o cpu.o loader.o)
SYSCALL_OBJ = $(addprefix $(OBJ)/, syscall.o sys_mem.o sys_listsyscall.o sys_kmem.o)
OS_OBJ = $(addprefix $(OBJ)/, cpu.o loader.o queue.o os.o sched.o timer.o mm-vm.o mm64.o mm.o mm-memphy.o libstd.o libmem.o)
OS_OBJ += $(SYSCALL_OBJ)
SCHED_OBJ = $(addprefix $(OBJ)/, cpu.o loader.o)
HEADER = $(wildcard $(INCLUDE)/*.h)
 
all: os

# Compile syscall
syscalltbl.lst: $(SRC)/syscall.tbl
	@echo $(OS_OBJ)
	chmod +x $(SRC)/syscalltbl.sh
	$(SRC)/syscalltbl.sh $< $(SRC)/$@ 

# Compile the whole OS simulation
os: $(OBJ) syscalltbl.lst $(OS_OBJ)
	$(MAKE) $(LFLAGS) $(OS_OBJ) -o os $(LIB)

$(OBJ)/%.o: %.c ${HEADER} $(OBJ)
	$(MAKE) $(CFLAGS) $< -o $@

$(OBJ):
	mkdir -p $(OBJ)

clean:
	rm -f $(SRC)/*.lst
	rm -f $(OBJ)/*.o os sched mem pdg
	rm -rf $(OBJ)
