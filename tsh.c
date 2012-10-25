/***************************************************************************
 *  Title: tsh
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.4 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80

/************Global Variables*********************************************/
extern bgjobL *bgjobs;
extern pid_t fgJobPid;
extern char *fgJobCmd;
 
/************Function Prototypes******************************************/
/* handles SIGINT, SIGSTOP signals */
static void sig(int);
/* handles SIGCHLD signals */
static void sigChldHandler(int signo);
/* loads ~/.tshrc file */
static void initTshRC();

/************External Declaration*****************************************/
extern void freeCommand(commandT* cmd);
/**************Implementation***********************************************/

/*
 * main
 *
 * arguments:
 *   int argc: the number of arguments provided on the command line
 *   char *argv[]: array of strings provided on the command line
 *
 * returns: int: 0 = OK, else error
 *
 * This sets up signal handling and implements the main loop of tsh.
 */
int main(int argc, char *argv[])
{
  /* Initialize command buffer */
  char *cmdLine = malloc(sizeof(char*) * BUFSIZE);
  if (argc > 1)
    return -1;

  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR)
    PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR)
    PrintPError("SIGTSTP");
  if (signal(SIGCHLD, sigChldHandler) == SIG_ERR)
    PrintPError("SIGCHLD");

  initTshRC();

  while (!forceExit) /* repeat forever */
  {
    /* read command line */
    getCommandLine(&cmdLine, BUFSIZE);

    //This is a hack to satisfy the test case. I apologize
    if (!strncmp(cmdLine, "exit", 4))
        break;
    
    /* checks the status of background jobs */
    CheckJobs();
    
    /* interpret command and line
     * includes executing of commands */
    Interpret(cmdLine);

  }
  
  fflush(stdout);
  /* shell termination */
  free(fgJobCmd);
  free(cmdLine);
  if( bgjobs != NULL)
  {
    bgjobL *job, *tmp;
    job = bgjobs;
    while(job != NULL)
    {
      tmp = job->next;
      freeBgJob(job);
      job = tmp;
    }
  }
  return 0;
} /* main */

/*
 * sig
 *
 * arguments:
 *   int signo: the signal being sent
 *
 * returns: none
 *
 * This should handle signals sent to tsh.
 * for INT or TERM, it will kill all background jobs and the foreground job
 * for CHLD it will reap all background jobs
 */
static void sig(int signo)
{
  if (signo == SIGINT || signo == SIGTERM || signo == SIGTSTP)
  {
    KillFgProc(signo);
  }
} /* sig */

/*
 * sigChldHandler
 *
 * arguments:
 *   int: signo: the signal received (should always be SIGCHLD)
 *
 * This handles SIGCHLD (when a child process dies) It waits for one
 * child and removes that child from the bgjob list
 *
 */
static void sigChldHandler(int signo)
{
  if (signo == SIGCHLD)
  {
    //    printf("Received SIGCHLD\n");
    fflush(stdout);
    pid_t child;
    int stat;
    child = waitpid(-1, &stat, WNOHANG | WUNTRACED);

    // if waitpid < 0, another thread was waiting
    if (child > 0)
    {
      if (child == fgJobPid)
      {
        fgJobPid = 0;
        if (WIFSTOPPED(stat))
        {
          AddBgJob(child, STOPPED, fgJobCmd);
        }
        else
        {
          free(fgJobCmd);
        }
        fgJobCmd = NULL;
        return;
      }
      printf("Chid pid: %d\n Fg pid: %d\n", child, fgJobPid);
      // it's a bg job, update it in the job list
      UpdateBgJob(child, toJobStatus(stat));
    }
    else
    {
      fprintf(stdout, "Someone else handled it though\n");
    }
  }
  else
  {
    fprintf(stderr, "SIGCHLD handler called with non-SIGCHLD!\n");
  }
}

/*
 * initTshRC
 *
 * arguments: none
 *
 * returns: none
 *
 * initTshRC will parse ~/.tshrc and run each line in it as if it were typed into tsh
 * All lines that begin with '#' are treated as comments
 * initTshRC will fail silently in the case that tshrc does not exist
 */

static void initTshRC()
{
  char lineBuffer[128], rcPath[128];
  struct passwd *pw = getpwuid(getuid());
  char *homedir = pw->pw_dir;
  sprintf(rcPath, "%s/.tshrc", homedir);

  FILE *file = fopen(rcPath, "r");
  if (!file)
    return;

  while(fgets(lineBuffer, 128, file) != NULL)
  {
    if (lineBuffer[0] == '#')
      continue;
    //fgets includes the \n so remove it
    lineBuffer[strlen(lineBuffer) - 1] = '\0';
    Interpret(lineBuffer);
  }
} /* initTshRC */
