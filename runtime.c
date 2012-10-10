/***************************************************************************
 *  Title: Runtime environment
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

/* the pids of the background processes */
bgjobL *bgjobs = NULL;

int fgJobPid = 0;

/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static void ResolveExternalCmd(commandT* cmd);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
/* checks whether or not a file exists */
static int fileExists(char *path);
/* sets an environment variable using strcpy */
static void setEnvVar(commandT* cmd);
/************External Declaration*****************************************/

/**************Implementation***********************************************/


/*
 * RunCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs the given command.
 */
void RunCmd(commandT* cmd)
{
  fflush(stdout);
  RunCmdFork(cmd, TRUE);
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0].
 */
void RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc <= 0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
  {
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
} /* RunCmdFork */


/*
 * RunCmdBg
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a command in the background.
 */
void RunCmdBg(commandT* cmd)
{
  // TODO
} /* RunCmdBg */


/*
 * RunCmdPipe
 *
 * arguments:
 *   commandT *cmd1: the commandT struct for the left hand side of the pipe
 *   commandT *cmd2: the commandT struct for the right hand side of the pipe
 *
 * returns: none
 *
 * Runs two commands, redirecting standard output from the first to
 * standard input on the second.
 */
void RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
} /* RunCmdPipe */


/*
 * RunCmdRedirOut
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard output
 *
 * returns: none
 *
 * Runs a command, redirecting standard output to a file.
 */
void RunCmdRedirOut(commandT* cmd, char* file)
{
} /* RunCmdRedirOut */


/*
 * RunCmdRedirIn
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard input
 *
 * returns: none
 *
 * Runs a command, redirecting a file to standard input.
 */
void RunCmdRedirIn(commandT* cmd, char* file)
{
}  /* RunCmdRedirIn */


/*
 * RunExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Tries to run an external command.
 */
static void RunExternalCmd(commandT* cmd, bool fork)
{
  ResolveExternalCmd(cmd);
  Exec(cmd, fork);
}  /* RunExternalCmd */


/*
 * ResolveExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Determines whether the command to be run actually exists.
 * If the command does not exist, cmd->name and cmd->argv[0] are set to NULL
 */
static void ResolveExternalCmd(commandT* cmd)
{
  int exists = fileExists(cmd->name);
  int i = 0, pathLength = 0;
  char *envPath;
  char tmp[128];

  if (exists && strchr(cmd->name, '/'))
  {
    return;
  }
  else if (exists)
  {
    strcpy(tmp, cmd->name);
    free(cmd->name);
    cmd->name = malloc((strlen(tmp) + 2) * sizeof(char));
    sprintf(cmd->name, "./%s", cmd->name);
    return;
  }
  else //check path
  {
    envPath = getenv("PATH");
    while(envPath[i]) {
      while(envPath[++i] && envPath[i] != ':')
        pathLength++;
      strncpy(tmp, envPath + i - pathLength, pathLength);
      tmp[pathLength] = '\0';

      if (tmp[pathLength - 1] == '/')
        sprintf(tmp, "%s%s", tmp, cmd->name);
      else
        sprintf(tmp, "%s/%s", tmp, cmd->name);
      if (fileExists(tmp)) {
        cmd->name = malloc(strlen(tmp) * sizeof(char));
        strcpy(cmd->name, tmp);
        return;
      }
      pathLength = 0;
      //finished checking all paths (reached end of string)
      if (!envPath[i-1])
        break;
    }
  }

  if (cmd->name[0] != '.' && cmd->name[1] != '/')
  {
    //append ./ at the beginning
    strcpy(tmp, cmd->name);
    free(cmd->name);
    cmd->name = malloc((strlen(tmp) + 2) * sizeof(char));
    sprintf(cmd->name, "./%s", tmp);
  }
  cmd->argc = 0;
  return;
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *
 * returns: none
 *
 * Executes a command.
 */
static void Exec(commandT* cmd, bool forceFork)
{
  pid_t child;
  int status = -1;

  if (forceFork)
  {
      child = fork();
      if (child >= 0)
      {
        if (child > 0) //parent process
        {
          fgJobPid = child;
          waitpid(child, &status, 0);
          status = WEXITSTATUS(status);
        }
        else //child process
        {
          if (setpgid(0, 0) != 0)
            PrintPError("Error setting child gid\n");
          if(cmd->argc > 0)
            status = execv(cmd->name, cmd->argv);
          else if (cmd->argc == 0)
            printf("./tsh-ref: line 1: %s: No such file or directory\n", cmd->name);
          exit(status);
        }
    }
    else
      PrintPError("Fork Failed\n");
  }
  else // no fork
  {
    status = execv(cmd->name, cmd->argv);
  }
} /* Exec */


/*
 * IsBuiltIn
 *
 * arguments:
 *   char *cmd: a command string (e.g. the first token of the command line)
 *
 * returns: bool: TRUE if the command string corresponds to a built-in
 *                command, else FALSE.
 *
 * Checks whether the given string corresponds to a supported built-in
 * command.
 */
static bool IsBuiltIn(char* cmd)
{
  if (strchr(cmd, '='))
    return TRUE;
  else if (!strcmp(cmd, "echo"))
    return TRUE;
  else if (!strcmp(cmd, "cd"))
    return TRUE;
  else if (!strcmp(cmd, "exit"))
    return TRUE;
  return FALSE;
} /* IsBuiltIn */


/*
 * RunBuiltInCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a built-in command.
 */
static void RunBuiltInCmd(commandT* cmd)
{
  char *tmp;
  if (strchr(cmd->name, '='))
  {
    if (cmd->name[0] == 'P' && cmd->name[1] == 'S' && cmd->name[2] == '1' && cmd->name[3] == '=')
      cmd->name[2] = 'X';
    setEnvVar(cmd);
  }
  else if (!strcmp(cmd->argv[0], "echo"))
  {
    if (cmd->argc > 1)
    {
      if (cmd->argv[1][0] == '$')
      {
        tmp = getenv(cmd->argv[1] + sizeof(char));
        if (tmp)
          printf("%s\n", tmp);
        else
          printf("Environment variable %s is not set.\n", cmd->argv[1] + sizeof(char));
      }
      else
        printf("%s\n", cmd->argv[1]);
    }
  }
  else if (!strcmp(cmd->argv[0], "cd"))
  {
    if (cmd->argc > 1)
    {
      if(chdir(cmd->argv[1]))
        PrintPError("Error Changing Directories\n");
    }
  }
  else if (!strcmp(cmd->argv[0], "exit"))
  {
    if (getenv("PSX"))
      printf("exit\n");
    forceExit = TRUE;
    return;
  }
} /* RunBuiltInCmd */

static void setEnvVar(commandT* cmd) {
  char *str = malloc(64 * sizeof(char));
  strcpy(str, cmd->argv[0]);
  putenv(str);
}


/*
 * CheckJobs
 *
 * arguments: none
 *
 * returns: none
 *
 * Checks the status of running jobs.
 */
void CheckJobs()
{
} /* CheckJobs */

/*
 * StopFgProc
 *
 * arguments: none
 *
 * returns: none
 *
 * Kills the fg process if there is one
 */
int StopFgProc()
{
  int ret;
  if (!fgJobPid)
    return -1;

  ret = kill(-1 * fgJobPid, SIGINT);
  if (ret == 0)
    fgJobPid = 0;

  return ret;
}

/*
 * fileExists
 *
 * arguments char *path: path to a file
 *
 * returns: 1 if exists 0 if doesn't
 *
 * This function returns whether or not a file exists
 */
static int fileExists(char *path)
{
  FILE *file = fopen(path, "r");
  if (file)
  {
    fclose(file);
    return 1;
  }
  else
    return 0;
}