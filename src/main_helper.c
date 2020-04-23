#include "jobs_helper.h"
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>


volatile sig_atomic_t sigChildFlag = 0;

char *help_text = "help (0 args) Print this help message\n"
"quit (0 args) Quit the program\n"
"enable (0 args) Allow jobs to start\n"
"disable (0 args) Prevent jobs from starting\n"
"spool (1 args) Spool a new job\n"
"pause (1 args) Pause a running job\n"
"resume (1 args) Resume a paused job\n"
"cancel (1 args) Cancel an unfinished job\n"
"expunge (1 args) Expunge a finished job\n"
"status (1 args) Print the status of a job\n"
"jobs (0 args) Print the status of all jobs\n";

// 0 means success, 1 means failure, -1 means something went wrong during a function call
int hookHandler(void) {
    // printf("IN HOOK HANDLER FUNCTION\n");
    // debug("Signal Child Flag is set to %d\n", sigChildFlag);
    // debug("sig child flag is 0");
    if(sigChildFlag != 0) {
        debug("sig child flag is 1");
        // ie there is something to reap/handle
        int status;
        pid_t receivedPID; 
        // pid_t receivedPID = waitpid(-1, &status, WNOHANG);
        // debug("(receivedPID) WAITPID WAS %d", receivedPID);
        // pid_t receivedPID2 = waitpid(0, &status, WNOHANG); 
        // debug("WAITPID 1 IS %d\n", receivedPID); 
        // debug("WAITPID 2 IS %d\n", receivedPID2); 
        while ((receivedPID = (waitpid(-1, &status, WNOHANG | WUNTRACED)))>0) {
            // debug("(receivedPID) WAITPID WAS %d", receivedPID);
            debug("IN HANDLER FUNCTION: WAITPID IS %d\n", receivedPID);  
            if(receivedPID > -1) {
                // ie process has children
                debug("(IN FUNCTION) IN SIGNAL HANDLER: WAITPID IS %d\n", receivedPID); 
                int jobid = find_job_index(receivedPID);

                if(WIFEXITED(status)) {
                    debug("Child terminated normally\n");
                    if(job_get_status(jobid) == CANCELED) setJobTerminatedAfterCancel(jobid, 1);
                    // then in that case we change the job status & run any spooled jobs by calling run_spooled_jobs
                    if(jobid != -1) {
                        JOB_STATUS oldJobStatus = job_get_status(jobid);
                        int exitStatus = WEXITSTATUS(status);
                        sf_job_end(jobid, receivedPID, exitStatus);
                        setJobStatus(jobid, COMPLETED);
                        sf_job_status_change(jobid, oldJobStatus, COMPLETED);
                        setExitStatus(jobid, exitStatus);
                        decrementNumRunners();
                        run_spooled_jobs();
                        return 0;
                    } else {
                        debug("COULD NOT FIND THE PROPER JOB ID");
                        return -1;
                    }
                } else {
                    debug("Child terminated abnormally\n"); // Covers below case?
                }
                if(WIFSIGNALED(status)) {
                    // if if was terminated by a signal
                    int terminatingStatus = WTERMSIG(status); 
                    debug("Child was terminated by a signal");
                    debug("Number of signal causing child to terminate: %d\n", terminatingStatus);
                    (void) terminatingStatus;
                    if(job_get_status(jobid) == CANCELED) setJobTerminatedAfterCancel(jobid, 1);
                    if(jobid != -1) {
                        JOB_STATUS oldStatus = job_get_status(jobid);
                        sf_job_end(jobid, receivedPID, WTERMSIG(status));
                        setJobStatus(jobid, ABORTED);
                        sf_job_status_change(jobid, oldStatus, ABORTED);
                        decrementNumRunners();
                        // decrementNumJobs();, we have to do this when "expunge" is called!
                        run_spooled_jobs();
                        return 0;
                    } else {
                        debug("COULD NOT FIND THE PROPER JOB ID");
                        return -1;
                    } 
                }
                if(WIFSTOPPED(status)){
                    // ie if child was stopped by delivery of signal
                    // ie if "stopped"
                    int signalCause = WSTOPSIG(status);
                    debug("Child was stopped by delivery of a signal");
                    debug("The signal causing the stop was: %d\n", signalCause);
                    (void) signalCause;
                    if(jobid != -1) {
                        sf_job_pause(jobid, receivedPID);
                        // Below 2 being taken care of in pause function
                        // sf_job_status_change(jobid, job_get_status(jobid), PAUSED);
                        // setJobStatus(jobid, PAUSED);
                        return 0;
                    } else {
                        debug("COULD NOT FIND THE PROPER JOB ID");
                        return -1;
                    }
                }
                // WCONTINUED
                if(WIFCONTINUED(status)) {
                    debug("Child was continued");
                     if(jobid != -1) {
                        sf_job_resume(jobid, receivedPID);
                        // Below 2 being taken care of in pause function
                        // sf_job_status_change(jobid, job_get_status(jobid), PAUSED);
                        // setJobStatus(jobid, PAUSED);
                        return 0;
                    } else {
                        debug("COULD NOT FIND THE PROPER JOB ID");
                        return -1;
                    }
                }

            }
            // ie must be "parent" process
        } 
        sigChildFlag = 0;
    }
    // printf("arg0 is %d", arg0);
    return 1;
} 


char *getFirstWord(char* userInput) {
    int idxOfSpace = 0;
    int len = strlen(userInput);
    for(int i = 0; i < len; i++) {
        if(userInput[i] == ' ') break;
        idxOfSpace++;
    }
    // while(*userInput != ' ') {
    //     userInput++;
    //     idxOfSpace++;
    // }
    char *firstWord = (char *) malloc(idxOfSpace+1);
    strncpy(firstWord, userInput, idxOfSpace);
    firstWord[idxOfSpace] = '\0';
    return firstWord;
}

char *getSecondWord(char* userInput) {
    int firstIndex = -1;
    int secondIndex = -1;
    int len = strlen(userInput);
    for (int i = 0; i < len; i++) { 
        if(userInput[i] == ' ') {
            if(firstIndex == -1) firstIndex = i;
            }
    }

    secondIndex = len;
    
    if(firstIndex != -1 && secondIndex != -1) {
        int byteCount = secondIndex - firstIndex;
        char *task_name = malloc(byteCount);
        strncpy(task_name, userInput+firstIndex+1, byteCount-1);
        task_name[byteCount-1] = '\0'; 
        return task_name;
    }
    
    return NULL;
}

char *getTaskStr(char* userInput) {
    int firstIndex = -1;
    int secondIndex = -1;
    int len = strlen(userInput);
    for (int i = 0; i < len; i++) { 
        if(userInput[i] == '\'') {
            if(firstIndex == -1) {
                firstIndex = i;
            } else {
                secondIndex = i;
            }
        }
    }
    // assume everything wrapped in single quotes
    if(firstIndex != -1 && secondIndex != -1) {
        int byteCount = secondIndex - firstIndex;
        char *task_name = malloc(byteCount);
        strncpy(task_name, userInput+firstIndex+1, byteCount-1);
        task_name[byteCount-1] = '\0'; 
        return task_name;
    }
    // if not wrapped in single quotes
    return NULL;
}

int getNumOfArgs(char *userInput) {
    int numOfArgs = 0;
    int len = strlen(userInput);
    for (int i = 0; i < len-1; i++) { 
        if(userInput[i] == ' ' && (userInput[i+1] && userInput[i+1] != ' ')) {
            numOfArgs++;
        }
    }
    return numOfArgs;
}

int digits_only(char *userInput)
{
    while (*userInput) {
        if (isdigit(*userInput++) == 0) return 0;
    }

    return 1;
}

void printJobStatus(int jobid) {
    // if if job exists
    if(job_get_taskspec(jobid)) { 
        // ie if job exists
        if(job_get_status(jobid) == COMPLETED) {
            printf("job %d [%s (%d)]: %s\n", jobid, job_status_names[job_get_status(jobid)], getExitStatus(jobid),job_get_taskspec(jobid));
        } else {
            printf("job %d [%s]: %s\n", jobid, job_status_names[job_get_status(jobid)], job_get_taskspec(jobid));
        }
    }
};

void printJobs() {
    char *enabledStatus = jobs_get_enabled() ? "enabled" : "disabled"; 
    printf("Starting jobs is %s\n", enabledStatus);
    for(int i = 0; i < MAX_JOBS; i++) {
        printJobStatus(i);
    }
};




// MY FUNCTIONS
int get_num_of_commands(COMMAND_LIST *current_cmd) {
    int i = 0;
    while(current_cmd) {
        i++;
        current_cmd = current_cmd->rest;
    }
    return i;
}

int get_num_of_words(WORD_LIST *init_word) {
    int i = 0;
    while(init_word) {
        i++;
        init_word = init_word->rest;
    }
    return i;
}


char** build_cmd_args(COMMAND *init_cmd) {
    WORD_LIST *dup_words = init_cmd->words;
    int numOfWords = get_num_of_words(init_cmd->words);
    char **wordsArr = malloc((numOfWords + 1) * sizeof(char*));
    for(int i = 0; i < numOfWords; i++) {
        wordsArr[i] = dup_words->first;
        dup_words = dup_words->rest;
    }
    wordsArr[numOfWords] = NULL;
    return wordsArr;
}

void unmalloc_cmd_args(char **cmd_args) {
    char **dup_cmd_args = cmd_args;
    while(*dup_cmd_args != NULL) {
         free(*dup_cmd_args);
         dup_cmd_args+=1;
    }
    free(cmd_args);
}

void setup_output_path(char *output_path){
    int fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    // Correct behavior for inability to open file?
    if(fd == -1) {
        perror("Could not open the file");
        abort(); // abort as in demo, make sure we set status & handle status change & all that good shit
    }
    if(dup2(fd, STDOUT_FILENO) == -1) {
        perror("an error occured in calling dup2");
        abort();
    }
    close(fd); 
}

void setup_input_path(char *input_path) {
    int fd = open(input_path, O_RDONLY);
    // Correct behavior for inability to open file?
    if(fd == -1) {
        perror("Could not open the input file. Does it exist?");
        abort(); // abort as in demo
    }
    if(dup2(fd, STDIN_FILENO) == -1) {
        perror("an error occured in calling dup2");
        abort();
    }
    close(fd);
}

// void handler(int sig)
// {
//   pid_t pid;

//   pid = wait(NULL);

//   // do stuff here based on pid/signal values
// }

void closePipes(int pipes[], int pipeSize) {
    for(int i = 0; i < pipeSize; i++) {
        if(close(pipes[i]) == -1) {
            perror("an error occured in closing this pipe");
            abort();
        }
    }
}

void handle_sigchld(int sig) {
    debug("HANDLING SIG CHILD");
    int saved_errno = errno;
    sigChildFlag = 1;
    errno = saved_errno;
}



void run_spooled_jobs() {
    // if not enabled then can't run any jobs!
    int isEnabled = jobs_get_enabled();
    if(isEnabled == 0) return; 
    for(int i = 0; i < MAX_JOBS; i++) {
        int numOfRunners = getNumRunners();
        JOB_STATUS jobStatus = job_get_status(i);
        if((numOfRunners < MAX_RUNNERS) && (jobStatus != -1) && (jobStatus == WAITING)) {
            incrementNumRunners();
            setJobStatus(i, RUNNING);
            // job_table[i].jobStatus = RUNNING;
            sf_job_status_change(i, WAITING, RUNNING);
            struct sigaction sa;
            sa.sa_handler = &handle_sigchld;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_RESTART;
            if (sigaction(SIGCHLD, &sa, 0) == -1) {
                perror(0);
                exit(1);
            }
            pid_t pid_runner = fork();
            // should add error (pid_runner < 0) handling to this!
            if(pid_runner == -1) {
                perror("an error occured in forking pid_runner");
                abort();
            }
            if(pid_runner == 0) {
                // signal(SIGINT, SIG_DFL);
                // RUNNER PROCESS (Child Process)
                // debug("Current process id: %d\n", getpid());
                // debug("Current parent process id: %d\n", getppid());
                // debug("Current group process id: %d\n", getpgrp());
                // debug("Setting pgid");
                if(setpgid(0,0) == -1) {
                    perror("an error occured in setting the pgid");
                    abort();
                }; 
                debug("IN RUNNER PROCESS: MY PGID IS: %d\n", getpgrp());
                debug("IN RUNNER PROCESS: MY PID IS: %d\n", getpid());
                // iterate through pipelines
                // tasksTable[0]->pipelines->first->commands->first->words->rest->rest->first
                PIPELINE_LIST* pipelinePtr = getJobPipelines(i);
                // PIPELINE_LIST* pipelinePtr = job_table[i].task->pipelines;
                
                while(pipelinePtr) {
                    PIPELINE* actualPipeline = pipelinePtr->first;
                    // check if actualPipeline == null! ?


                    char *input_path = actualPipeline->input_path;  
                    char *output_path = actualPipeline->output_path;
                    char **cmd_args;

                     
                    // commands need to run concurrently, stdin of one goes into stdout of another
                    // if only 1 command then ez
                    COMMAND_LIST *commands = actualPipeline->commands;

                    int numOfCmds = get_num_of_commands(commands);
                    int status;
                    pid_t cpid;
                    if(numOfCmds == 1) {
                        // if only 1 command then no need for pipes
                        if((cpid = fork()) < 0) {
                            perror("an error occured in forking this child process");
                            abort();
                        }
                        if(cpid == 0) {
                            debug("(1 COMMAND) IN FORKED CHILD COMMAND: MY PGID IS: %d\n", getpgrp());
                            debug("(1 COMMAND) IN FORKED CHILD COMMAND: MY PID IS: %d\n", getpid());
                            // In child process
                            if(output_path) {
                                // would abort if error so no need for if/else
                                setup_output_path(output_path);
                            }

                            if(input_path) {
                                // would abort if error so no need for if/else
                                setup_input_path(input_path);
                            }

                            // builds cmd array, should UNMALLOC it after we are finished or if some abnormal termination occurs
                            cmd_args = build_cmd_args(commands->first);
                            // memory gets free'd on termination so don't need to do anything?
                            if(execvp(cmd_args[0], cmd_args) == -1) {
                                debug("error in running %s\n", cmd_args[0]);
                                perror("execvp failed");
                                unmalloc_cmd_args(cmd_args);
                                abort(); // Prolly need to abort here
                            };
                        }
                        // IN PARENT PROCESS
                        // wait pid & handle stuff here!
                        waitpid(-1, &status, 0);
                        int exitStatus;
                        if(WIFEXITED(status) ) {
                        // if if terminated normally, with exit() or returning from main basically
                            exitStatus = WEXITSTATUS(status); // get exit status
                            debug("pipeline exited with status %d\n", exitStatus);
                            if(pipelinePtr->rest == NULL) {
                                // if last pipeline, then exit pipeline master process w/ exit code
                                debug("NO MORE PIPELINES, EXITING WITH EXIT STATUS %d", exitStatus);
                                exit(exitStatus);
                            }
                            // check if last pipeline, if so, make sure exited from runner proocess correctly/aborted corrected
                        } else {
                            debug("Child terminated abnormally\n"); // Covers below case?
                        }
                        if(WIFSIGNALED(status)) {
                            // if if was terminated by a signal
                            exitStatus = WTERMSIG(status); 
                            debug("Number of signal causing child to terminate: %d\n", exitStatus);
                            // check if last pipeline, if so, make sure exited from runner proocess correctly/aborted corrected
                            abort();
                            // jobStatus[i] = ABORTED;
                            // sf_job_status_change(i, RUNNING, ABORTED);
                        } 
                        
                        pipelinePtr = pipelinePtr->rest;

                        continue;
                    }


                    int numOfPipes = numOfCmds - 1; 
                    int numOfFileDescriptors = numOfPipes * 2;
                    // num of file descriptors = numOfPipes * 2
                    int pipefd[numOfFileDescriptors];

                    // below code sets up pipes
                    for(int i = 0; i < numOfFileDescriptors; i+=2) {
                        pipe(pipefd+i);
                    }
                    int nextReadFD = 0;

                    while(commands) {
                        COMMAND *currentCommand = commands->first;
                        if((cpid = fork()) < 0) {
                            perror("an error occured in forking this child process");
                            abort();
                        }
                        if(cpid == 0) {
                            
                            debug("IN FORKED CHILD COMMAND: MY PGID IS: %d\n", getpgrp());
                            debug("IN FORKED CHILD COMMAND: MY PID IS: %d\n", getpid());
                            if(nextReadFD == 0) {   
                                if(input_path) {
                                    setup_input_path(input_path);
                                }
                                // ie writing to pipe 1
                                if(dup2(pipefd[1], STDOUT_FILENO) == -1) {
                                    perror("an error occured in calling dup2");
                                    abort();
                                }
                            } else if(nextReadFD == numOfFileDescriptors) {
                                // if no output path would just write to stdout
                                if(output_path) {
                                    setup_output_path(output_path);
                                }
                                if(dup2(pipefd[nextReadFD-2], STDIN_FILENO) == -1) {
                                    perror("an error occured in calling dup2");
                                    abort();
                                }
                            } else {
                                if(dup2(pipefd[nextReadFD-2], STDIN_FILENO) == -1) {
                                    perror("an error occured in calling dup2");
                                    abort();
                                }
                                if(dup2(pipefd[nextReadFD+1], STDOUT_FILENO) == -1) {
                                    perror("an error occured in calling dup2");
                                    abort();
                                }
                            }

                            closePipes(pipefd, numOfFileDescriptors);
                            // pipes all set up can now read args & execute

                            cmd_args = build_cmd_args(currentCommand);
                            // memory gets free'd on termination so don't need to do anything?
                            if(execvp(cmd_args[0], cmd_args) == -1) {
                                debug("error in running %s\n", cmd_args[0]);
                                perror("execvp failed");
                                unmalloc_cmd_args(cmd_args);
                                abort(); // Prolly need to abort here
                            };
                        }
                        // Intermediate "parent" here we increment pipe so local variable will work for next pipe

                        nextReadFD += 2;  
                        commands = commands->rest;
                    }

                    closePipes(pipefd, numOfFileDescriptors);
                    // wait for all commands to "finish pipeline"
                    int exitStatus;
                    for (i = 0; i < numOfFileDescriptors-1; i++) {
                        waitpid(-1, &status, 0);
                        // take logic here to abort or exit or whatever if pipeline fails/succeeds
                        if(WIFEXITED(status) ) {
                        // if command terminated normally, with exit() or returning from main basically
                            exitStatus = WEXITSTATUS(status); // get exit status
                            debug("pipeline exited with status %d\n", exitStatus);
                            // check if last pipeline, if so, make sure exited from runner proocess correctly/aborted corrected
                        } else {
                            debug("Child terminated abnormally\n"); // Covers below case?
                        }
                        if(WIFSIGNALED(status)) {
                            // if if was terminated by a signal
                            exitStatus = WTERMSIG(status); 
                            debug("Number of signal causing child to terminate: %d\n", exitStatus);
                            // if command aborted then entire pipeline should abort
                            abort();
                            // jobStatus[i] = ABORTED;
                            // sf_job_status_change(i, RUNNING, ABORTED);
                        } 
                    }

                    // if reached here then has not aborted so all command exited correctly
                    if(pipelinePtr->rest == NULL) {
                        // if last compipeline, then exit pipeline master process w/ exit code
                        exit(exitStatus);
                    }


                    pipelinePtr = pipelinePtr->rest;
                }

                // exit success somewhere here ?

            } else {
                // Parent, set pgid, which is equal to id of child
                sf_job_start(i, pid_runner);
                setPGID(i, pid_runner);
                // job_table[i].processGroupID = pid_runner;
            }
        }
    }
}