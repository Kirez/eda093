/* 
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file 
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive: 
      $> tar cvf lab1.tgz lab1/
 *
 * All the best 
 */


#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"

#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>

/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
char *locate_executable(char *name);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void) {
  Command cmd;
  int n;

  while (!done) {

    char *line;
    line = readline("> ");

    if (!line) {
      /* Encountered EOF at top level */
      done = 1;
    } else {
      /*
       * Remove leading and trailing whitespace from the line
       * Then, if there is anything left, add it to the history list
       * and execute it.
       */
      stripwhite(line);

      if (*line) {
        add_history(line);
        /* execute it */
        parse(line, &cmd);

        if (cmd.pgm->pgmlist[0] == NULL) {
          fprintf(stderr, "Aborting: Empty program from parser");
        }

        char *executable = locate_executable(cmd.pgm->pgmlist[0]);

        if (executable != NULL) {
          pid_t pid = fork();

          assert(pid >= 0);

          if (pid == 0) {
            execvp(executable, cmd.pgm->pgmlist);
            fprintf(stderr, "Error: execvp failed with code %d\n", errno);
          } else {
            int status;
            wait(&status);
            if (status != 0) {
              printf("%d ", status);
            }
          }

          free(executable);
        } else {
          printf("lsh: command not found: %s\n", cmd.pgm->pgmlist[0]);
        }

      }
    }

    if (line) {
      free(line);
    }
  }
  return 0;
}

char *locate_executable(char *name) {
  int name_len = strlen(name);
  char *executable_location = NULL;
  int executable_location_len;
  char *path_env = getenv("PATH");
  char *token_str = malloc(sizeof(char) * (strlen(path_env) + 1));
  if (token_str == NULL) { return NULL; }
  char *env_entry;
  int env_entry_len;

  strcpy(token_str, path_env);
  env_entry = strtok(token_str, ":");

  do {
    env_entry_len = strlen(env_entry);
    executable_location_len = 1 + env_entry_len + name_len;

    char adjust = 0;
    if (env_entry[env_entry_len - 1] != '/') {
      executable_location_len += 1;
      adjust = 1;
    }

    executable_location = calloc(executable_location_len, sizeof(char));
    if (executable_location == NULL) { return NULL; }
    strcpy(executable_location, env_entry);
    if (adjust) { strcat(executable_location, "/"); }
    strcat(executable_location, name);

    if (access(executable_location, X_OK) == 0) {
      break;
    } else {
      free(executable_location);
      executable_location = NULL;
    }

    env_entry = strtok(NULL, ":");
  } while (env_entry != NULL);

  free(token_str);
  return executable_location;
}

/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void
PrintCommand(int n, Command *cmd) {
  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin ? cmd->rstdin : "<none>");
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>");
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void
PrintPgm(Pgm *p) {
  if (p == NULL) {
    return;
  } else {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    PrintPgm(p->next);
    printf("    [");
    while (*pl) {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void
stripwhite(char *string) {
  register int i = 0;

  while (isspace(string[i])) {
    i++;
  }

  if (i) {
    strcpy(string, string + i);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace (string[i])) {
    i--;
  }

  string[++i] = '\0';
}
