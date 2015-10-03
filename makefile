CC		= gcc


# CFLAGS		= -Wall -pedantic -std=c11 -m64 -g
# CFLAGS		= -m64 -g -Wall -pedantic -Werror -std=c99
# CFLAGS		= -O3 -maltivec -Wall -pedantic -std=c99
# CFLAGS		= -O3 -Wall -pedantic -std=c99
CFLAGS      = -O3 -Wall -pedantic -std=c11 -m64 -pthread -D_GNU_SOURCE
# CFLAGS      = -g -O3 -Wall -pedantic -std=c11 -m64 -pthread -D_GNU_SOURCE

OBJS		= main.o list.o error.o random.o set.o dataflow.o

OUT		= live

LDFLAGS		= -lpthread

# S=100000
# V=100000
# U=4
# A=1000
# T=4
# P=0

# The ultimate test
# S=100000 # the range of symbol values [0, n]
# V=100000 # vertices in the graph
# U=4 		 # successors
# A=1000   # nactive (different values of entries in the sets)
# T=4      # threads
# P=0      # print

# For debugging
S=100000 # the range of symbol values [0, n]
V=100000 # vertices in the graph
U=4   # successors for each vertex
A=100  # max number of active (live) variables for a set
T=4   # threads
P=0   # print

all: $(OBJS)	
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(OUT)

run:
	./$(OUT) $(S) $(V) $(U) $(A) $(T) $(P)
	
valgrind:
	valgrind --read-var-info=yes --tool=helgrind ./$(OUT) $(S) $(V) $(U) $(A) $(T) $(P) $(L)

clean:
	rm -f $(OUT) $(OBJS) cfg.dot
