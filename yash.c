#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>

// boolean struct
typedef int bool;
#define true 1
#define false 0

// enum to keep track of what type of indirect was detected
enum file_flag {inputf, outputf, errorf};

typedef struct jobs jobs;

struct jobs{
	pid_t pid;
	int id;
	char * state;
	char * command;
	jobs * next;
};

// contains information from the parsed command
typedef struct{
	// holds parsed command
	char ** parsed_token;
	// holds second part of input if pipe existed
	char ** parsed_token2;
	// holds entired parsed command
	char * command;
	// holds first input/output/error redirected files
	char * io_file_output;
	char * io_file_input;
	char * io_file_error;
	// holds second input/output/error redirected files if pipe exists
	char * io_file2_output;
	char * io_file2_input;
	char * io_file2_error;
	// bool determining whether pipe exists
	bool pipe;
	bool background_process;
} parsed_command;

// GLOBAL VARIABLE TO HOLD MOST RECENT JOB NUMBER
// GLOBAL ARRAY FOR ALL JOBS
int process_index = 0;
jobs * head = NULL;
pid_t recent_job;
pid_t parent_pid;
char * recent_command;
// FUNCTION HEADERS
parsed_command parse_input(char * input);
void jobs_func();
void execute_command(parsed_command parsed_input);
void execute_pipe_command(parsed_command parsed_input);
jobs * add_node(int id, char * state, char * command, pid_t pid, jobs * head);
void fg_func();
void bg_func();
void sig_handler_test();
//void sig_handler_sigchild(int signo);


void sig_handler_sigchild(int signo){
	int status;
	pid_t kek = waitpid(-1, &status, 0);
	printf("CHILD EXITED: %d\n", (int) kek);
}

void sig_handler_sigtstp(int signo){
	printf("%d\n", signo);
	if(signo != parent_pid){
		printf("RECENT JOB: %d\n", recent_job);
		kill(recent_job, SIGTSTP);
		printf("PARENT JOB: %d\n", parent_pid);
		kill(parent_pid, SIGCONT);
	}

}

void sig_handler_sigtstp_child(int signo){
	printf("parent: %d\n", parent_pid);
	printf("current pid: %d\n", tcgetpgrp(STDIN_FILENO));
	if(getpid() != parent_pid){
		printf("this worked?\n");
		add_node(process_index, "Stopped", recent_command, getpid(), head);
		process_index += 1;
		kill(-1, SIGTSTP);
		kill(parent_pid, SIGCONT);
	}
}

void sig_handler_test(int signo){
	// find process number and add into jobs list
	printf("\n");
	fflush(stdout);
}



int main() {
	// hold input command from stdin
	//char input[2000];
	char ** parsed_input;
	pid_t pid;
	char * input;
	int status = 0;
	pid_t wpid;

	parent_pid = getpid();
	parent_pid = getpid();
	setpgid(parent_pid, parent_pid);
	tcsetpgrp(0, parent_pid);


	// SIGNAL HANDLING
	
	if(signal(SIGINT, SIG_IGN) == SIG_ERR) printf("signal(SIGINT) error\n");
	if(signal(SIGTSTP, sig_handler_test) == SIG_ERR) perror("signal(SIGTSTP) error\n");
	if(signal(SIGCHLD, SIG_DFL) == SIG_ERR) perror("signal(SIGCHLD error \n");
	if(signal(SIGQUIT, SIG_IGN) == SIG_ERR) perror("signal(SIGQUIT error \n");
	if(signal(SIGTTIN, SIG_IGN) == SIG_ERR) perror("signal(SIGTTIN error \n");
	if(signal(SIGTTOU, SIG_IGN) == SIG_ERR) perror("signal(SIGTTOU error \n");
	/*if(signal(SIGTSTP, sig_handler_sigtstp_child) == SIG_ERR){
		printf("signal (SIGTSTP) error\n");
	}*/
	
	// default reaping of child
	/*if(signal(SIGCHLD, sig_handler_sigchild) == SIG_ERR){
		printf("signal (SIGCHLD) error\n");
	}*/
	/*if(signal(SIGCHLD, SIG_DFL) == SIG_ERR){
		printf("signal (SIGCHLD) error\n");
	}
	*/


	while(input = readline("# ")){
		// returns struct that contains parsed command string, redirection information, pipe information
		parsed_command parsed_input = parse_input(input);

		// PIPE COMMAND AHHH
		if(parsed_input.pipe == true){
			execute_pipe_command(parsed_input);
		} else {	// NO PIPE
			// CUSTOM COMMANDS
			if(parsed_input.parsed_token[0] != NULL && ((strcmp(parsed_input.parsed_token[0], "jobs") == 0) || (strcmp(parsed_input.parsed_token[0], "fg") == 0) || (strcmp(parsed_input.parsed_token[0], "bg") == 0))){
				if(strcmp(parsed_input.parsed_token[0], "jobs") == 0){
					jobs_func();
				}
				if(strcmp(parsed_input.parsed_token[0], "fg") == 0){
					fg_func();
				}
				if(strcmp(parsed_input.parsed_token[0], "bg") == 0){
					bg_func();
				}
			// normal command
			} else {
				execute_command(parsed_input);
			}
		}
		// free malloc stuff
		free(parsed_input.parsed_token);
		free(parsed_input.parsed_token2);
		//recent_job = getpid();
	}
}



parsed_command parse_input(char * input){
	parsed_command parsed_info = {
		.parsed_token = NULL,
		.parsed_token2 = NULL,
		.command = NULL,
		.io_file_output = NULL,
		.io_file_input = NULL,
		.io_file_error = NULL,
		.io_file2_output = NULL,
		.io_file2_input = NULL,
		.io_file2_error = NULL,
		.pipe = false,
		.background_process = false
	};
	// determine which file the redirection points to
	enum file_flag f_flag;
	int max_words = 20;
	int max_words2 = 20;
	int i = 0;
	int j = 0;
	// malloc space for input command tokens
	parsed_info.parsed_token = malloc(max_words * sizeof(char*));
	parsed_info.parsed_token2 = malloc(max_words2 * sizeof(char*));

	// flag to determine whether to ignore next word (word is a redirection file)
	bool ignore = false;


	// check for mem allocation error
	if(!parsed_info.parsed_token){
		printf("ALLOCATION ERROR\n");
		exit(EXIT_FAILURE);
	}
	if(!parsed_info.parsed_token2){
		printf("ALLOCATION ERROR\n");
		exit(EXIT_FAILURE);
	}

	// grab command
	if(strlen(input) != 0){
		parsed_info.command = strdup(input);
		recent_command = strdup(input);
	}

	// grab first word
	char * token = strtok(input, " \n\t");

	while(token != NULL){
		// check for i/o redirection
		if(strcmp(token, ">") == 0 || strcmp(token, "<") == 0 || strcmp(token, "2>") == 0){
			if(strcmp(token, ">") == 0){
				//parsed_info.io_redirect_output = token;
				f_flag = outputf;
			} else if (strcmp(token, "<") == 0){
				//parsed_info.io_redirect_input = token;
				f_flag = inputf;			
			} else {
				//parsed_info.io_redirect_error = token;
				f_flag = errorf;			
			}
			ignore = true;
		// check for pipe
		} else if (strcmp(token, "|") == 0){
			parsed_info.pipe = true;
		// check for background process
		} else if (strcmp(token, "&") == 0){
			parsed_info.background_process = true;
		// store redirect file info
		} else if(ignore){
			// after a redirection symbol is detected, save next path for redirection
			ignore = false;
			if(f_flag == outputf){

				if(!parsed_info.pipe){
					parsed_info.io_file_output = token;
					//printf("%s\n", parsed_info.io_file_output);
				} else {
					parsed_info.io_file2_output = token;
					//printf("%s\n", parsed_info.io_file2_output);
				}

			} else if(f_flag == inputf){

				if(!parsed_info.pipe){
					parsed_info.io_file_input = token;
					//printf("%s\n", parsed_info.io_file_input);
				} else {
					parsed_info.io_file2_input = token;
					//printf("%s\n", parsed_info.io_file2_input);
				}
			} else {

				if(!parsed_info.pipe){
					parsed_info.io_file_error = token;
					//printf("%s\n", parsed_info.io_file_error);
				} else {
					parsed_info.io_file2_error = token;
					//printf("%s\n", parsed_info.io_file2_error);
				}
			}
		} else {
			// save command token
			if(!parsed_info.pipe){
				parsed_info.parsed_token[i] = token;
				i++;
			} else {
				parsed_info.parsed_token2[j] = token;
				j++;
			}
		}
		// if malloc amount is too small, realloc 
		if(i >= max_words){
			max_words += 20;
			parsed_info.parsed_token = realloc(parsed_info.parsed_token, max_words * sizeof(char*));
			// check for mem allocation error
			if(!parsed_info.parsed_token){
				perror("ALLOCATION ERROR\n");
				exit(EXIT_FAILURE);
			}
		}

		if(j >= max_words2){
			max_words2 += 20;
			parsed_info.parsed_token2 = realloc(parsed_info.parsed_token, max_words2 * sizeof(char*));
			if(!parsed_info.parsed_token2){
				perror("ALLOCATION ERROR\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, " \n\t");
	}

	parsed_info.parsed_token[i] = NULL;
	//parsed_info.parsed_token2[j] = NULL;

	return parsed_info;
}

// print jobs table
void jobs_func(){
	jobs * current = head;
	while(current != NULL){
		if (current->next != NULL){
			printf("[%d]-	%s    			%s\n", current->id, current->state, current->command);	
		} else {
			printf("[%d]+	%s    			%s\n", current->id, current->state, current->command);	
		}
						
		current = current->next;
	}
}

// adds node to job linked list
jobs * add_node(int id, char * state, char * command, pid_t pid, jobs * head){
	jobs * process = (jobs*) malloc(sizeof(jobs));
	process->id = id;
	process->state = strdup(state);
	process->command = strdup(command);
	process->pid = pid;
	process->next = NULL;

	jobs * current = head;
	if(head == NULL){
		head = process;
	} else {
		while(current->next != NULL){
			current = current->next;
		}
		current->next = process;
	}
	return head;
}

// execute command
void execute_command(parsed_command parsed_input){
	int status;

	pid_t pid = fork();

			
	if(pid == 0){	// child process
		// SIGNAL HANDLING
		//signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
		//recent_job = getpid()
		if(signal(SIGINT, SIG_DFL) == SIG_ERR){
			printf("signal (SIGINT) child error\n");
		} 
		/*if(signal(SIGTSTP, sig_handler_sigtstp_child) == SIG_ERR){
			printf("signal (SIGTSTP) child error\n");
		}*/
		
		/*if(setpgid(getpid(), getpid()) < 0){
			perror("SETPGID FAIL\n");
			exit(EXIT_FAILURE);
		}*/

		// input redirection
		if(parsed_input.io_file_input != NULL){
			int file = open(parsed_input.io_file_input, O_RDONLY);
			if (file < 0){
				perror(parsed_input.io_file_input);
				exit(EXIT_FAILURE);
			}
			dup2(file, STDIN_FILENO);
			close(file);
		}

		// output redirection
		if(parsed_input.io_file_output != NULL){
			int file = open(parsed_input.io_file_output, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(file, STDOUT_FILENO);
			close(file);
		}
		// error redirection
		if(parsed_input.io_file_error != NULL){
			int file = open(parsed_input.io_file_error, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(file, STDERR_FILENO);
			close(file);
		}
	// execute command
		if(execvp(parsed_input.parsed_token[0], parsed_input.parsed_token) < 0){
			// WILL ONLY EXECUTE IF EXECVP FAILS
			printf("%s: command not found\n", parsed_input.parsed_token[0]);
			exit(EXIT_FAILURE);
		}
	
	} else if(pid < 0) {
		// ERROR CODE CAUSE FORK FAILED
		perror("FORK FAILED CHILD\n");
		exit(EXIT_FAILURE);
	} else {
	// parent process must wait for child process to finish
		if(parsed_input.background_process == false){
			waitpid(pid, &status, WUNTRACED | WCONTINUED);
			//wait(NULL);
		} else {
			head = add_node(process_index, "Running", parsed_input.command, pid, head);
			process_index += 1;
		}
	}
}

// execyte a ouoed cinnabd
void execute_pipe_command(parsed_command parsed_input){
	pid_t pid_c1;
	pid_t pid_c2;
	int status;

	int pfd[2];
	pipe(pfd);

	pid_c1 = fork();


	if(pid_c1 == 0){	// FIRST CHILD:

		//setpgid(getpid(), getpid());

		// SIGNAL HANDLING
		/*if(signal(SIGINT, SIG_DFL) == SIG_ERR){
			printf("signal (SIGINT) child error\n");
		}
		if(sigaction(SIGTSTP, sig_handler_sigtstp_child) == SIG_ERR){
			printf("signal (SIGTSTP) child error\n");
		}*/

		// input redirection
		if(parsed_input.io_file_input != NULL){
			int file = open(parsed_input.io_file_input, O_RDONLY);
			if (file < 0){
				perror(parsed_input.io_file_input);
				exit(EXIT_FAILURE);
			}
			dup2(file, STDIN_FILENO);
			close(file);
		}

		// output redirection
		if(parsed_input.io_file_output != NULL){
			int file = open(parsed_input.io_file_output, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(file, STDOUT_FILENO);
			close(file);
		}

		// error redirection
		if(parsed_input.io_file_error != NULL){
			int file = open(parsed_input.io_file_error, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(file, STDERR_FILENO);
			close(file);
		} 

		// pipe redirection (only valid is output wasn't redirected)
		if(parsed_input.io_file_output == NULL){
			// change STDUO to input to pipe
			close(pfd[0]);
			dup2(pfd[1], STDOUT_FILENO);
		}

		// executed command
		if(execvp(parsed_input.parsed_token[0], parsed_input.parsed_token) < 0){
			// WILL ONLY EXECUTE IF EXECVP FAILS
			printf("%s: command not found\n", parsed_input.parsed_token[0]);
			exit(EXIT_FAILURE);
		}

	} else if(pid_c1 < 0){	// PID child 1 is error
		perror("FORK ERROR CHILD 1\n");
		exit(EXIT_FAILURE);

	} else {	// PARENT CODE

		pid_c2 = fork();

		if(pid_c2 == 0){	// SECOND CHILD

			/*if(setpgid(getpid(), pid_c1) < 0){
				perror("SETPGID FAIL\n");
			}*/

			// SIGNAL HANDLING
			/*if(signal(SIGINT, SIG_DFL) == SIG_ERR){
				printf("signal (SIGINT) child error\n");
			}
			if(signal(SIGTSTP, sig_handler_sigtstp_child) == SIG_ERR){
				printf("signal (SIGTSTP) child error\n");
			}*/

			// input redirection
			if(parsed_input.io_file2_input != NULL){
				int file = open(parsed_input.io_file2_input, O_RDONLY);
				if (file < 0){
					perror(parsed_input.io_file2_input);
					exit(EXIT_FAILURE);
				}
				dup2(file, STDIN_FILENO);
				close(file);
			}

			// output redirection
			if(parsed_input.io_file2_output != NULL){
				int file = open(parsed_input.io_file2_output, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
				dup2(file, STDOUT_FILENO);
				close(file);
			}

			// error redirection
			if(parsed_input.io_file2_error != NULL){
				int file = open(parsed_input.io_file2_error, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
				dup2(file, STDERR_FILENO);
				close(file);
			} 

			// pipe redirection (only valid if the input hasn't been redirected)
			if(parsed_input.io_file2_input == NULL){
				// change STDIN to output of pipe
				//printf("SUCCESSFUL PIPE FOR CHILD2\n");
				close(pfd[1]);
				dup2(pfd[0], STDIN_FILENO);
			}

			// execute second command
			if(execvp(parsed_input.parsed_token2[0], parsed_input.parsed_token2) < 0){
				// WILL ONLY EXECUTE IF EXECVP FAILS
				printf("%s: command not found\n", parsed_input.parsed_token[0]);
				exit(EXIT_FAILURE);
			}

		} else if(pid_c2 < 0){	// error for child 2
			perror("FORK ERROR CHILD 2\n");
			exit(EXIT_FAILURE);
		} else {	// parent
			//wait(NULL);	// MAYBE NEED TO WAIT TWICE????????????????????
			close(pfd[0]);
			close(pfd[1]);

			// if its not a background process then wait
			if(parsed_input.background_process == false){
				waitpid(pid_c2, &status, WUNTRACED | WCONTINUED);
				//waitpid(pid_c2, &status, 0);
			} else {
				head = add_node(process_index, "Running", parsed_input.command, pid_c1, head);
				process_index += 1;
			}
		}
	}
}

// TODO
void fg_func(){}

// TODO
void bg_func(){}