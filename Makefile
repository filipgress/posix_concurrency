CC = gcc
MPICC = mpicc

CFLAGS = -g -Wall -Wextra -Werror -pedantic

SRC = lock_free_queue.c mcount_fork.c pipe.c
MPI_SRC = MPI_coin_toss_protocol.c

OBJ = $(SRC:.c=.o)
MPI_OBJ = $(MPI_SRC:.c=.o)

EXEC = $(SRC:.c=)
MPI_EXEC = $(MPI_SRC:.c=)

all: $(EXEC) $(MPI_EXEC)

$(EXEC): % : %.o 
	$(CC) $(CFLAGS) -o $@ $<

$(MPI_EXEC): % : %.o
	$(MPICC) $(CFLAGS) -o $@ $< # && mpirun -np 6 ./$@

%.o: %.c
	$(CC) -c $< -o $@

$(MPI_OBJ): %.o : %.c
	$(MPICC) -c $< -o $@

clean:
	rm -rf $(EXEC) $(MPI_EXEC) *.o
