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
	char * bgfg;
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
int process_index = 1;
jobs * head = NULL;
pid_t recent_job;
pid_t parent_pid;
char * recent_command;
pid_t live_process_pid;
int status;
// FUNCTION HEADERS
parsed_command parse_input(char * input);
void jobs_func();
void execute_command(parsed_command parsed_input);
void execute_pipe_command(parsed_command parsed_input);
jobs * add_node(int id, char * state, char * command, pid_t pid, jobs * head);
jobs * remove_node(pid_t process_id);
bool node_exists(pid_t process_id);
void fg_func();
void bg_func();
void sig_handler_test();
void sig_handler_sigtstp();
void sig_handler_sigint();
jobs * set_job_bgfg(pid_t process, int fgbg);
jobs * update_state(pid_t process_id, char * state);
//void sig_handler_sigchild(int signo);


void sig_handler_sigtstp(int signo){
	printf("\n**************************************** SIGTSTP HANDLER ****************************************\n");
	if(parent_pid != recent_job){
		// set recent stopped job flag to foreground
		printf("TSTP HANDLER sent kill to process that caught ^Z\n\n");
		kill(recent_job, SIGTSTP);
	}
	fflush(stdout);
}

void sig_handler_sigint(int signo){
	if(parent_pid != recent_job){
		printf("\n");
		fflush(stdout);
		kill(recent_job, SIGINT);
	}
}


void sig_handler_child(int signo){
	printf("\n**************************************** SIGCHLD HANDLER ****************************************\n");
	printf("STATUS in Child Handler: %d\n", status);
	if(WIFEXITED(status)){

		printf("forground process caught and exited normally\n");
		// if process in the foreground exits and it exists in the job table, remove it
		if(node_exists(recent_job)){
			printf("remove the job cause it exists in the table?\n");
			head = remove_node(recent_job);
		}

	} else if(WIFSTOPPED(status)){
		
		printf("foreground process caught tstp signal\n");

		// special case where process was stopped -> fg -> ^z so it should be stopped again
		if(node_exists(recent_job)){

			// set job status to stopped
			head = update_state(recent_job, "Stopped");
			// set background/forground status to background
			head = set_job_bgfg(recent_job, 1);

		} else {

			// add stopped command into jobs table
			head = add_node(process_index, "Stopped", recent_command, recent_job, head);
			//head = set_job_bgfg(recent_job, 1);
		
		}

	} else {

		printf("background process calling sig child\n");
		// catch backgroun dprocesses completing
		pid_t pid_handler = waitpid(-1, &status, WNOHANG);
	
		printf("Process ID in Handler: %d\n", pid_handler);
		printf("Recent Job in Handler: %d\n", recent_job);
	
		if(WIFSTOPPED(status)){

			printf("Successful signal caught\n");
			printf("THIS SHOULD NEVER RUN\n");
			head = set_job_bgfg(recent_job, 1);
			head = add_node(process_index, "Stopped", recent_command, pid_handler, head);

		} else if (WIFEXITED(status)){

			//printf("STATUS: %d\n", status);
			printf("Successful exit of background process: %d\n", pid_handler);
			head = update_state(pid_handler, "Done");

		}

	}
	status = -1;
	printf("\n");
}


int main() {
	// hold input command from stdin
	//char input[2000];
	char ** parsed_input;
	pid_t pid;
	char * input;
	pid_t wpid;

	parent_pid = getpid();
	printf("PARENT PID: %d\n", getpid());
	setpgid(parent_pid, parent_pid);
	//tcsetpgrp(0, parent_pid);
	recent_job = getpid();
	live_process_pid = getpid();
	status = -1;
	// SIGNAL HANDLING
	
	if(signal(SIGINT, sig_handler_sigint) == SIG_ERR) printf("signal(SIGINT) error");
	if(signal(SIGTSTP, sig_handler_sigtstp) == SIG_ERR) perror("signal(SIGTSTP) error");
	if(signal(SIGCHLD, sig_handler_child) == SIG_ERR) perror("signal(SIGCHLD error");
	//if(signal(SIGQUIT, SIG_IGN) == SIG_ERR) perror("signal(SIGQUIT error\n");
	//if(signal(SIGTTIN, SIG_IGN) == SIG_ERR) perror("signal(SIGTTIN error\n");
	//if(signal(SIGTTOU, SIG_IGN) == SIG_ERR) perror("signal(SIGTTOU error\n");


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
		printf("recent job shell: %d\n", recent_job);
		// free malloc stuff
		free(parsed_input.parsed_token);
		free(parsed_input.parsed_token2);
		live_process_pid = getpid();
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
				perror("ALLOCATION ERROR");
				exit(EXIT_FAILURE);
			}
		}

		if(j >= max_words2){
			max_words2 += 20;
			parsed_info.parsed_token2 = realloc(parsed_info.parsed_token, max_words2 * sizeof(char*));
			if(!parsed_info.parsed_token2){
				perror("ALLOCATION ERROR");
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
// will need to remove done nodes *********************************
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
	if(node_exists(pid)){
		head = update_state(pid, state);
		return head;
	}
	process_index += 1;
	jobs * process = (jobs*) malloc(sizeof(jobs));
	process->id = id;
	process->bgfg = "background";
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

	pid_t pid = fork();

			
	if(pid == 0){	// child process

		// input redirection
		if(parsed_input.io_file_input != NULL){
			printf("SUCCESS: <\n");
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
			printf("SUCCESS: >\n");
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
		perror("FORK FAILED CHILD");
		exit(EXIT_FAILURE);
	} else {
	// parent process must wait for child process to finish
		// ***************************************************************************
		//jobs
		//setpgid(pid, pid);
		recent_job = pid;
		live_process_pid = pid;
		if(parsed_input.background_process == false){
			//printf("child pid?: %d\n", pid);
			if((recent_job = waitpid(pid, &status, WUNTRACED | WCONTINUED)) == -1){
				perror("WAITPID failed");
			}
			if(WIFSTOPPED(status)){
				printf("caught ^z in yash\n");
			}
			status = -1;
			//printf("STATUS: %d\n", status);
			// FG BG COMMANDS
			//wait(NULL);
		} else {
			head = add_node(process_index, "Running", parsed_input.command, pid, head);
		}
	}
}

// execute a piped command
void execute_pipe_command(parsed_command parsed_input){
	pid_t pid_c1;
	pid_t pid_c2;

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
		perror("FORK ERROR CHILD 1");
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
			perror("FORK ERROR CHILD 2");
			exit(EXIT_FAILURE);
		} else {	// parent
			close(pfd[0]);
			close(pfd[1]);

			setpgid(pid_c1, pid_c1);
			setpgid(pid_c2, pid_c1);

			recent_job = pid_c1;
			live_process_pid = pid_c1;
			// if its not a background process then wait
			if(parsed_input.background_process == false){
				waitpid(pid_c2, &status, WUNTRACED | WCONTINUED);
				status = -1;
				//waitpid(pid_c2, &status, 0);
			} else {
				head = add_node(process_index, "Running", parsed_input.command, pid_c1, head);
			}
		}
	}
}

// check if process exists in job table
bool node_exists(pid_t process_id){
	jobs * current = head;
	while(current != NULL){
		if(current->pid == process_id){
			return true;
		}
		current = current->next;
	}
	return false;
}

jobs * update_state(pid_t process_id, char * state){
	jobs * current = head;
	while(current != NULL){
		if(current->pid == process_id){
			current->state = strdup(state);
		}
		current = current->next;
	}
	return head;
}

// TODO
void fg_func(){
	jobs * current = head;
	if(current == NULL){
		return;
	}
	while(current->next != NULL){
		current = current->next;
	}

	current->state = "Running";
	current->bgfg = "foreground";
	printf("current->pid: %d\n", current->pid);
	
	recent_job = current->pid;
	live_process_pid = current->pid;
	recent_command = current->command;
	
	if(kill(current->pid, SIGCONT)) perror("Kill (SIGCONT) error");
	printf("are we getting here?\n");
	
	if((waitpid(current->pid, &status, WUNTRACED)) < 0) perror("WAIT PID ERROR");
	
	status = -1;
}

// TODO
void bg_func(){
	jobs * current = head;
	jobs * process = current;
	if(current == NULL){
		return;
	}
	while(current != NULL){
		if(strcmp(current->state, "Stopped")){
			while(current != process){
				process = process->next;
			}
		}
		current = current->next;
	}
	process->state = "Running";
	//recent_job = process->pid;
	//recent_command = process->command;
	if(kill(process->pid, SIGCONT)) perror("Kill (SIGCONT) error");
	status = -1;
}

// need to work on this function >:( maybe works now?
jobs * remove_node(pid_t process_id){
	//printf("remove node process id: %d\n", process_id);
	printf("process id received into remove_node function: %d\n", process_id);
	jobs * current = head;
	jobs * prev = NULL;
	if(head == NULL){
		return NULL;
	}
	if (head->next == NULL){
		if(head->pid == process_id){
			if(strcmp(head->bgfg, "foreground") == 0){
				//free(head->state);
				free(head->command);
				free(head);
				return NULL;	
			} else {
				head->state = "Done";
			}
		}
		return head;
	}
	while(current != NULL){
		if(current->pid == process_id){
			if(strcmp(head->bgfg, "foreground") == 0){
				//free(current->state);
				free(current->command);
				prev->next = current->next;
				free(current);				
			} else {
				current->state = "Done";
			}

		}
		prev = current;
		current = current->next;
	}
	return head;
}

jobs * set_job_bgfg(pid_t process, int fgbg){
	jobs * current = head;
	while(current != NULL){
		if(current->pid == process){
			if(fgbg){
				current->bgfg = "background";				
			} else {
				current -> bgfg = "foreground";
			}

		}
		current = current->next;
	}
	return head;
}