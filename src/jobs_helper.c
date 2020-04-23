#include "jobs_table.h"

struct job job_table[8] = {NULL};

// valid = 0, invalid = -1
int validJobIndex(int jobid) {
    return (jobid >= 0 && jobid < MAX_JOBS);
}


int setJobTerminatedAfterCancel(int jobid, int flag) {
    if(!validJobIndex(jobid)) return -1;
    if(job_table[jobid].job != NULL) {
        job_table[jobid].jobTerminatedAfterCancel = flag;
        return 0;
    }
    return -1;
}

int setJobStatus(int jobid, JOB_STATUS statusToSet) {
    if(!validJobIndex(jobid)) return -1;
    if(job_table[jobid].job != NULL) {
        job_table[jobid].jobStatus = statusToSet;
        return 0;
    }
    return -1;
}

int setPGID(int jobid, pid_t pgidToSet) {
    if(!validJobIndex(jobid)) return -1;
    if(job_table[jobid].job != NULL) {
        job_table[jobid].processGroupID = pgidToSet;
        return 0;
    }
    return -1;
}

int setExitStatus(int jobid, int exitStatus) {
    if(!validJobIndex(jobid)) return -1;
    if(job_table[jobid].job != NULL) {
        job_table[jobid].exitStatus = exitStatus;
        return 0;
    }
    return -1;
}

int getExitStatus(int jobid) {
    if(!validJobIndex(jobid)) return -1;
    // should only be able to get if job status is completed!
    if(job_table[jobid].job != NULL && job_get_status(jobid) == COMPLETED) {
        return job_table[jobid].exitStatus;
    }
    debug("Cannot get exit status of not COMPLETED job");
    return -1;
}


int find_job_index(int PGID) {
    for(int i = 0; i < MAX_JOBS; i++) {
        debug("Finding pgid for %d", i);
        int job_pgid = job_get_pgid(i);
        if(job_pgid != -1 && job_pgid == PGID) {
            return i;
        }
    }
    return -1;
}


int expungeJobResources(int jobid) {
    debug("Expunging job resources");
    if(!validJobIndex(jobid)) return -1;
    struct job *jobToExpunge = &job_table[jobid];
    if(jobToExpunge->job != NULL) {
        free(jobToExpunge->job);
        free_task(jobToExpunge->task);
        jobToExpunge->job = NULL;
        jobToExpunge->task = NULL;
        // jobToExpunge->jobStatus = NULL;
        jobToExpunge->processGroupID = -1;
        jobToExpunge->exitStatus = -1;
        jobToExpunge->jobTerminatedAfterCancel = -1;
        return 0;
    }
    return -1;
}
// struct job {
//     char *job;
//     TASK *task;
//     JOB_STATUS jobStatus;
//     pid_t processGroupID;
//     int exitStatus;
// } job;


PIPELINE_LIST *getJobPipelines(int jobid) {
    if(!validJobIndex(jobid)) return NULL;
    if(job_table[jobid].job != NULL) {
        return job_table[jobid].task->pipelines;
    }
    return NULL;
}

int incrementNumRunners() {
    // only works if # of runners < 4
    if(numOfRunners < MAX_RUNNERS) {
        numOfRunners++;
        return 0;
    } else{
        debug("Only %d runners are allowed at one time.", MAX_RUNNERS);
        return -1;
    }
}

int decrementNumRunners() {
    // only works if # of runners > 0
    if(numOfRunners > 0) {
        numOfRunners--;
        return 0;
    } else{
        debug("Cannot have less than 0 runners.");
        return -1;
    }
}

int decrementNumJobs() {
    // only works if # of job slots > 0
    if(numOfFilledJobSlots > 0) {
        numOfFilledJobSlots--;
        return 0;
    } else{
        debug("Cannot have less than 0 jobs.");
        return -1;
    }

}

int getNumRunners() {
    return numOfRunners;
}

