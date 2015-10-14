CC		= gcc


# CFLAGS		= -Wall -pedantic -std=c11 -m64 -g
# CFLAGS		= -m64 -g -Wall -pedantic -Werror -std=c99
# CFLAGS		= -O3 -maltivec -Wall -pedantic -std=c99
# CFLAGS		= -O3 -Wall -pedantic -std=c99
# CFLAGS      = -O3 -Wall -pedantic -std=c11 -m64 -pthread -D_GNU_SOURCE
CFLAGS      = -O3 -maltivec -Wall -pedantic -std=c11 -m64 -pthread -D_GNU_SOURCE

OBJS		= main.o list.o error.o random.o set.o dataflow.o

OUT		= live

LDFLAGS		= -lpthread

# For debugging
#S=100 # the range of symbol values [0, n]
#V=100 # vertices in the graph
#U=4   # successors for each vertex
#A=10  # max number of active (live) variables for a set
#T=4   # threads
#P=0   # print

# For debugging
S2=10000 # the range of symbol values [0, n]
V2=10000 # vertices in the graph
U2=4     # successors for each vertex
A2=1000  # max number of active (live) variables for a set
T2=4     # threads
P2=0     # print

# The ultimate test
S3=100000 # the range of symbol values [0, n]
V3=100000 # vertices in the graph
U3=4 		  # successors
A3=1000   # nactive (different values of entries in the sets)
T3=4      # threads
P3=0      # print

all: $(OBJS)	
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(OUT)


test:
	./$(OUT) $(S) $(V) $(U) $(A) $(T) $(P)

run:
	./$(OUT) $(S2) $(V2) $(U2) $(A2) $(T2) $(P2)

full:
	./$(OUT) $(S3) $(V3) $(U3) $(A3) $(T3) $(P3)
	
valgrind:
	valgrind --read-var-info=yes --tool=helgrind ./$(OUT) $(S) $(V) $(U) $(A) $(T) $(P) $(L)

clean:
	rm -f $(OUT) $(OBJS) cfg.dot
