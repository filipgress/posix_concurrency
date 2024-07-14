#define _POSIX_C_SOURCE 200809L

/*
 * Count the total number of bytes read from multiple
 * file descriptors concurrently
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <poll.h>

struct Handler {
	pid_t pid;
	int pipe_out;
};

int handle_child(int pipe, int fd_count, const int* fds) {
	int rv = -1;

	struct pollfd poll_fds[fd_count];
	for (int i = 0; i < fd_count; i++) {
		poll_fds[i].fd = fds[i];
		poll_fds[i].events = POLLIN;
	}

	int opened = fd_count;
	int total = 0;

	while (opened) {
		if (poll(poll_fds, fd_count, -1) == -1)
			goto end;

		for (int i = 0; i < fd_count; i++) {
			if ((poll_fds[i].revents & (POLLIN | POLLHUP)) == 0)
				continue;

			char buf[255];
			int bytes = read(poll_fds[i].fd, buf, 255);

			if (bytes == -1)
				goto end;
			
			if (bytes == 0) {
				poll_fds[i].fd = -1;
				opened--;
			} 

			total += bytes;
		}
	}

	if (write(pipe, &total, sizeof(total)) == -1)
		goto end;

	rv = 0;
end:
	close(pipe);
	return rv;
}

void *mcount_start( int fd_count, const int* fds ) {
	struct Handler *handle = malloc(sizeof(struct Handler));
	int pipe_fds[2] = {-1, -1};

	if (!handle || pipe(pipe_fds) == -1)
		goto end;

	handle->pipe_out = pipe_fds[0];
	handle->pid = fork();

	if (handle->pid == -1)
		goto end;

	if (handle->pid == 0) {
		free(handle);
		close(pipe_fds[0]);

		exit(handle_child(pipe_fds[1], fd_count, fds));
	}

	close(pipe_fds[1]);
	return handle;

end:
	free(handle);
	close(pipe_fds[0]);
	close(pipe_fds[1]);

	return NULL;
}

int mcount_cleanup( void *handle ) {
	int rv = -1, status;
	struct Handler *handle_ptr = handle;

	if (waitpid(handle_ptr->pid, &status, 0) == -1 ||
			!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		goto end;

	int size;
	if (read(handle_ptr->pipe_out, &size, sizeof(size)) == -1)
		goto end;

	rv = size;

end:
	close(handle_ptr->pipe_out);
	free(handle);

	return rv;
}

// tests

#include <err.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static void close_or_warn( int fd, const char *name )
{
    if ( close( fd ) == -1 )
        warn( "closing %s", name );
}

static void mk_pipe( int* fd_r, int* fd_w )
{
    int p[ 2 ];
    if ( pipe( p ) == -1 )
        err( 2, "pipe" );
    *fd_r = p[ 0 ];
    *fd_w = p[ 1 ];
}

static void mk_pipes( int n, int* fds_r, int* fds_w )
{
    for ( int i = 0; i < n; ++i )
        mk_pipe( fds_r + i, fds_w + i );
}

static void close_fds( int n, const int* fds, const char* desc )
{
    for ( int i = 0; i < n; ++i )
        close_or_warn( fds[ i ], desc );
}

static void fill( int fd, ssize_t bytes )
{
    char buf[ 512 ];
    memset( buf, 'x', 512 );

    ssize_t nwrote;
    while ( ( nwrote = write( fd, buf,
                              bytes > 512 ? 512 : bytes ) ) > 0 )
        bytes -= nwrote;

    assert( nwrote != -1 );
    assert( bytes == 0 );
}

static int fork_solution( int n, int* fds_r, int* fds_w,
                          int fd_count, int expected_res )
{
    int sync[ 2 ];

    if ( pipe( sync ) == -1 )
        err( 2, "sync pipe" );

    alarm( 5 ); /* if we get stuck */

    pid_t pid = fork();
    if ( pid == -1 )
        err( 2, "fork" );

    if ( pid == 0 )   /* child -> verifies solution */
    {
        close_or_warn( sync[ 0 ], "sync pipe: read end (fork_solution)" );
        close_fds( n, fds_w, "input pipes: write ends (fork_solution)" );

        void* handle = mcount_start( fd_count, fds_r );
        assert( handle != NULL );

        close_fds( n, fds_r, "input pipes: read ends (fork_solution)" );
        if ( write( sync[ 1 ], "a", 1 ) == -1 )
            err( 2, "sync write" );
        close_or_warn( sync[ 1 ], "sync pipe: write end (fork_solution)" );

        int ntot = mcount_cleanup( handle );
        if ( ntot != expected_res )
            errx( 1, "expected_res = %d, ntot = %d", expected_res, ntot );
        exit( 0 );
    }

    /* parent -> sends data to the solution */

    close_or_warn( sync[ 1 ], "sync pipe: write end (tests)" );
    char c;
    assert( read( sync[ 0 ], &c, 1 ) != -1 ); /* blocks until counter_start is called */
    close_or_warn( sync[ 0 ], "sync pipe: read end (tests)" );

    return pid;
}

static int reap_solution( pid_t pid )
{
    int status;
    if ( waitpid( pid, &status, 0 ) == -1 )
        err( 2, "waitpid" );
    return WIFEXITED( status ) && WEXITSTATUS( status ) == 0;
}

int main( void )
{
    int fds_r[ 3 ], fds_w[ 3 ];

    mk_pipes( 3, fds_r, fds_w );
    pid_t pid = fork_solution( 3, fds_r, fds_w, 3, 15 );
    close_fds( 3, fds_r, "input pipes: read ends (tests)" );
    fill( fds_w[ 0 ], 5 );
    fill( fds_w[ 1 ], 5 );
    fill( fds_w[ 2 ], 5 );
    close_fds( 3, fds_w, "input pipes: write ends (tests)" );
    assert( reap_solution( pid ) );

    mk_pipes( 3, fds_r, fds_w );
    pid = fork_solution( 3, fds_r, fds_w, 3, 10000 );
    close_fds( 3, fds_r, "input pipes: read ends (tests)" );
    fill( fds_w[ 0 ], 3333 );
    fill( fds_w[ 1 ], 6666 );
    fill( fds_w[ 2 ], 1 );
    close_fds( 3, fds_w, "input pipes: write ends (tests)" );
    assert( reap_solution( pid ) );

    mk_pipes( 3, fds_r, fds_w );
    pid = fork_solution( 3, fds_r, fds_w, 3, 20 );
    close_fds( 3, fds_r, "input pipes: read ends (tests)" );
    fill( fds_w[ 0 ], 19 );
    fill( fds_w[ 1 ], 1 );
    close_fds( 3, fds_w, "input pipes: write ends (tests)" );
    assert( reap_solution( pid ) );

    return 0;
}
