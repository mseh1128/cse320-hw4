#include <signal.h>
#include <stdio.h>
#include "task.h"

// NEED TO BE EXTERN?
char *help_text;
volatile sig_atomic_t sigChildFlag;

// To get first word from inputted string, need to free!
char *getFirstWord(char* userInput);
// To get task from spool command, need to free!
char *getTaskStr(char* userInput);
char *getSecondWord(char* userInput);
void printJobs();
void printJobStatus(int jobid);
int getNumOfArgs(char *userInput);
int digits_only(char *userInput);
int hookHandler(void);
int num_of_commands(COMMAND_LIST *current_cmd);
int get_num_of_commands(COMMAND_LIST *current_cmd);
int get_num_of_words(WORD_LIST *init_word);
char** build_cmd_args(COMMAND *init_cmd);
void unmalloc_cmd_args(char **cmd_args);
void setup_output_path(char *output_path);
void setup_input_path(char *input_path);
void closePipes(int pipes[], int pipeSize);
void handle_sigchld(int sig);
void run_spooled_jobs();