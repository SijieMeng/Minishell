#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define ENDCHAR(str) (str + strlen(str) - 1)

typedef struct st_job {
	pid_t pid;
	char command[100];
	struct st_job* next;
} job_st;

int easeSpaceAtTail(char * input_string) {
	char * endpoint = ENDCHAR(input_string);
	while (isspace(*endpoint)) {
		*endpoint-- = '\0';
	}
	return 0;
}

// used to split the input command into the array which is working as parameter of execvp systemcall.
// return 0, running at the foreground
//        1, running at the background
//       -1, the command shouldn't run
int splitStrToArr(char * input_string, char ** output_array) {
	// if the command is end of '\n', replace it with '\0'
	// otherwise, the execvp will report error
	char *pos;
	if ((pos = strchr(input_string, '\n')) != NULL)
		*pos = '\0';

	// get rid of the space at the end of input.
	easeSpaceAtTail(input_string);
	//	printf("%u --%s--\n", strlen(input_string),input_string);

	// if the ending character is &, that means the command should run at the background.
	// the '&' should be removed then. the return value should change to 1
	int re = 0;
	if (*(ENDCHAR(input_string)) == '&') {
		re = 1;
		*(ENDCHAR(input_string)) = '\0';
	}

	// get rid of the space again after remove the &
	if (re == 1)
		easeSpaceAtTail(input_string);

	// if the input is just a chain of space, that means the command is null
	// the command shouldn't run
	if (strlen(input_string) == 0)
		re = -1;

	// use strtok to get each parameter and set them into the array.
	int idx = 0;
	char * token = strtok(input_string, " ");
	//	printf("--1-- **%s**\n", token);
	while (token != NULL) {
		output_array[idx] = token;
		token = strtok(NULL, " ");
		//		printf("--%d-- **%s**\n", idx, token);
		idx++;
	}
	return re;
}

void printJobList(job_st* node) {
	if (node == NULL)
		return;

	job_st* curr = node;
	printf("\n");
	while (1) {
		printf("%d->", curr->pid);
		if (curr->next == curr) {
			break;
		} else {
			curr = curr->next;
		}
	}
	printf("\n");
}

job_st* listJobs(job_st* jobQHead) {
	if (jobQHead == NULL) {
		printf("No Background Process is running now.\n");
		return NULL;
	} else {
		job_st* head = jobQHead;
		job_st* prev = jobQHead;
		job_st* curr = jobQHead;
		pid_t pid;

		while (1) {
//			printJobList(head);
			// check the status of the process. if the return pid is the process number, that means finished, otherwise, not finished
			pid = waitpid(curr->pid, NULL, WNOHANG);
			// show the information of the process
			printf("[%d] %s %s\n", curr->pid, curr->command, pid == 0 ? "Running" : "Finished");

			// if the process has been finished, remove it from the list.
			if (pid > 0) {
				if (head == curr && curr->next == curr) { // just one element on the list
//					printf("==1==\n");
					head = NULL;
					free(curr);
					break;
				} else if (curr == head && curr->next != curr) { // more than one elements, and the current element is also the head of the list
//					printf("==2==\n");
					head = curr->next;
					prev = curr->next;
					free(curr);
					curr = head;
				} else if (curr != head && curr->next == curr) { // curr element is the end of list.
//					printf("==3==\n");
					prev->next = prev;
					free(curr);
					break;
				} else if (curr != head && curr->next != curr) { // curr element is in the middle.
//					printf("==4==\n");
					prev->next = curr->next;
					free(curr);
					curr = prev->next;
				} else {
					assert(1 == 0);
				}
			} else {
				if (curr->next == curr)
					break;
				prev = curr;
				curr = curr->next;
			}
		}
		return head;
	}
}

job_st* addBGJobToList(job_st* jobQHead, pid_t pid, char* command) {
	// save the process_id into an list.
	job_st* job = (job_st*) malloc(sizeof(job_st));
	job->pid = pid;
	strcpy(job->command, command);
	// if the job Queue is null, set the next of this new job to itself
	if (jobQHead == NULL) {
		job->next = job;
	} else { // otherwise, point to the node the jobQHead points to
		job->next = jobQHead;
	}
	// set jobQHead to the new node.
	jobQHead = job;

	return jobQHead;
}


int main(void) {
	char command[1024];
	pid_t pid; // child process id
	char *argv[20];
	int flag; // 0:foreground 1:background -1:not running
	job_st* jobQHead = NULL;

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	while (1) {
		flag = 0;
		printf("minish>");
		fflush(stdin);         // flush mem
		fgets(command, 1024, stdin); //get a input from standard input and set it to command

		// format the input command into the array with their parameter.
		// at the same time, check whether the command is just spaces, and whether the command should be run at background.
		memset(argv, 0, sizeof(argv));
		flag = splitStrToArr(command, argv);

		// if the command is just space, don't do anything, and ask to type a new command
		if (flag == -1)
			continue;

		// for command "cd", it sould be run at the father process directly.
		if (strcmp(argv[0], "cd") == 0) {
			chdir(argv[1]);
			continue;
		}

		// list the background process
		if (strcmp(argv[0], "listjobs") == 0) {
			jobQHead = listJobs(jobQHead);
			continue;
		}

		// bring a background process to foreground
		if (strcmp(argv[0], "fg") == 0) {
			waitpid(atoi(argv[1]), NULL, WUNTRACED);
			continue;
		}

		// exit
		if (strcmp(argv[0], "exit") == 0) {
			if (jobQHead == NULL)
				return 0;
			// send kill signal to all child process to ask them exit.
			job_st* curr = jobQHead;
			while (1) {
				kill(curr->pid, SIGABRT);
				if (curr->next == curr) {
					break;
				} else {
					curr = curr->next;
				}
			}
			return 0;
		}

		// for other commands, they have to run on the child processes
		pid = fork();
		if (pid < 0) {
			printf("error\n");
			exit(0);
		} else if (pid == 0) { // child processes
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);
			signal(SIGTTIN, SIG_DFL);
			signal(SIGTTOU, SIG_DFL);
			signal(SIGCHLD, SIG_DFL);

			if (execvp(argv[0], argv) < 0) {
				fprintf(stderr, "Error when execute %s, the error information is :%s \n", argv[0], strerror(errno));
			}
			exit(0);
		} else { // when pid is larger than 0, it is a father process
			// run command as a foreground process.
			if (flag == 0) {
				int child_status;
				waitpid(pid, &child_status, WUNTRACED);
				// run command as a background process.
			} else {
				// save the process_id into an list.
				printf("[%d]\n", pid);
				jobQHead = addBGJobToList(jobQHead, pid, argv[0]);
			}
		}

	}

	return EXIT_SUCCESS;
}

