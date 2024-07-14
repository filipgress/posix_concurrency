#define _POSIX_C_SOURCE 200809L
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

/* 
 * Function to handle the execution of a child process.
 * It redirects the standard input and output to the given file descriptors
 * and executes the specified command with its arguments.
 */
int handle_child(const char *cmd, char* const argv[],
								 int fd_in, int fd_out) {
	dup2(fd_in, STDIN_FILENO);
	dup2(fd_out, STDOUT_FILENO);

	close(fd_in);
	close(fd_out);

	return execvp(cmd, argv);
}

/* 
 * Function to pipe the output of one command to the input of another.
 * It sets up a pipeline between two commands, with input and output
 * redirection to specified files. It also retrieves the exit statuses
 * of both commands.
 */
int pipe_files( const char *cmd_1, char * const argv_1[],
                const char *cmd_2, char * const argv_2[],
                const char *file_1, const char *file_2,
                int *status_1, int *status_2 ) {
	int fds[2] = {-1, -1};
	pid_t pid_1, pid_2;

	if (pipe(fds) == -1 || (pid_1 = fork()) == -1)
		return -1;

	if (pid_1 == 0) {
		close(fds[0]);

		int fd_in = open(file_1, O_RDONLY);
		if (fd_in == -1)
			exit(-1);
		exit(handle_child(cmd_1, argv_1, fd_in, fds[1]));
	}

	close(fds[1]);

	if (waitpid(pid_1, status_1, 0) == -1 || 
			WEXITSTATUS(*status_1) > 127)
		return -1;

	if ((pid_2 = fork()) == -1)
		return -1;

	if (pid_2 == 0) {
		int fd_out = open(file_2, O_CREAT | O_RDWR, 0777);
		exit(handle_child(cmd_2, argv_2, fds[0], fd_out));
	}

	close(fds[0]);

	if (waitpid(pid_2, status_2, 0) == -1 ||
			WEXITSTATUS(*status_2) > 127)
		return -1;

	return 0;
}

// tests

#include <err.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>

static void unlink_if_exists( const char *file )
{
    if ( unlink( file ) == -1 && errno != ENOENT )
        err( 2, "unlink" );
}

static void mk_file( const char* file, const char* content )
{
    int fd = open( file, O_WRONLY | O_CREAT, 0777 );
    if ( fd == 0 )
        err( 2, "open" );
    if ( write( fd, content, strlen( content ) ) == -1 )
        err( 2, "write" );
    close( fd );
}

static int filecmp( const char* file, const char* expected )
{
    int fd = open( file, O_RDONLY );
    assert( fd != -1 );

    off_t sz = lseek( fd, 0, SEEK_END );

    if ( sz == -1 || lseek( fd, 0, SEEK_SET ) == -1 )
        err( 2, "lseek" );

    char buf[ sz ];
    memset( buf, 0, sz );

    assert( read( fd, buf, sz ) != -1 );
    close( fd );

    return sz == ( int ) strlen( expected ) &&
           memcmp( buf, expected, strlen( expected ) ) == 0 ? 0 : -1;
}

int main( void )
{
    int status1, status2;
    char * const argv_wc[]  = { "wc", "-l", NULL };
    char * const argv_cat[] = { "cat", NULL };
    char * const argv_sed[] = { "sed", "-e", "s, ,,g", NULL };
    char * const argv_sed_file[] = { "sed", "-e", "s, ,,g",
                                     "zt.p4_pipe_in", NULL };
    char * const argv_empty[] = { NULL };
    char * const argv_false[] = { "false", NULL };

    unlink_if_exists( "zt.p4_pipe_in" );
    unlink_if_exists( "zt.p4_pipe_out" );
    mk_file( "zt.p4_pipe_in", "test\n" );

    assert( pipe_files( "wc", argv_wc, "sed", argv_sed,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == 0 );
    assert( WIFEXITED( status1 ) && WEXITSTATUS( status1 ) == 0 );
    assert( WIFEXITED( status2 ) && WEXITSTATUS( status2 ) == 0 );
    assert( filecmp( "zt.p4_pipe_out", "1\n" ) == 0 );

    unlink_if_exists( "zt.p4_pipe_in" );
    unlink_if_exists( "zt.p4_pipe_out" );
    mk_file( "zt.p4_pipe_in", "hello\nworld\nthree\n" );

    assert( pipe_files( "wc", argv_wc, "sed", argv_sed,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == 0 );
    assert( WIFEXITED( status1 ) && WEXITSTATUS( status1 ) == 0 );
    assert( WIFEXITED( status2 ) && WEXITSTATUS( status2 ) == 0 );
    assert( filecmp( "zt.p4_pipe_out", "3\n" ) == 0 );

    /* no unlinking */

    mk_file( "zt.p4_pipe_in", "hello\nworld\nthree\n" );
    assert( pipe_files( "wc", argv_wc, "sed", argv_sed,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == 0 );
    assert( WIFEXITED( status1 ) && WEXITSTATUS( status1 ) == 0 );
    assert( WIFEXITED( status2 ) && WEXITSTATUS( status2 ) == 0 );
    assert( filecmp( "zt.p4_pipe_out", "3\n" ) == 0 );

    /* first command does not exits, exec* fails */
    assert( pipe_files( "hello", argv_empty, "sed", argv_sed,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == -1 );

    /* same, but the second one */
    assert( pipe_files( "wc", argv_wc, "world", argv_empty,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == -1 );

    /* input file does not exist */
    unlink_if_exists( "zt.p4_pipe_in" );
    assert( pipe_files( "wc", argv_wc, "sed", argv_sed,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == -1 );

    /* first command runs, but does not succeed */
    /* it may print error message to stderr, that's okay */
    mk_file( "zt.p4_pipe_in", "mom?\n" );
    assert( pipe_files( "false", argv_false, "sed", argv_sed_file,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == 0 );

    assert( WIFEXITED( status1 ) && WEXITSTATUS( status1 ) != 0 );
    assert( WIFEXITED( status2 ) && WEXITSTATUS( status2 ) == 0 );

    /* same, but the second one */
    assert( pipe_files( "wc", argv_wc, "false", argv_false,
                       "zt.p4_pipe_in", "zt.p4_pipe_out",
                       &status1, &status2 ) == 0 );
    assert( WIFEXITED( status1 ) && WEXITSTATUS( status1 ) == 0 );
    assert( WIFEXITED( status2 ) && WEXITSTATUS( status2 ) != 0 );

    char buffer[ PIPE_BUF * 2 ];
    memset( buffer, 'x', sizeof buffer );
    buffer[ sizeof( buffer ) - 1 ] = 0;

    mk_file( "zt.p4_pipe_in", buffer );

    assert( pipe_files( "cat", argv_cat, "cat", argv_cat,
                        "zt.p4_pipe_in", "zt.p4_pipe_out",
                        &status1, &status2 ) == 0 );
    assert( WIFEXITED( status1 ) && WEXITSTATUS( status1 ) == 0 );
    assert( WIFEXITED( status2 ) && WEXITSTATUS( status2 ) == 0 );
    assert( filecmp( "zt.p4_pipe_out", buffer ) == 0 );

    return 0;
}
