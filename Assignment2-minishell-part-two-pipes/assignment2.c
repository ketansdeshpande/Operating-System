#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

//limits
#define MAX_TOKENS 100
#define MAX_STRING_LEN 100

size_t MAX_LINE_LEN = 10000;

// builtin commands
#define EXIT_STR "exit"
#define EXIT_CMD 0
#define DEFAULT_CMD 99


FILE *fp, *fp2, *fp3;
bool bgProcess;
char **commands, *line, *full_command[100], path[100], *file_name, str[100], *out_token;
bool in_command = false, out_command = false, pipe_command = false;
int counter = 0, running_process_id[100], no_of_pipes = 0, no_of_commands = 0;
pid_t process_id;

void handle_sigint()
{
	printf("\nCtrl+C pressed\n");
}

void initialize()
{
	// allocate space for the whole command
	assert( ( line = malloc( sizeof( char ) * MAX_STRING_LEN ) ) != NULL );

	// allocate space for individual commands
	assert( ( commands = malloc( sizeof( char* )*MAX_TOKENS ) ) != NULL );

	// open stdin as a file pointer 
	assert( ( fp = fdopen( STDIN_FILENO, "r" ) ) != NULL );

}

char* trim(char *str)
{
	// remove whitespaces and append null character at end
	while(isspace(*str)) str++;
	str[strlen(str)-1] = '\0';
	return str;
}

void tokenize( char * string )
{
	int token_count = 0, size = MAX_TOKENS;
	char *this_token, *pipe_token;
	in_command = false;
	out_command = false;
	pipe_command = false;
	no_of_pipes = 0;
	no_of_commands = 0;

	// Check if it is a background process
	bgProcess = ( string[ strlen( string ) - 2 ] == '&' ) ? true : false;

	// Check if it's filter command
	if( strchr(string, '|') != NULL ) {
		pipe_command = true;
		char *first[100];
		while( ( pipe_token = strsep( &string, "|" ) ) != NULL ) {
			commands[ token_count ] = trim(pipe_token);
			*first = commands[ token_count ];
			token_count++;
			no_of_pipes++;
		}
	} else {
		// Separate out each command argument
		while( ( this_token = strsep( &string, " \t\v\f\n\r'&'" ) ) != NULL ) {
			if( *this_token == '\0' ) continue;
			if( strcmp( this_token, "<" ) == 0 ) in_command = true;
			if( strcmp( this_token, ">" ) == 0 ) out_command = true;
			if( in_command || out_command ) file_name = this_token;

			commands[ token_count++ ] = this_token;

			// if there are more commands than space ,reallocate more space
			if( token_count >= size ) {
				size*=2;
				assert ( ( commands = realloc( commands, sizeof( char* ) * size ) ) != NULL );
			}

			no_of_commands++;
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
	char *this_token;
	int execvp_status, i;
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
			} else if( kill_status == -1 ) {
				printf("Process %d is completed\n", running_process_id[for_counter]);
			}
		}

		return DEFAULT_CMD;
	} else if(strcmp( commands[0], "fg" ) == 0) {
		if( commands[1] == NULL ) {
			perror("Command not found!\n");
			return DEFAULT_CMD;
		}

		// As we are accepting process id to bring it to the foreground, we should make sure, it is indeed an int type
		int fg_pid = atoi( commands[1] );
		// hold the process mentioned by user
		pid_t wait_pid = waitpid( fg_pid, NULL, 0 );
		// Send signal for that process_id to continue execution
		kill( wait_pid, SIGCONT );
		return DEFAULT_CMD;
	} else if (pipe_command) {
		int pipefds[2*no_of_pipes], i, j = 0, k, status, total_command_length = MAX_TOKENS;
		pid_t pid;
		char **command_name;
		assert( ( command_name = malloc( sizeof( char* )*MAX_TOKENS ) ) != NULL );

		for(i = 0; i < (no_of_pipes); i++) {
			if(pipe(pipefds + i*2) < 0) {
				perror("Couldn't pipe: ");
				exit(EXIT_FAILURE);
			}
		}

		for(i=0; i<no_of_pipes; i++) {
			k=0;
			while( ( this_token = strsep( &commands[i], " \t\v\f\n\r'&'" ) ) != NULL ) {
				command_name[k] = this_token;
				k++;
				if( k >= total_command_length ) {
					total_command_length*=2;
					assert ( ( command_name = realloc( command_name, sizeof( char* ) * total_command_length ) ) != NULL );
				}
			}

			// fork child process for each command in filter
			pid = fork();
			if(pid == 0) {
				if(commands[i+1]) {
					if(dup2(pipefds[j + 1], 1) < 0) {
						perror("Error in dup2: ");
						exit(EXIT_FAILURE);
					}
				}

				if(j != 0 ) {
					if(dup2(pipefds[j-2], 0) < 0) {
						perror("Error in dup2: ");
						exit(EXIT_FAILURE);
					}
				}

				for(i = 0; i < 2*no_of_pipes; i++) {
					close(pipefds[i]);
				}

				if( execvp(command_name[0], command_name) < 0 ) {
					perror(command_name[0]);
					exit(EXIT_FAILURE);
				}
			} else if(pid < 0) {
				perror("Error: ");
				exit(EXIT_FAILURE);
			}

			j+=2;
		}

		for(i = 0; i < 2 * no_of_pipes; i++) {
			close(pipefds[i]);
		}

		for(i = 0; i < no_of_pipes + 1; i++)
			wait(&status);
	} else if (in_command) {
		int i, k = 0, total_command_length = MAX_TOKENS;
		char **command_name;
		assert( ( command_name = malloc( sizeof( char* )*MAX_TOKENS ) ) != NULL );
		for(i=0; i<no_of_commands; i++) {
			if(strcmp( commands[i], "<" ) != 0 ) {
				command_name[k] = commands[i];
				k++;
				if( k >= total_command_length ) {
					total_command_length*=2;
					assert ( ( command_name = realloc( command_name, sizeof( char* ) * total_command_length ) ) != NULL );
				}
			}
		}

		process_id = fork();

		if( process_id==0 ) {
			int fd = open(file_name, O_RDONLY);
			if (fd < 0)
			{
				fputs("could not open myfile!\n", stderr);
				perror("open");
				exit(EXIT_FAILURE);
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
			
			if( execvp(command_name[0], command_name) < 0 ) {
				perror(command_name[0]);
				exit(EXIT_FAILURE);
			}
			exit(0);
		} else if( process_id > 0 ) {
			signal(SIGINT, handle_sigint);
			wait(NULL);
		} else if( process_id < 0 ) {
			printf("Fork error!\n");
		}
	} else if (out_command) {
		int a = 0, b;
		char *cmd;
		cmd[0] = '\0';
		while(strcmp(commands[a], ">") != 0) {
			strcat(cmd, commands[a]);
			strcat(cmd, " ");
			a++;
		}
		cmd[strlen(cmd)-1] = '\0';
		fp2 = popen(cmd, "r");
		if (fp2 == NULL) {
			printf("Failed to run command\n" );
			exit(1);
		}

		fp3 = fopen (file_name, "w");
		while (fgets(path, sizeof(path)-1, fp2) != NULL) {
			fprintf (fp3, "%s", path);
		}

		pclose(fp2);
		pclose(fp3);
		printf("File created with file name: %s\n", file_name);
	} else {
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
				// signal( SIGCHLD, SIG_DFL ); assignment 1, signaling default
				signal(SIGINT, handle_sigint);

				running_process_id[counter] = process_id;
				counter++;
				wait(NULL);
			} else if( process_id < 0 ) {
				// If fork couldn't create child process
				printf("Fork error!\n");
			}
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
		printf("\nsh550> ");
		read_command();
	} while( run_command() != EXIT_CMD );

	return 0;
}