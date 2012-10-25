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

/************Function Prototypes******************************************/
/* run command */
 static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
 static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
 static void ResolveExternalCmd(commandT* cmd);
/* forks and runs a external program */
 static void Exec(commandT*, bool, bool);
/* runs a builtin command */
 static void RunBuiltInCmd(commandT*);
 /* turns a piped command into a reverse linked list of the individual commands */
 static commandTLinked *parsePipedCmd(commandT*
  );
/* checks whether a command is a builtin command */
 static bool IsBuiltIn(char*);
/* checks whether or not a file exists */
 static int fileExists(char *path);
/* sets an environment variable using strcpy */
 static void setEnvVar(commandT* cmd);
/* adds a pid to the bg job list  */
 static void AddBgJob(pid_t pid);
/* removes a pid from the bg job list */
 static void RmBgJob(pid_t pid);
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
  int i;
  commandT *cmd2;
  if (cmd->argc <= 0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
  {
    RunBuiltInCmd(cmd);
  }
  else
  {
    for(i = 0; cmd->argv[i] != 0; i++)
    {
      if (strcmp(cmd->argv[i], "|") == 0)
      {
        //make a new commandT and set its argv to the argv from cmd1 that are after the pipe
        parsePipedCmd(cmd);
        /*cmd2 = malloc(sizeof(commandT));
        cmd2->argc = cmd->argc - i;
        *cmd2->argv = cmd->argv[i + 1];
        cmd2->name = cmd2->argv[0];
        //terminate cmd argv prior to the pipe
        cmd->argc = i;
        free(cmd->argv[i]);
        cmd->argv[i] = 0;
        RunCmdPipe(cmd, cmd2);*/
        return;
      }
      else if(strcmp(cmd->argv[i], ">") == 0)
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
    RunExternalCmd(cmd, fork);
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
          RunCmdPipeRecurse(cmd);
          exit(1);
        } else //Parent process
        {
          status = waitpid(child, &status, 0);
          if(status == -1){
            perror("Wait Fail");
          }
          status = WEXITSTATUS(status);
          exit(status);
        }
      }
    else // fork failed
    {
      perror("Fork failed!\n");
    }

 } /* RunCmdPipe */
 /*
  *
  *
  */
  void RunCmdPipeRecurse(commandTLinked* cmd){
    int fd[2];
    pipe(fd);
    int grandchild_status = -1;
    if(cmd->next != NULL){
      pid_t grandchild;

      grandchild = fork();

    if(grandchild == 0) // grandchild process
    {
     close(fd[0]); //close read from pipe
     dup2(fd[1], STDOUT_FILENO); // Replace stdout with the write end of the pipe
     close(fd[1]);
     RunCmdPipeRecurse(cmd->next);
     ResolveExternalCmd(cmd->cmd);
     grandchild_status = execv(cmd->cmd->name, cmd->cmd->argv);

     if(grandchild_status == -1){
      perror("Execv Fail");
    }
    exit(1);
  }
    else //child process
    {
      close(fd[1]); //close write to pipe
      dup2(fd[0], STDIN_FILENO); // Replace stdin with the read end of the pipe
      close(fd[0]);

      grandchild_status = waitpid(grandchild, &grandchild_status, 0);
      if(grandchild_status == -1){
        perror("Wait Fail");
      }
      grandchild_status = WEXITSTATUS(grandchild_status);
      exit(grandchild_status);
    }
  }
  else{
      close(fd[0]); //close read from pipe
      dup2(fd[1], STDOUT_FILENO); // Replace stdout with the write end of the pipe
      close(fd[1]);

      ResolveExternalCmd(cmd->cmd);

      grandchild_status = execv(cmd->cmd->name, cmd->cmd->argv);
      if(grandchild_status == -1){
        perror("Execv Fail");
      }
    }
  }
 /* RunCmdPipe */


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
      pipedCmdCurrent->next = malloc(sizeof(commandTLinked));
      pipedCmdCurrent = pipedCmdCurrent->next;
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
  pipedCmdCurrent->next = malloc(sizeof(commandTLinked *));
  pipedCmdCurrent = pipedCmdCurrent->next;
  pipedCmdCurrent->cmd = cmd;
  pipedCmdCurrent->cmd->argc = argc;
  pipedCmdCurrent->next = NULL;
  ResolveExternalCmd(pipedCmdCurrent->cmd);
  pipedCmdCurrent = pipedCmdHead;

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
 static void RunExternalCmd(commandT* cmd, bool fork)
 {
  ResolveExternalCmd(cmd);
  Exec(cmd, fork, cmd->bg);
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
 static void Exec(commandT* cmd, bool forceFork, bool bg)
 {
  pid_t child;
  int status = -1;
  int pid = 0;
  int ioRedirectFile = -1;

  if (forceFork)
  {
    if ((child = fork()) > 0) // parent process
    {
      if (bg)
      {
        AddBgJob(child);
        printf("Bg job: %x\n", child);
      }
      else
      {
        fgJobPid = child;
        do
        {
          pid = waitpid(child, &status, 0);
        } while (pid > 0);
        status = WEXITSTATUS(status);
      }
    }
    else if (child == 0) // child process
    {
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
    if (getenv("PS1"))
      printf("exit\n");
    forceExit = TRUE;
    return;
  }
  else if (!strcmp(cmd->argv[0], "jobs"))
  {
    int i = 0;
    bgjobL* job_i = bgjobs;

    while(job_i != NULL)
    {
     printf("[%x] Pid: %x\n", i, job_i->pid);
     job_i = job_i->next;
     i++;
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

/*
 * AddBgJob
 *
 * arguments pid_t pid: pid of bg process
 *
 * returns: none
 *
 * This function adds the pid to the end of the bg job list
 *
 */
 void AddBgJob(pid_t pid)
 {
  bgjobL* new_bgjob = malloc(sizeof(bgjobL));
  new_bgjob->pid = pid;
  new_bgjob->next = NULL;
  if (bgjobs == NULL)
  {
    // first bg job
    bgjobs = new_bgjob;
  }
  else
  {
    // add to end of list
    bgjobL* i;
    while(i->next != NULL)
    {
      i = i->next;
    }
    i->next = new_bgjob;
  }
}

/*
 * RmBgJob
 *
 * arguments pid_t pid: pid of process that has completed
 *
 * returns: none
 *
 * This function removes a pid from the bg job list
 *
 */
 void RmBgJob(pid_t pid)
 {
  // TODO
 }
