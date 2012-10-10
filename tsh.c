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



/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */
static void sig(int);
/* loads ~/.tshrc file */
static void initTshRC();

/************External Declaration*****************************************/

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

  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR)
    PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR)
    PrintPError("SIGTSTP");

  initTshRC();

  while (!forceExit) /* repeat forever */
  {
    /* read command line */
    getCommandLine(&cmdLine, BUFSIZE);

    /* checks the status of background jobs */
    CheckJobs();

    /* interpret command and line
     * includes executing of commands */
    Interpret(cmdLine);

  }
  fflush(stdout);
  /* shell termination */
  free(cmdLine);
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
 */
static void sig(int signo)
{
  if (signo == SIGINT || signo == SIGTERM)
  {
    while (bgjobs)
    {
      kill(-1 * bgjobs->pid, signo);
      bgjobs = bgjobs->next;
    }
    StopFgProc();
  }
} /* sig */

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