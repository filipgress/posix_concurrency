#include <mpi.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * MPI Application for Electing a Ruler Using a Coin Toss Protocol
 *
 * This MPI application is designed to elect a single ruler among n-processes
 * using a multi-round coin toss protocol. Each process participates in a coin
 * toss, and processes with a coin toss result of 'heads' (1) continue to the
 * next round while those with 'tails' (0) are eliminated if any heads are present.
 * This continues until only one process remains active. Once a ruler is determined,
 * all processes print a message indicating their recognition of the ruler.
 */

int main(int argc, char* argv[]) {
	MPI_Init(&argc, &argv);

	int npes, rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &npes);

	srand(time(NULL) + rank);

	int ruler = -1;
	int active = 1;

	while(1) {
		int coin = active ? rand() % 2 : 0;

		int ones;
		MPI_Allreduce(&coin, &ones, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

		if (ones > 0 && !coin)
			active = 0;

		int active_count;
		MPI_Allreduce(&active, &active_count, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

		if (active_count == 1) {
			if (active)
				ruler = rank;
			break;
		}
	}

	MPI_Allreduce(&ruler, &ruler, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
	printf("rank(%d): %d\n", rank, ruler);

	MPI_Finalize();
	return 0;
}
