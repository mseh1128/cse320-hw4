#include "main_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include "jobber.h"
#include <sys/types.h>

int setJobTerminatedAfterCancel(int jobid, int flag);
int setJobStatus(int jobid, JOB_STATUS statusToSet);
int setExitStatus(int jobid, int exitStatus);
int setPGID(int jobid, pid_t pgidToSet);
PIPELINE_LIST *getJobPipelines(int jobid);
int getExitStatus(int jobid);
int incrementNumRunners();
int decrementNumRunners();
int decrementNumJobs();
int getNumRunners();
int find_job_index(int PGID);
int validJobIndex(int jobid);
int expungeJobResources(int jobid);