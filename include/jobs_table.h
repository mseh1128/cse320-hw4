#include "jobs_helper.h"
// #include "jobs.h"


extern int numOfRunners;
extern int numOfFilledJobSlots;

#ifndef STRUCT_DEF
#define STRUCT_DEF
struct job {
    char *job;
    TASK *task;
    JOB_STATUS jobStatus;
    pid_t processGroupID;
    int exitStatus;
    int jobTerminatedAfterCancel;
} job;
extern struct job job_table[8];
#endif