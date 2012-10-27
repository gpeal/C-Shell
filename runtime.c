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
 #include <errno.h>

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
char* fgJobCmd = NULL;

/* All aliased commands */
Alias *aliases = NULL;

/************Function Prototypes******************************************/
/* run command */
 static void RunCmdFork(commandT*, char*, bool);
/* runs an external program command after some checks */
 static void RunExternalCmd(commandT*, char*, bool);
/* resolves the path and checks for exutable flag */
 static void ResolveExternalCmd(commandT* cmd);
/* forks and runs a external program */
 static void Exec(commandT*, char*, bool, bool);
/* runs a builtin command */
 static void RunBuiltInCmd(commandT*);
/* turns a piped command into a reverse linked list of the individual commands */
 static commandTLinked *parsePipedCmd(commandT*);
/* Recursively runs all of the commands in a pipe */
 static void RunCmdPipeRecurse(commandTLinked* cmd, int passed_fd[]);
/* checks whether a command is a builtin command */
 static bool IsBuiltIn(char*);
/* checks whether or not a file exists */
 static int fileExists(char *path);
/* sets an environment variable using strcpy */
 static void setEnvVar(commandT* cmd);
/* adds a pid to the bg job list  */
 void AddBgJob(pid_t pid, jobStatus status, char* cmdLine);
/* removes a pid from the bg job list */
 void RmBgJobPid(pid_t pid);
/* prints a job when it's status changes */
 static void printJob(bgjobL*,int jobNum);
/* clean up the job list and print out changed statuses  */
 void CheckJobs();
/* frees a bgjobl struct  */
 void freeBgJob(bgjobL* job);
/* waits for the foreground process and reaps zombie children */
 static void waitFg(pid_t);
/* finds a job in the bg job list by pid */
 static int findBgJobPid(pid_t pid, bgjobL** job);
/* finds a job in the bg job list by job number */
 static bgjobL *findBgJobNum(int jobNum);
/************External Declaration*****************************************/
extern commandT *copyCommand(commandT* cmd);
extern void freeCommand(commandT* cmd);
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
 void RunCmd(commandT* cmd, char* cmdLine)
 {
   fflush(stdout);
   RunCmdFork(cmd, cmdLine, TRUE);
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
void RunCmdFork(commandT* cmd, char* cmdLine, bool fork)
{
  int i;
  bool pipe = FALSE;
  char *quotPtr;
  for(i = 0; cmd->argv[i] != 0; i++)
  {
    quotPtr = strrchr(cmd->argv[i], '|');
    if(quotPtr != NULL){
      pipe = TRUE;
    }
  }
  if (cmd->argc <= 0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
  {
    RunBuiltInCmd(cmd);
  }
  else if (pipe)
  {
    //make a new commandT and set its argv to the argv from cmd1 that are after the pipe
    commandTLinked *cmdt = parsePipedCmd(cmd);
    RunCmdPipe(cmdt);
    return;
  }
  else
  {
    for(i = 0; cmd->argv[i] != 0; i++)
    {

      if(strcmp(cmd->argv[i], ">") == 0)
      {
        cmd->argc = i;
        free(cmd->argv[i]);
        cmd->argv[i] = 0;
        cmd->ioRedirect = IO_REDIRECT_OUT;
        cmd->ioRedirectPath = cmd->argv[i + 1];
      }
      else if(strcmp(cmd->argv[i], "<") == 0)
      {
        cmd->argc = i;
        free(cmd->argv[i]);
        cmd->argv[i] = 0;
        cmd->ioRedirect = IO_REDIRECT_IN;
        cmd->ioRedirectPath = cmd->argv[i + 1];
      }
    }
    RunExternalCmd(cmd, cmdLine, fork);
  }
} /* RunCmdFork */


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
 void RunCmdPipe(commandTLinked* cmd)
 {
  pid_t child;
  int status = -1;
  child = fork();

   if(child >= 0) // fork was successful
   {
        if(child == 0) // child process
        {
          RunCmdPipeRecurse(cmd, NULL);
          exit(1);
        } else //Parent process
        {
          status = waitpid(child, &status, 0);
          if(status == -1){
            perror("Wait Fail");
          }
          status = WEXITSTATUS(status);
        }
      }
    else // fork failed
    {
      perror("Fork failed!\n");
    }

 } /* RunCmdPipe */

 /*
  * RunCmdPipeRecurse
  *
  * arguments:
  *   commandTLinked *cmd: the list of piped commands
  *   int fdPrevious[]: the pipe from the previous command (NULL for the first one)
  *
  * returns: none
  *
  * RunCmdPipeRecurse runs a list of piped commands and links their file descriptors
  * such that the pipes behave as you would expect.
  *
  */
static void RunCmdPipeRecurse(commandTLinked *cmd, int fdPrevious[])
{
  int fd[2];
  pid_t grandchild;

  if (cmd->next != NULL)
  {
    pipe(fd);
  }

  if ((grandchild = fork()) == 0) // grandchild process
  {
    if (fdPrevious != NULL)
    {
      dup2(fdPrevious[0], STDIN_FILENO);
      close(fdPrevious[0]);
    }
    if (cmd->next != NULL)
    {
      dup2(fd[1], STDOUT_FILENO);
      close(fd[1]);
      close(fd[0]);
    }
    execv(cmd->cmd->name, cmd->cmd->argv);
    exit(1);
  }
  else if (grandchild > 0) // child process
  {
    if (fdPrevious != NULL)
    {
      close(fdPrevious[0]);
    }
    if (cmd->next != NULL)
    {
      close(fd[1]);
      RunCmdPipeRecurse(cmd->next, fd);
    }
  }
  else
  {
    perror("Fork Error");
  }
} /* RunCmdPipeRecurse */


/*
 * parsePipedCmd
 *
 * arguments:
 *   commandT *cmd: the command to be parsed
 *
 * returns *commandTLinked: a linked list *commandT representing each piped command in reverse order (right to left)
 *
 * Takes a commandT* that has multiple piped commands and structures them into an array of individual commands
 * This also reverses the order of the commands for use by RunCmdFork
 */
 static commandTLinked *parsePipedCmd(commandT* cmd)
 {
  int i = 0;
  int i2 = 0;
  int argc = 0;
  char *argv;
  commandTLinked *pipedCmdHead;
  commandTLinked *pipedCmdCurrent = NULL;

  for (i = cmd->argc - 1; i >= 0; i--)
  {
    if (strcmp(cmd->argv[i], "|") != 0)
    {
      // increase the argc for the current command and continue to the next argv
      argc++;
      continue;
    }
    // argv[i]  "|"
    // the first time we have to set the new piped command to the head command
    if (!pipedCmdCurrent)
    {
      pipedCmdHead = malloc(sizeof(commandTLinked));
      pipedCmdCurrent = pipedCmdHead;
      pipedCmdCurrent->next = NULL;
    }
    else
    {
      // for reverse order
      // pipedCmdCurrent->next = malloc(sizeof(commandTLinked));
      // pipedCmdCurrent = pipedCmdCurrent->next;
      // for in order
      pipedCmdCurrent = malloc(sizeof(commandTLinked));
      pipedCmdCurrent->next = pipedCmdHead;
      pipedCmdHead = pipedCmdCurrent;
    }

    pipedCmdCurrent->cmd = malloc(sizeof(commandT) + sizeof(char*) * (argc + 1));
    pipedCmdCurrent->cmd->argc = argc;
    for (i2 = 0; i2 < argc; i2++)
    {
      argv = malloc(sizeof(char) * (strlen(cmd->argv[i + 1 + i2]) + 1));
      strcpy(argv, cmd->argv[i + 1 + i2]);
      pipedCmdCurrent->cmd->argv[i2] = argv;
      free(cmd->argv[i + 1 + i2]);
    }
    pipedCmdCurrent->cmd->argv[i2] = 0;
    pipedCmdCurrent->cmd->name = pipedCmdCurrent->cmd->argv[0];
    ResolveExternalCmd(pipedCmdCurrent->cmd);
    free(cmd->argv[i]);
    cmd->argv[i] = 0;
    argc = 0;
  }
  // now add the first part of the cmd
  // for reverse order
  // pipedCmdCurrent->next = malloc(sizeof(commandTLinked *));
  // pipedCmdCurrent = pipedCmdCurrent->next;
  // epipedCmdCurrent->next = NULL;
  // end for reverse order
  // for in order
  pipedCmdCurrent = malloc(sizeof(commandTLinked));
  pipedCmdCurrent->next = pipedCmdHead;
  pipedCmdHead = pipedCmdCurrent;
  // end for in order
  pipedCmdCurrent->cmd = cmd;
  pipedCmdCurrent->cmd->argc = argc;
  ResolveExternalCmd(pipedCmdCurrent->cmd);
  pipedCmdCurrent = pipedCmdHead;

  /*while (pipedCmdCurrent != NULL)
  {
    for (i = 0; i < pipedCmdCurrent->cmd->argc; i++)
    {
      printf("%s ", pipedCmdCurrent->cmd->argv[i]);
    }
    printf("\n");
    pipedCmdCurrent = pipedCmdCurrent->next;
  }*/

  return pipedCmdHead;
}


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
static void RunExternalCmd(commandT* cmd, char* cmdLine, bool fork)
{
  ResolveExternalCmd(cmd);
  Exec(cmd, cmdLine, fork, cmd->bg);
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
        cmd->name = malloc(strlen(tmp) * sizeof(char) + 1);
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
    // append ./ at the beginning
    strcpy(tmp, cmd->name);
    free(cmd->name);
    cmd->name = malloc((strlen(tmp) + 3) * sizeof(char));
    sprintf(cmd->name, "./%s", tmp);
  }
  // command could not be found. Set argc to 0 as a red flag for later
  cmd->argc = 0;
  return;
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *   bool bg: whether or not the command should be run in the background
 *
 * returns: none
 *
 * Executes a command.
 */
static void Exec(commandT* cmd, char* cmdLine, bool forceFork, bool bg)
{
  pid_t child;
  int status = -1;
  int ioRedirectFile = -1;
  sigset_t signals;

  sigemptyset(&signals);
  sigaddset(&signals, SIGCHLD);

  if (forceFork)
  {
    // block sigchld to avoid race condition between adding and
    // removing jobs from the bg job list
    sigprocmask(SIG_BLOCK, &signals, NULL);
    if ((child = fork()) > 0) // parent process
    {
      if (bg)
      {
        AddBgJob(child, RUNNING, strndup(cmdLine, strlen(cmdLine)));
        sigprocmask(SIG_UNBLOCK, &signals, NULL);
      }
      else
      {
        fgJobPid = child;
        fgJobCmd = strndup(cmdLine, strlen(cmdLine));
        sigprocmask(SIG_UNBLOCK, &signals, NULL);
        waitFg(child);
        status = WEXITSTATUS(status);
      }
    }
    else if (child == 0) // child process
    {
      // unblock sigchld
      sigprocmask(SIG_UNBLOCK, &signals, NULL);
      // set the group id to the pid so it can be killed properly
      if (setpgid(0, 0) != 0)
        PrintPError("Error setting child gid\n");
      // if the command could not be resolved, argc is set to 0
      if(cmd->argc > 0)
      {
        // set up io redirection if neccesary
        if (cmd->ioRedirect == IO_REDIRECT_OUT || cmd->ioRedirect == IO_REDIRECT_IN)
        {
          ioRedirectFile = open(cmd->ioRedirectPath,
            ((cmd->ioRedirect == IO_REDIRECT_OUT) ? O_WRONLY : O_RDONLY) | O_CREAT, 0644);
          if (ioRedirectFile == -1)
          {
            printf("Error opening file for io redirection. Error: %s\n", strerror(errno));
            fflush(stdout);
            exit(errno);
          }
          // this works because of the careful choice of the ioRedirect defines
          dup2(ioRedirectFile, cmd->ioRedirect);
        }

        status = execv(cmd->name, cmd->argv);
        if (ioRedirectFile != -1)
          close(ioRedirectFile);
      }
      else if (cmd->argc == 0)
      {
        printf("./tsh-ref: line 1: %s: No such file or directory\n", cmd->name);
      }
      exit(status);
    }
    else
    {
      PrintPError("Fork Failed\n");
    }
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
  else if (!strcmp(cmd, "jobs"))
    return TRUE;
  else if (!strcmp(cmd, "alias"))
    return TRUE;
  else if (!strcmp(cmd, "unalias"))
    return TRUE;
  else if (!strcmp(cmd, "bg"))
    return TRUE;
  else if (!strcmp(cmd, "fg"))
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
  char *from;
  char *to;
  Alias *alias;
  Alias *aliasPrevious;
  Alias *aliastmp;
  if (strchr(cmd->name, '='))
    setEnvVar(cmd);
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
      {
        int i = 1;
        for (i = 1; i < cmd->argc; i++)
        {
            if(i != cmd->argc - 1)
            {
                printf("%s ", cmd->argv[i]);
            }
            else
            {
                printf("%s\n", cmd->argv[i]);
            }
        }
      }
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
    if (getenv("PS1"))
      printf("exit\n");
    forceExit = TRUE;
    return;
  }
  else if (!strcmp(cmd->argv[0], "jobs"))
  {
    int i = 1;
    bgjobL* job_i = bgjobs;

    while(job_i != NULL)
      {
        printJob(job_i, i);
	job_i = job_i->next;
	i++;
      }
  }
  else if (!strcmp(cmd->argv[0], "alias"))
  {
    if (cmd->argc == 1)
    {
      alias = aliases;
      while(alias)
      {
        printf("%s=%s\n", alias->from, alias->to);
        alias = alias->next;
      }
      return;
    }
    tmp = cmd->argv[1];
    while (*tmp != '=' && *tmp != '\0')
      tmp++;
    // Normally you would multiply by sizeof(char) bit in the pointer arithmetic, you also divide by it so they
    // actually cancel (cool!)
    from = malloc(tmp - cmd->argv[1] + sizeof(char));
    strncpy(from, cmd->argv[1], (tmp - cmd->argv[1]) / sizeof(char));
    tmp++;
    if (*tmp == '\'')
      tmp++;
    // malloc the size of the command minus the length of the start to the beginning of the to command
    to = malloc(strlen(cmd->argv[1]) - (tmp - cmd->argv[1]) + sizeof(char));
    strncpy(to, tmp, (strlen(cmd->argv[1]) - (tmp - cmd->argv[1])) / sizeof(char));
    from[(tmp - cmd->argv[1]) / sizeof(char) - 1] = '\0';
    to[(strlen(cmd->argv[1]) - (tmp - cmd->argv[1])) / sizeof(char)] = '\0';
    alias = malloc(sizeof(Alias));
    if (aliases == NULL)
    {
      aliases = alias;
      alias->next = NULL;
    }
    else
    {
      alias->next = aliases;
      aliases = alias;
    }
    alias->from = from;
    alias->to = to;
    printf("Aliasing %s to %s\n", from, to);

  }
  else if (!strcmp(cmd->argv[0], "unalias"))
  {
    alias = aliases;
    if (alias == NULL || cmd->argc != 2)
    {
      return;
    }
    // Check the first alias
    if (strcmp(alias->to, cmd->argv[1]) == 0)
    {
      aliases = alias->next;
      free(alias);
      alias = aliases;
    }
    // if we removed the only alias, return now
    if (aliases == NULL)
    {
      return;
    }
    aliasPrevious = alias;
    alias = alias->next;
    while (alias != NULL)
    {
      if (strcmp(alias->from, cmd->argv[1]) == 0)
      {
        aliasPrevious = alias->next;
        free(alias);
        alias = aliasPrevious->next;
      }
      else
      {
        alias = alias->next;
      }
    }
  }
  else if (!strcmp(cmd->argv[0], "bg"))
  {
    int jobNum;
    bgjobL *job;

    if (cmd->argc <= 1)
      return;

    jobNum = atoi(cmd->argv[1]);
    job = findBgJobNum(jobNum);

    if (job != NULL && job->status != RUNNING)
    {
      job->status = RUNNING;
      kill(-1 * job->pid, SIGCONT);
    }
  }
  else if (!strcmp(cmd->argv[0], "fg"))
  {
    int jobNum;
    bgjobL *job;

    if (cmd->argc <= 1)
      return;

    jobNum = atoi(cmd->argv[1]);
    job = findBgJobNum(jobNum);

    if(job != NULL)
    {
      job->status = RUNNING;
      fgJobPid = job->pid;
      fgJobCmd = strndup(job->cmdLine, strlen(job->cmdLine));
      if (job->status != RUNNING)
        kill(-1 * job->pid, SIGCONT);
      RmBgJobPid(job->pid);
      waitFg(fgJobPid);
      fflush(stdout);
    }
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
    int i=1;
  bgjobL *prev, *job_i;
  prev = NULL;
  job_i = bgjobs;
  if (bgjobs == NULL)
    return;
  while (job_i != NULL)
  {
    if(job_i->changed)
      printJob(job_i, i);
    if (job_i->status == DONE)
    {
      if(job_i == bgjobs)
      {
        // remove from list and make job_i->next the head
        bgjobs =  job_i->next;
        freeBgJob(job_i);
        job_i = bgjobs;
      }
      else
      {
        // remove from list and make prev->next = job_i->next
        prev->next = job_i->next;
        freeBgJob(job_i);
        job_i = prev->next;
      }
    }
    else
    {
      prev = job_i;
      job_i = job_i->next;
    }
    i++;
  }
} /* CheckJobs */

/*
 * KillFgProc
 *
 * arguments: int signo: the signal to forward
 *
 * returns: none
 *
 * Sends signal to the fg process if there is one
 */
int KillFgProc(int signo)
{
  int ret;
  if (!fgJobPid)
    return -1;

  ret = kill(-1 * fgJobPid, signo);

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

/*
 * AddBgJob
 *
 * arguments pid_t pid: pid of bg process
 *           jobStatus status: status of the process
 *           commandT* command: the command struct from the call for this job
 *           
 * returns: none
 * 
 * This function adds the pid to the end of the bg job list
 * cmd will be freed when the job is freed so don't free it anywhere else!
 *
 */
void AddBgJob(pid_t pid, jobStatus status, char* cmdLine)
{
  bgjobL* new_bgjob = malloc(sizeof(bgjobL));
  int i = 1;
  new_bgjob->pid = pid;
  new_bgjob->status = status;
  new_bgjob->cmdLine = cmdLine;
  new_bgjob->changed = FALSE;
  new_bgjob->next = NULL;
  if (bgjobs == NULL)
  {
    // first bg job
    bgjobs = new_bgjob;
  }
  else
  {
    i++;
    // add to end of list
    bgjobL* job_i = bgjobs;
    while(job_i->next != NULL)
    {
      job_i = job_i->next;
      i++;
    }  
    job_i->next = new_bgjob;
  }
  if (new_bgjob->status == STOPPED)
  {
    printf("\n");
    printJob(new_bgjob, i);
  }
}

/*
 * UpdateBgJob
 *
 * arguments pid_t pid: pid of process that has completed
 * 
 * returns: none
 * 
 * This function updates a bg job in the bg job list by pid
 *
 */
void UpdateBgJob(pid_t pid, jobStatus status)
{
  bgjobL* job;
  int jobNum;

  jobNum = findBgJobPid(pid, &job);

  if (job != NULL)
  {
    job->status = status;
    job->changed = TRUE;
  }
  else
    fprintf(stderr, "job not found");
}

/*
 * printJob
 */
static void printJob(bgjobL* bgjob, int jobNum)
{
  printf("[%x]   ", jobNum);
  switch(bgjob->status)
  {
    case DONE:
      printf("Done");
      break;
    case STOPPED:
      printf("Stopped");
      break;
    case RUNNING:
      printf("Running");
      break;
    default:
      printf("WTF");
  }
  bgjob->changed = FALSE;
  printf("\t\t");
  int len = strlen(bgjob->cmdLine);
  if(bgjob->status == DONE)
  {
    char* printcmd = calloc(len - 1, 1);
    strncpy(printcmd, bgjob->cmdLine, len - 1);
    printf("%s ", printcmd);
    free(printcmd);
  }
  else 
  {
    printf("%s ", bgjob->cmdLine);
  }
  printf("\n");
}

/*
 * freeBgJob
 *
 * frees the job struct
 */
void freeBgJob(bgjobL* job)
{
  free(job->cmdLine);
  free(job);
}

/*
 * waitFg
 *
 * arguments: pid_t child, the pid of the child proc
 *
 * returns: 
 *
 * Waits for fg process to revert control.
 * Depends on  sigChldHandler handling job control
 */
static void waitFg(pid_t child)
{
  while (fgJobPid == child)
  {
    sleep(0);
  }
}

/*
 * toJobStatus
 */
jobStatus toJobStatus(int status)
{
  if (WIFEXITED(status)) {
    return DONE;
  }
  if (WIFSTOPPED(status)) {
    return STOPPED;
  }
  return DONE;
}

/*
 * findBgJobPid
 *
 * returns the jobNumber, sets (*bgjobL) to NULL if job not found
 */ 
int findBgJobPid(pid_t pid, bgjobL** job)
{
  int i = 0;
  *job = bgjobs;
  while((*job) != NULL && (*job)->pid != pid)
  {
    (*job) = (*job)->next;
    i++;
  }
  return i;
}
/*
 * findBgJobNum
 */
static bgjobL *findBgJobNum(int jobNum)
{
  int i = 1;
  bgjobL *job = bgjobs;
  
  while (job != NULL && i <= jobNum)
  {
    if (jobNum == i)
      return job;
    job = job->next;
    i++;
  }
  return NULL;
}

void RmBgJobPid(pid_t pid)
{
  bgjobL *job, *prev;
  findBgJobPid(pid, &job);

  prev = NULL;
  job = bgjobs;

  if (bgjobs == NULL)
    return;

  while (job != NULL)
  {
    if (job->pid == pid)
    {
      if (job == bgjobs)
      {
        // first job in the list
        bgjobs = job->next;
      }
      else
      {
        // not the first element of the list
        prev->next = job->next;
      }
      freeBgJob(job);
      return;
    }
    prev = job;
    job = job->next;
  }
}
