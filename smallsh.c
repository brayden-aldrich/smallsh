/**
* Name: Brayden Aldrich
* Date: 2/23/24
* Program: smallsh
*/
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

bool foregroundOnly = 0; // if true, the program ignores &
pid_t spawnPid; // spawnPid global variable


/**
* SIGSTP signal handler. Updates foregroundOnly and displays relevant messages
*/
void handle_SIGTSTP(int sig){
	
	
	if(!foregroundOnly){
		foregroundOnly = 1;
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, strlen(message));
		fflush(stdout);
	} else {
		foregroundOnly = 0;
		char* message = "Exiting foreground-only mode\n";
		write(STDOUT_FILENO, message, strlen(message));
		fflush(stdout);
	}
	
}

int main(){
	/* SET UP AND INSTALL SIGINT HANDLING */
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = SIG_IGN; // Default SIGINT to ignore 
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;

	/* SET UP AND INSTALL SIG HANDLING */
	struct sigaction si = {0};
	si.sa_handler = handle_SIGTSTP; // set to handle_SIGTSTP
	sigfillset(&si.sa_mask);
	si.sa_flags = SA_RESTART; // Fixes issues surrounding SIGTSTP handling
	sigaction(SIGTSTP, &si, NULL);

	
   	int childStatus = 0;
	
    do {
		printf(":");
		fflush(stdout);

		char command[2048];

		sigaction(SIGINT, &SIGINT_action, NULL); // SET SIGINT handle to ignore it
		
		// if valid input
        if(fgets(command, sizeof(command) - 1, stdin)){
			fflush(stdin);

			const char* comment = "#";

			char* pos = strstr(command, "$$"); // grab position of $$ in command
			char buffer[2048];
			if(pos != NULL){ // if position exists
				char before[2048];
				char after[2048];
				// using pointer math, copy specific amount from command ie cd test$$ -> copy first 6 chars to before
				strncpy(before, command, pos - command);
				before[pos - command] = '\0'; 
				// move pointer 2 chars forward and copy the rest to after
				strcpy(after, pos + 2);
				after[strlen(command)] = '\0';

				sprintf(buffer, "%s%d%s", before, getpid(), after);

			} else{
				strcpy(buffer, command); // weirdness with strtok
			}
			

			char *tok;
			bool ignore = 0;
			tok = strtok(buffer, " \n");
			char *args[500];
			int idx = 0;
			// if enter key is pressed continue and ignore
			if(tok == NULL){
				continue;
			}
			// if comment continue and ignore
			if(strncmp(tok, "#", 1) == 0){
				continue;
			}
			
			//strtok and hold it in arg pointer array
			while(tok != NULL){
				args[idx] = strdup(tok);
				idx++;
				tok = strtok(NULL,  " \n");
			}
			
			// /************************************/
			if(strcmp(args[0], "exit") == 0){ // if exit, exit
				break;
			}else if (strcmp(args[0], "cd") == 0){
				// set path to home directory if only given "cd"
				if(idx == 1){
					args[1] = getenv("HOME");
				}
				int x = chdir(args[1]);
				if(x == -1){
					printf("Folder not found\n"); 	
					fflush(stdout);
				}
			}else if(strcmp(args[0], "status") == 0){ // display childStatus, if exited or terminated
				if(WIFEXITED(childStatus)){
					printf("Exit value %d\n", WEXITSTATUS(childStatus));
					fflush(stdout);
				}else{
					printf("Terminated by signal %d\n", WTERMSIG(childStatus));
					fflush(stdout);
				}
			} else { 
				/*	MAIN FORK AREA	*/

				bool background = 0; 
				int x = 0;

				// if need to run in background
				if(strcmp(args[idx - 1], "&") == 0){
					// if not foreground only
					if(!foregroundOnly){
						background = 1;
					}	
					//update args array to remove the & from command
					idx--;
					args[idx] = '\0';
				} 

				spawnPid = fork();
				
				switch(spawnPid){
					case -1:
						perror("ruh roh, fork() failed!");
						exit(1);
						break;
					case 0: 
						fflush(stdout);
						// if function is in background, then set SIGINT to default behaviors
						if(!background){
							signal(SIGINT, SIG_DFL);
						}

						char *c = buffer; // execlp didn't like passing the buffer into it
						if(idx == 1){
							execlp(c, c, NULL);
							perror("Error, failed to execute: ");
							exit(1);
						} else { 
							
							/*	CHECK FOR REDIRECTION	*/
							char* comCut[500]; // this is the array that will be passed to execvp. Will be updated to not include < or >
							int cci = 0;
							int l;
							bool redirect = 0;
							// loop to find and redirect <>
							for(l = 0; l < idx; l++){
								//if output
								if(strcmp(">", args[l]) == 0){
									l++; // increment to ignore >
									int fp = open(args[l], O_WRONLY | O_CREAT | O_TRUNC, 0666); // open output file in the name of satan
									if(fp == -1){
										perror("Cannot open file\n");
										exit(1);
									}
									dup2(fp, STDOUT_FILENO);
									fcntl(fp, F_SETFD, FD_CLOEXEC); // close fp
									redirect = 1;
							
								} else if (strcmp("<", args[l]) == 0){
									l++; // increment to ignore <
									int fp = open(args[l], O_RDONLY, 0666); // open input file in read only 
									if(fp == -1){
										perror("Cannot open file");
										exit(1);
									}
									dup2(fp, STDIN_FILENO); 
									fcntl(fp, F_SETFD, FD_CLOEXEC); //close fp
									redirect = 1;
									
								} else {
									comCut[cci] = args[l]; // else just set comCut[current comCut index] to args[l]
									cci++;
								}
							}
							// if there is no redirect and it's run in background, set redirects to /dev/null
							if(background && !redirect){
								int devNull = open("/dev/null", O_RDWR);
								dup2(devNull, STDOUT_FILENO);
								dup2(devNull, STDIN_FILENO);
								dup2(devNull, STDERR_FILENO);
								close(devNull);
							}
							
							comCut[cci] = NULL; // null term the array
							
							execvp(comCut[0], comCut);
							perror("execvp failed");  
							exit(1);
						}	
						break;
					default:
					// if command isn't run in the background or foreground only is enabled, wait for pid
					// else just steamroll ahead
						if(!background || foregroundOnly){
							spawnPid = waitpid(spawnPid, &childStatus, 0);
						}else {
							printf("Background pid is: %d\n", spawnPid); 
							fflush(stdout);
						}
					/*		CHECK FOR CHILD BACKGROUND JOBS		*/
					pid_t childPid;
					int childStatus = -1;
						
					while ((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
						if (WIFEXITED(childStatus)) {
							printf("Background pid %d is done: exit value %d\n", childPid, WEXITSTATUS(childStatus));
							fflush(stdout);
						}else if(WIFSIGNALED(childStatus)){
							printf("Terminated pid %d with signal %d\n", childPid, WTERMSIG(childStatus));
							fflush(stdout);
						}	
					}
				}
			
			}
        }
		
    } while(1);
	
	return 0;
	
}