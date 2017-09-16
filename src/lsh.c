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
#include <fcntl.h>

/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
char *locate_executable(char *name);
pid_t fork_executable(char *executable, char **argv, int read_pipe[2], int write_pipe[2]);
pid_t fork_executable2(char *executable, char **argv, int io_fd[2]);
pid_t fork_executable3(char *executable, char**argv, int in_pipe[2], int out_pipe[2]);

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
        n = parse(line, &cmd);

        if (cmd.pgm->pgmlist[0] == NULL) {
          fprintf(stderr, "Aborting: Empty program from parser");
        }

        Pgm *pgm = cmd.pgm;

        int chain_in[2];
        int chain_out[2];
        int in_pipe[2];
        int out_pipe[2];

        pipe(in_pipe);
        pipe(out_pipe);

        char *executable = locate_executable(pgm->pgmlist[0]);

        if (executable == NULL) { printf("lsh: command not found: %s\n", pgm->pgmlist[0]);}

        fork_executable3(executable, cmd.pgm->pgmlist, in_pipe, out_pipe);

        chain_out[0] = out_pipe[0];
        chain_out[1] = out_pipe[1];

        out_pipe[0] = in_pipe[0];
        out_pipe[1] = in_pipe[1];

        if (pgm->next != NULL) {
          do {
            pgm = pgm->next;
            executable = locate_executable(pgm->pgmlist[0]);

            if (executable == NULL) { return -1; } //TODO Do not do this

            pipe(in_pipe);

            fork_executable3(executable, pgm->pgmlist, in_pipe, out_pipe); //TODO handle zombie apocalypse (OS denies fork())

            close(out_pipe[0]);
            close(out_pipe[1]);

            out_pipe[0] = in_pipe[0];
            out_pipe[1] = in_pipe[1];


          } while (pgm->next != NULL);
        }

        chain_in[0] = in_pipe[0];
        chain_in[1] = in_pipe[1];

        //Close unused ends
        close(chain_in[0]); //We do not read input
        close(chain_out[1]); //We do not write output

        char buf[1024];
        int read_len;

        while ((read_len = read(chain_out[0], buf, 1024)) > 0) {
          write(STDOUT_FILENO, buf, read_len);
        }


        //TODO redirects

        //TODO maybe read and write to standard out

        while(wait(NULL) > 0);
      }
    }

    if (line) {
      free(line);
    }
  }
  return 0;
}

//Forks an executable and redirects standard input and output to provided pipes and closes unused pipes on child's end
pid_t fork_executable3(char *executable, char **argv, int in_pipe[2], int out_pipe[2]) {
  pid_t pid = fork();

  if (pid == 0) {
    //Child
    dup2(in_pipe[0], STDIN_FILENO); //Makes it so reading from standard in is equal to reading from parent's write end
    dup2(out_pipe[1], STDOUT_FILENO); //Makes it so writing to standard out is equal to writing to parent's read end
    close(in_pipe[0]); //Replaced by STDIN_FILENO
    close(in_pipe[1]); //Parent's write end
    close(out_pipe[0]); //Parent's read end
    close(out_pipe[1]); //Replaced by STDOUT_FILENO
    execvp(executable, argv);
    return -1; //Only gets here if running of executable failed
  }

  //Parent

  return pid;
}

pid_t fork_executable2(char *executable, char **argv, int io_fd[3]) {
  int read_pipe[2];
  int write_pipe[2];
  int error_pipe[2];

  if (pipe(read_pipe) < 0) {
    fprintf(stderr, "ERROR: read pipe failed (%d)\n", errno);
    return -1;
  }
  if (pipe(write_pipe) < 0) {
    fprintf(stderr, "ERROR: write pipe failed (%d)\n", errno);
    return -1;
  }
  if (pipe(error_pipe) < 0) {
    fprintf(stderr, "ERROR: error pipe failed (%d)\n", errno);
  }

  pid_t pid = fork();

  if (pid < 0) {
    fprintf(stderr, "ERROR: fork failed (%d)\n", errno);
    return -1;
  } else if(pid == 0) {
    dup2(write_pipe[0], STDIN_FILENO);
    dup2(read_pipe[1], STDOUT_FILENO);
    dup2(error_pipe[1], STDERR_FILENO);
    close(read_pipe[0]);
    close(write_pipe[1]);
    close(error_pipe[0]);
    execvp(executable, argv);
    return -1; // Execution never reaches this point assuming execvp is successful
  } else {
    close(read_pipe[1]);
    close(write_pipe[0]);
    close(error_pipe[1]);
    io_fd[0] = read_pipe[0];
    io_fd[1] = write_pipe[1];
    io_fd[2] = error_pipe[0];
  }

  return pid;
}

pid_t fork_executable(char *executable, char **argv, int read_pipe[2], int write_pipe[2]) {
  pid_t pid = fork();

  if (pid < 0) {
    fprintf(stderr, "ERROR: fork failed (%d)\n", errno);
    return -1;
  } else if(pid == 0) {
    dup2(write_pipe[0], STDIN_FILENO);
    dup2(read_pipe[1], STDOUT_FILENO);
    close(read_pipe[0]);
    close(write_pipe[1]);
    execvp(executable, argv);
    return -1;
  } else {
    close(read_pipe[1]);
    close(write_pipe[0]);
  }

  return pid;
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
