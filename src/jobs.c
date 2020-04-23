/*
 * Job manager for "jobber".
 */

// #include <stdlib.h>

#include "jobs_table.h"
// #include "jobber.h"
// #include "task.h"


#include <string.h>
#include <signal.h>
// #include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>	

static int isEnabled; // 0 = not enabled, 1 = enabled

static int findFreeJobIndex();

int numOfRunners;
int numOfFilledJobSlots;

int jobs_init(void) {
    // TO BE IMPLEMENTED
    // no way for this to fail rn
    numOfFilledJobSlots = 0;
    isEnabled = 0;
    numOfRunners = 0;
    return 0;
}

// Returns -1 if no job found
// (Should never occur since numOfSlots can be checked first)
static int findFreeJobIndex() {
    for(int i = 0; i < MAX_JOBS; i++) {
        if(!job_table[i].job) return i;
    }
    return -1;
}

/*
 * @brief  Finalize the job spooler.
 * @details  This function must be called once when job processing is to be terminated.
 * It cancels any remaining jobs, waits for them to terminate, expunges all jobs,
 * and frees any other memory or resources before returning.
 */


void jobs_fini(void) {
    for(int i = 0; i < MAX_JOBS; i++){
        if(job_table[i].job != NULL) {
            job_cancel(i);
            debug("Status is %d", job_get_status(i));
            // if(job_table[i].job != NULL && (job_get_status(i) != COMPLETED || job_get_status(i) != ABORTED)) {
            //     // ie it requires the forced handler function!
            //     hookHandler();
            // }
            JOB_STATUS jobStatus = job_get_status(i);
            while(jobStatus != COMPLETED && jobStatus != ABORTED) {
                debug("Not yet finished");
                debug("Job status is %s", job_status_names[job_get_status(i)]);
                hookHandler();
                jobStatus = job_get_status(i);
            }
            job_expunge(i);
            // FREE ANY OTHER MEMORY OR RESOURCE LEAKS HERE!
        }   
    }
}

int jobs_set_enabled(int val) {
    int initialEnabledVal = isEnabled;
    isEnabled = val;
    run_spooled_jobs(isEnabled);
    return initialEnabledVal;
}

int jobs_get_enabled() {
    // TO BE IMPLEMENTED
    return isEnabled;
}

int job_create(char *command) {
    // TO BE IMPLEMENTED
    if(numOfFilledJobSlots >= MAX_JOBS) {
        return -1;
    }
    // need to store command somewhere
    int commandLen = strlen(command);
    char *dup_cmd = malloc(commandLen+1);
    if(!dup_cmd) {
        // ie if malloc failed
        return -1;
    }
    strcpy(dup_cmd, command);
    dup_cmd[commandLen] = '\0';

    char **cmdPointer = &command;
    TASK *new_task = parse_task(cmdPointer);
    if(!new_task) {
        // NULL if invalid (ie cmd string not well-formed)
        free(dup_cmd);
        return -1;
    }

    // printf("TASK: %s\n", dup_cmd);
    
    int freeJobIndex = findFreeJobIndex();
    job_table[freeJobIndex].job = dup_cmd;
    job_table[freeJobIndex].task = new_task;
    job_table[freeJobIndex].jobStatus = NEW;
    sf_job_create(freeJobIndex);
    job_table[freeJobIndex].jobStatus = WAITING;
    sf_job_status_change(freeJobIndex, NEW, WAITING);
    numOfFilledJobSlots++;

    run_spooled_jobs(isEnabled);
    
    return freeJobIndex;
}

/*
 * @brief  Expunge a terminated job.
 * @details  This function takes a job ID as a parameter and, if that job is in the
 * COMPLETED or ABORTED state, then it is removed from the job table and all resources
 * (e.g. memory) held by that job are freed.
 *
 * @param jobid  An integer in the range [0, MAX_JOBS).
 * @return 0  If the job exists, was in the COMPLETED or ABORTED state, and was
 * successfully expunged, and -1 otherwise.
 */
int job_expunge(int jobid) {
    if(!validJobIndex(jobid)) return -1;
    if(job_table[jobid].job != NULL) {
        JOB_STATUS jobStatus = (job_table[jobid].jobStatus);
        if(jobStatus == COMPLETED || jobStatus == ABORTED) {
            // remove from table & expunge all its resources
            expungeJobResources(jobid);
            sf_job_expunge(jobid);
            decrementNumJobs();
            return 0;
        }
        return -1;
    }
    return -1;
}

/*
 * @brief  Attempt to cancel a job.
 * @details  This function takes a job ID as a parameter and, if that job is in the
 * WAITING, RUNNING, or PAUSED state, then it is set to the CANCELED state.
 * In addition, if the job was in the RUNNING or PAUSED state, then a SIGKILL signal
 * is sent to the process group of the job's runner process in order to forcibly
 * terminate it.  If the job was in the WAITING state then it does not have a runner,
 * so no signal is sent in that case.
 *
 * Note that this function must appropriately mask signals in order to avoid a race
 * between the querying of the job status and the subsequent setting of the status
 * to CANCELED and the sending of the SIGKILL signal.
 *
 * @param jobid  An integer in the range [0, MAX_JOBS).
 * @return 0  If the job exists, was in the WAITING, RUNNING, or PAUSED state,
 * and, if the job was in the RUNNING or PAUSED state, then the SIGKILL signal was
 * successfully sent.  Otherwise, -1 is returned.
 */
int job_cancel(int jobid) {
    // TO BE IMPLEMENTED
    if(!validJobIndex(jobid)) return -1;
     if(job_table[jobid].job != NULL) {
         // block signals here
         sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);

        sigprocmask(SIG_BLOCK, &set, NULL);

        JOB_STATUS jobStatus = job_table[jobid].jobStatus;

        sigprocmask(SIG_UNBLOCK, &set, NULL);
        if(jobStatus == WAITING || jobStatus == RUNNING || jobStatus == PAUSED) {
            if(jobStatus == WAITING) {
                // not started yet so can just change status 
                JOB_STATUS oldStatus = job_get_status(jobid);
                setJobStatus(jobid, ABORTED);
                sf_job_status_change(jobid, oldStatus, ABORTED);
                return 0;
            } else {
                JOB_STATUS oldStatus = job_get_status(jobid);
                setJobStatus(jobid, CANCELED);
                sf_job_status_change(jobid, oldStatus, CANCELED);
                if(killpg(job_get_pgid(jobid), SIGKILL) == -1){
                    perror("Could not cancel this job!");
                    return -1;
                } 
                return 0;
                // already started so need to set to cancel, & send sigkill to runner's pg group
            }
            // set to pause state & send ISGSTOP to process group
            // sf_job_status_change(jobid, jobStatus, CANCELED);
            // setJobStatus(jobid, CANCELED);

            return 0;
        }
        return -1;
    }
    return -1;
}

/*
 * @brief  Pause a running job.
 * @details  This function takes a job ID as a parameter and, if that job is in the
 * RUNNING state, then it is set to the PAUSED state and a SIGSTOP signal is sent
 * to the process group of the job's runner process in order to cause the job to
 * pause execution.
 *
 * Note that this function must appropriately mask signals in order to avoid a race
 * between the querying of the job status and the subsequent setting of the status
 * to PAUSED and the sending of the SIGSTOP signal.
 *
 * @param jobid  An integer in the range [0, MAX_JOBS).
 * @return 0  If the job exists, was in the RUNNING state, and the SIGSTOP signal
 * was successfully sent.  Otherwise, -1 is returned.
 */
int job_pause(int jobid) {
     if(!validJobIndex(jobid)) return -1;
     if(job_table[jobid].job != NULL) {
         // block signals here
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);

        sigprocmask(SIG_BLOCK, &set, NULL);

        JOB_STATUS jobStatus = job_table[jobid].jobStatus;

        sigprocmask(SIG_UNBLOCK, &set, NULL);
        if(jobStatus == RUNNING) {
            // set to pause state & send ISGSTOP to process group
            setJobStatus(jobid, PAUSED);
            sf_job_status_change(jobid, RUNNING, PAUSED);
            debug("PROCESS GROUP ID IS %d", job_table[jobid].processGroupID);
            if(killpg(job_table[jobid].processGroupID, SIGSTOP) == -1){
                perror("Could not pause this job!");
                return -1;
            } else {
                return 0;
            }
        }
 
        // unlock signals here
        return -1;
    }
    return -1;
}

/*
 * @brief  Resume a paused job.
 * @details  This function takes a job ID as a parameter and, if that job is in the
 * PAUSED state, then it is set to the RUNNING state and a SIGCONT signal is sent
 * to the process group of the job's runner process in order to cause the job to
 * continue execution.
 *
 * Note that this function must appropriately mask signals in order to avoid a race
 * between the querying of the job status and the subsequent setting of the status
 * to RUNNING and the sending of the SIGCONT signal.
 *
 * @param jobid  An integer in the range [0, MAX_JOBS).
 * @return 0  If the job exists, was in the PAUSED state, and the SIGCONT signal
 * was successfully sent.  Otherwise, -1 is returned.
 */
int job_resume(int jobid) {
    if(!validJobIndex(jobid)) return -1;
     if(job_table[jobid].job != NULL) {
         sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);

        sigprocmask(SIG_BLOCK, &set, NULL);

        JOB_STATUS jobStatus = job_table[jobid].jobStatus;

        sigprocmask(SIG_UNBLOCK, &set, NULL);
        if(jobStatus == PAUSED) {
            // set to pause state & send ISGSTOP to process group
            setJobStatus(jobid, RUNNING);
            sf_job_status_change(jobid, PAUSED, RUNNING);
            debug("PROCESS GROUP ID IS %d", job_table[jobid].processGroupID);
            if(killpg(job_table[jobid].processGroupID, SIGCONT) == -1){
                perror("Could not resume this job!");
                return -1;
            } else {
                return 0;
            }
        }
        // unlock signals here
        return -1;
    }
    return -1;
}

int job_get_pgid(int jobid) {
    if(!validJobIndex(jobid)) return -1;
    debug("GETTING PGID FOR %d", jobid);
    if(job_table[jobid].job != NULL) {
        JOB_STATUS jobStatus = (job_table[jobid].jobStatus);
        if(jobStatus == RUNNING || jobStatus == PAUSED || jobStatus == CANCELED) {
            return job_table[jobid].processGroupID;
        }
        return -1;
    }
    return -1;
}

JOB_STATUS job_get_status(int jobid) {
    if(!validJobIndex(jobid)) return -1;
    if(job_table[jobid].job != NULL) {
        return job_table[jobid].jobStatus;
    }
    return -1;
}


/*
 * @brief  Get the result (i.e. the exit status) of a job.
 * @return  If the specified job ID is the ID of an existing job that is in the
 * COMPLETED state, then the exit status of the job's runner process is returned,
 * otherwise -1 is returned.  The exit status should be exactly as would be
 * returned by the waitpid() function.
 */
int job_get_result(int jobid) {
    if(!validJobIndex(jobid)) return -1;
    if(job_table[jobid].job != NULL) {
        if(job_table[jobid].jobStatus == COMPLETED) {
            return job_table[jobid].exitStatus; 
        }
        return -1;
    }
    return -1;
}


/*
 * @brief  Determine if a job was successfully canceled.
 * @return If the specified job ID is the ID of an existing job that is in the
 * ABORTED state, the job was explicitly canceled by the user, and the job was
 * terminated by a SIGKILL signal at some point after having been set to the
 * CANCELED state, then 1 is returned, otherwise 0 is returned.
 */
int job_was_canceled(int jobid) {
    if(!validJobIndex(jobid)) return -1;
    struct job *retrievedJob = &job_table[jobid];
    if(retrievedJob->job != NULL) {
        if((job_get_status(jobid) == ABORTED) && (retrievedJob->jobTerminatedAfterCancel == 1)) {
            debug("IM IN BOIS");
            // do stuff here
            return 1; 
        }
        return 0;
    }
    return 0;
}


/*
 * @brief  Get the task specification of a job.
 * @return  If the specified job ID is the ID of an existing job, then the task
 * specification string that was passed when the job was created is returned,
 * otherwise NULL is returned.  The specification string will remain valid until
 * the job has been expunged, after which the string should no longer be used.
 */
char *job_get_taskspec(int jobid) {
    if(!validJobIndex(jobid)) return NULL;
    // debug("GET TASKSPEC, job is %s",job_table[jobid].job);
    if(job_table[jobid].job != NULL) {
        return job_table[jobid].job;
    }
    return NULL;
}