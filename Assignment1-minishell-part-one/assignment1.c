#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>

//limits
#define MAX_TOKENS 100
#define MAX_STRING_LEN 100

size_t MAX_LINE_LEN = 10000;

// builtin commands
#define EXIT_STR "exit"
#define EXIT_CMD 0
#define DEFAULT_CMD 99


FILE *fp; // file struct for stdin
bool bgProcess;
char **commands, *line;
int counter = 0, running_process_id[100];
pid_t process_id;

void initialize()
{
	// allocate space for the whole command
	assert( ( line = malloc( sizeof( char ) * MAX_STRING_LEN ) ) != NULL );

	// allocate space for individual commands
	assert( ( commands = malloc( sizeof( char* )*MAX_TOKENS ) ) != NULL );

	// open stdin as a file pointer 
	assert( ( fp = fdopen( STDIN_FILENO, "r" ) ) != NULL );

}

void tokenize( char * string )
{
	int token_count = 0, size = MAX_TOKENS;
	char *this_token;

	// Check if it is a background process
	bgProcess = ( string[ strlen( string ) - 2 ] == '&' ) ? true : false;

	// Separate out each command argument
	while( ( this_token = strsep( &string, " \t\v\f\n\r'&'" ) ) != NULL ) {
		if( *this_token == '\0' ) continue;

		commands[ token_count ] = this_token;
		token_count++;

		// if there are more commands than space ,reallocate more space
		if( token_count >= size ) {
			size*=2;
			assert ( ( commands = realloc( commands, sizeof( char* ) * size ) ) != NULL );
		}
	}

	// As we have stripped out last character of a string, re-set it at the end of the string
	if( token_count > 0 ) commands[ token_count ] = NULL;
}

void read_command() 
{
	assert( getline( &line, &MAX_LINE_LEN, fp ) > -1 );
	tokenize( line );
}

int run_command() {
	int execvp_status;
	if ( strcmp( commands[0], EXIT_STR ) == 0 ) {
		// If exit, then terminate
		kill( 0, SIGTERM );
		return EXIT_CMD;
	} else if( strcmp( commands[0], "listjobs" ) == 0 ) {
		int for_counter, kill_status;

		for( for_counter=0; for_counter<=counter; for_counter++ ) {
			// Send signal to all processes by using kill
			kill_status = kill( running_process_id[for_counter], 0 );
			if( kill_status == 0 ) {
				printf("Process %d is running\n", running_process_id[for_counter]);
			} else if(kill_status == -1){
				printf("Process %d is completed\n", running_process_id[for_counter]);
			}
		}

		return DEFAULT_CMD;
	} else if(strcmp( commands[0], "fg" ) == 0) {
		if( commands[1] == NULL ) {
		printf("Command not found!\n");
			return DEFAULT_CMD;
		}

		// As we are accepting process id to bring it to the foreground, we should make sure, it is indeed an int type
		int fg_pid = atoi( commands[1] );
		// hold the process mentioned by user
		pid_t wait_pid = waitpid( fg_pid, NULL, 0 );
		// Send signal for that process_id to continue execution
		kill( wait_pid, SIGCONT );
		return DEFAULT_CMD;
	}

	if( true == bgProcess ) {
		// Create new child process
		process_id = fork();
		
		if( process_id==0 ) {
			// If its a parent process
			execvp_status = execvp( commands[0], commands );
			if( execvp_status < 0 ) {
				printf("Command not found!\n");
			}
			exit(0);
		} else if( process_id > 0 ) {
			// If its a child process
			signal( SIGCHLD, SIG_IGN );

			printf("Process %d is running in background\n", process_id);
			running_process_id[counter] = process_id;			
			counter++;
			return DEFAULT_CMD;
		} else if( process_id < 0 ) {
			// If fork couldn't create child process
			printf("Fork error!\n");
		}
		return DEFAULT_CMD;
	} else {
		// Create new child process
	 	process_id = fork();

		if( process_id==0 ) {
			// If its a parent process
			execvp_status = execvp( commands[0], commands );
			if( execvp_status < 0 ) {
				printf("Command not found!\n");
			}
			exit(0);
		} else if( process_id > 0 ) {
			// If its a child process
			signal( SIGCHLD, SIG_DFL );

			running_process_id[counter] = process_id;
			counter++;
			wait(NULL);
		} else if( process_id < 0 ) {
			// If fork couldn't create child process
			printf("Fork error!\n");
		}
	}

	return DEFAULT_CMD;
}

// Take care of any zombie process
void killzombie() {
	waitpid( process_id, NULL, WNOHANG );
}

int main()
{
	signal( SIGCHLD, killzombie );
	initialize();
	do {
		printf("sh550> ");
		read_command();
	} while( run_command() != EXIT_CMD );

	return 0;
}