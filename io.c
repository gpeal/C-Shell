/***************************************************************************
 *  Title: Input/Output
 * -------------------------------------------------------------------------
 *    Purpose: Handles the input and output
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: io.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: io.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.3  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __IO_IMPL__

/************System include***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>
#include <time.h>

/************Private include**********************************************/
#include "io.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

/* indicates that the standard input stream is currently read  */
bool isReading = FALSE;

/************Function Prototypes******************************************/

char *getPromptName();

/************External Declaration*****************************************/

/**************Implementation***********************************************/


/*
 * PrintNewline
 *
 * arguments: none
 *
 * returns: none
 *
 * Prints a newline to stdout.
 */
void PrintNewline()
{
  putchar('\n');
} /* PrintNewLine */


/*
 * Print
 *
 * arguments:
 *   char *msg: a null-terminated string to be written to standard output
 *
 * returns: none
 *
 * Prints the string to standard output.
 */
void Print(char* msg)
{
  assert(msg != NULL);
  puts(msg);
} /* Print */


/*
 * PrintPError
 *
 * arguments:
 *   char *msg: the error to be printed
 *
 * returns: none
 *
 * Formats and prints the given string as an error message.
 */
void PrintPError(char* msg)
{
  char* format = "%s: %s";
  char str[MAXLINE];
  if (msg == NULL)
    {
      perror(SHELLNAME);
      return;
    }
  snprintf(str, MAXLINE - 1, format, SHELLNAME, msg);
  perror(str);
} /* PrintPError */


/*
 * IsReading
 *
 * arguments: none
 *
 * returns: bool: whether isReading is set
 *
 * Returns whether the isReading flag is set.
 */
bool IsReading()
{
  return isReading;
} /* IsReading */


/*
 * getCommandLine
 *
 * arguments:
 *   char **buf: pointer to a pointer to memory allocated for reading
                 from stdin
 *   int size: bytes allocated at *buf
 *
 * returns: none
 *
 * Reads from standard input until it sees a newline or EOF. Stores
 * the string that was read at *buf.
 */
void getCommandLine(char** buf, int size)
{
  char ch;
  size_t used = 0;
  char* cmd = *buf;
  char *promptName;
  cmd[0] = '\0';

  promptName = getPromptName();
  if (promptName)
    printf("%s", promptName);
  free(promptName);

  isReading = TRUE;
  while (((ch = getc(stdin)) != EOF) && (ch != '\n'))
  {
    if (used == size)
      {
        size *= 2;
        cmd = realloc(cmd, sizeof(char) * (size + 1));
      }
    cmd[used] = ch;
    used++;
    cmd[used] = '\0';
  }

  if (ch == EOF)
    forceExit = TRUE;

  isReading = FALSE;
} /* getCommandLine */

/*
 * getPromptName
 *
 * arguments: none
 *
 * returns:
 *   char *promptName: the string that will be displayed before the command prompt
 *
 *  getPromptName will parse the PS1 environment variable and return a string to be displayed
 *  at the beginning of every command prompt.
 */

char *getPromptName() {
  char *PS1 = getenv("PS1");
  int PS1_i = 0;
  char *promptName = malloc(128 * sizeof(char));
  int promptName_i = 0;
  char tmp[64];
  char *tmp2;
  time_t rawtime;
  struct tm *timeInfo;

  if (!PS1)
    return NULL;

  while (PS1[PS1_i])
  {
    if (PS1[PS1_i] == '\\')
    {
      PS1_i++;
      if (!PS1[PS1_i])
        continue;
      else if(PS1[PS1_i] == 'u')
      {
        strcpy(promptName + promptName_i, getenv("USER"));
        promptName_i = strlen(promptName);
      }
      else if(PS1[PS1_i] == 'h')
      {
        gethostname(tmp, 64);
        tmp2 = strchr(tmp, '.');
        if (tmp2)
          *tmp2 = '\0';
        strcpy(promptName + promptName_i, tmp);
        promptName_i = strlen(promptName);
      }
      else if(PS1[PS1_i] == 'w')
      {
        if (getcwd(tmp, 64))
        {
          strcpy(promptName + promptName_i, tmp);
          promptName_i = strlen(promptName);
        }
      }
      else if(PS1[PS1_i] == 't')
      {
        time(&rawtime);
        timeInfo = localtime(&rawtime);
        strftime(tmp, 64, "%H:%M:%S", timeInfo);
        strcpy(promptName + promptName_i, tmp);
        promptName_i = strlen(promptName);
      }
      else
      {
        PS1[PS1_i] = '\\';
        promptName_i++;
      }
    }
    else
    {
      promptName[promptName_i] = PS1[PS1_i];
      promptName_i++;
    }
    PS1_i++;
  }
  promptName[promptName_i] = '\0';
  return promptName;
}