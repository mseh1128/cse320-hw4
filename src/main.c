#include <stdlib.h>
#include <errno.h>

#include <string.h>
#include "main_helper.h"
#include "jobber.h"

/*
 * "Jobber" job spooler.
 */

int main(int argc, char *argv[])
{
    // char *task = "echo start > out; echo done";
    // job_create(task);
    // TO BE IMPLEMENTED
    signal_hook_func_t* signalHookFunction = hookHandler;
    sf_set_readline_signal_hook(*signalHookFunction);
    jobs_init();
    int continuePrompt = 1;
    while(continuePrompt) {
        char *prompt = "jobber> ";
        char *userInput = sf_readline(prompt);
         if(userInput == NULL) {
            jobs_fini();
            debug("EXITING WITH SUCCESS");
            exit(EXIT_SUCCESS);
        } 
        // need to free this eventually
        char *command = getFirstWord(userInput); 

        if(userInput == NULL) {
            // do i even need to do anything? call the fini method?
            // no make input
            // exit(EXIT_SUCCESS);
        } else if(*userInput == '\0')  {

        } else if(strcmp("help",command) == 0) {
            printf("%s", help_text);
        } else if(strcmp("quit",command) == 0) {
            jobs_fini();
            debug("EXITING WITH SUCCESS");
            exit(EXIT_SUCCESS);
        } else if(strcmp("status",command) == 0) {
            int numOfArgs = getNumOfArgs(userInput);  
            if(numOfArgs != 1) {
                printf("Wrong number of args (given: %d, required: 1) for command \'status\'\n", numOfArgs);
            } else {
                char *secondWord = getSecondWord(userInput);
                if(digits_only(secondWord)) {
                    int jobIndex = atoi(secondWord);
                    debug("JOB INDEX IS %d", jobIndex);
                    printJobStatus(jobIndex);
                }
                free(secondWord);  
            }
        } else if(strcmp("jobs",command) == 0) {
            printJobs();
        } else if(strcmp("enable",command) == 0) {
            // start any spooled job
            jobs_set_enabled(1);                
        } else if(strcmp("disable",command) == 0) {
            jobs_set_enabled(0);    
        } else if(strcmp("spool",command) == 0) {
            // creates a new job
            // ex: spool 'echo start ; cat /etc/passwd | grep bash > out ; echo done'
            // need to call job_create w/ extracted task
            char* task_name = getTaskStr(userInput);
            if(task_name == NULL) {
                printf("No pair of single quotes found! Invalid!\n");
                continue;
            }
            job_create(task_name);
            // if enable == true, then actually start the task


        } else if(strcmp("pause",command) == 0) {
            int numOfArgs = getNumOfArgs(userInput);  
            if(numOfArgs != 1) {
                printf("Wrong number of args (given: %d, required: 1) for command \'pause\'\n", numOfArgs);
            } else {
                char *secondWord = getSecondWord(userInput);
                if(digits_only(secondWord)) {
                    int jobIndex = atoi(secondWord);
                    if(job_pause(jobIndex) == -1) {
                        debug("Something went wrong pausing the job\n");
                        printf("Error: pause\n");
                    }
                } else {
                    printf("Error: pause\n"); // ie non digits were located here!
                }
                free(secondWord);
                
            }
            
        }  else if(strcmp("resume",command) == 0) {
            int numOfArgs = getNumOfArgs(userInput);  
            if(numOfArgs != 1) {
                printf("Wrong number of args (given: %d, required: 1) for command \'resume\'\n", numOfArgs);
            } else {
                char *secondWord = getSecondWord(userInput);
                if(digits_only(secondWord)) {
                    int jobIndex = atoi(secondWord);
                    if(job_resume(jobIndex) == -1) {
                        debug("Something went wrong resuming the job\n");
                        printf("Error: resume\n");
                    }
                } else {
                    printf("Error: resume\n"); // ie non digits were located here!
                }
                free(secondWord);
                
            }
            
        } else if(strcmp("cancel",command) == 0) {
            int numOfArgs = getNumOfArgs(userInput);  
            if(numOfArgs != 1) {
                printf("Wrong number of args (given: %d, required: 1) for command \'cancel\'\n", numOfArgs);
            } else {
                char *secondWord = getSecondWord(userInput);
                if(digits_only(secondWord)) {
                    int jobIndex = atoi(secondWord);
                    if(job_cancel(jobIndex) == -1) {
                        debug("Something went wrong cancelling the job\n");
                        printf("Error: cancel\n");
                    }
                } else {
                    printf("Error: cancel\n"); // ie non digits were located here!
                }
                free(secondWord);
            }
        } else if(strcmp("expunge",command) == 0) {
            int numOfArgs = getNumOfArgs(userInput);  
            if(numOfArgs != 1) {
                printf("Wrong number of args (given: %d, required: 1) for command \'expunge\'\n", numOfArgs);
            } else {
                char *secondWord = getSecondWord(userInput);
                if(digits_only(secondWord)) {
                    int jobIndex = atoi(secondWord);
                    if(job_expunge(jobIndex) == -1) {
                        debug("Something went wrong expunging the job\n");
                        printf("Error: expunge\n");
                    }
                } else {
                    printf("Error: expunge\n"); // ie non digits were located here!
                }
                free(secondWord);
            }
        } else {
            // invalid input
            printf("Unrecognized command: %s\n", command);
        }
        free(userInput);
        free(command);
    }

    // debug("EXITING WITH FAILURE");
    // exit(EXIT_FAILURE);
    jobs_fini();
    debug("EXITING WITH SUCCESS");
    exit(EXIT_SUCCESS);
}


/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
