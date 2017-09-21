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
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#include "parse.h"

// This should be more than enough for the scope of the lab.
// We suspect that adding support for unlimited piped commands is unnecessary
#define MAX_PIPED_COMMANDS 50

/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
char *locate_executable(char *name);
pid_t fork_executable(char *executable, char **argv, int *in_pipe, int *out_pipe, int background);
void handle_command(Command *cmd);
void handle_sigchld(int sig);
void handle_sigint(int sig);

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void) {
  Command cmd;
  int n;
  /* When non-zero, this global means the user is done using this program. */ //Unnecessary global moved here
  int done = 0;

  if (signal(SIGCHLD, &handle_sigchld) < 0) {
    fprintf(stderr, "ERROR: failed to register SIGCHLD handler (%d)\n", errno);
  };

  if (signal(SIGINT, &handle_sigint) < 0) {
    fprintf(stderr, "ERROR: failed to register SIGINT handler (%d)\n", errno);
  }

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

        // TODO allt stuff ligger där

        if (strcmp(cmd.pgm->pgmlist[0], "exit") == 0) {
          //TODO Kill the children
          exit(0);
        } else if (strcmp(cmd.pgm->pgmlist[0], "cd") == 0) {
          if (chdir(cmd.pgm->pgmlist[1]) < 0) {
            printf("lsh: cd: directory: %s not found\n", cmd.pgm->pgmlist[1]);
          }
        } else {
          handle_command(&cmd);
        }
      }
    }

    if (line) {
      free(line);
    }
  }
  return 0;
}

void handle_sigint(int sig) {
  // Prevent SIGINT from terminating parent by doing... nothing
  // printf("\n> "); // Yes, but it looks good in the terminal // Turns out it does not look good
}

void handle_sigchld(int sig) {
  int status;
  pid_t pid;

  do {
    pid = waitpid(-1, &status, WNOHANG);
  } while (pid > 0);

}

void handle_command(Command *cmd) {
  if (cmd == NULL) { return; }
  if (cmd->pgm == NULL) { return; }
  Pgm *pgm = cmd->pgm;
  if (pgm->pgmlist == NULL) { return; }

  // When a piped command can't be found all other forked commands must be killed
  // pid_array is used to keep track of forked commands started by this call
  pid_t pid_array[MAX_PIPED_COMMANDS];
  pid_array[0] = 0; //End of array
  int pid_array_cursor = 0;

  // creates file descriptors for redirects of rstdout and rstdin
  int fd_out;
  int fd_in;
  fd_out = open(cmd->rstdout,
                O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  fd_in = open(cmd->rstdin, O_RDONLY);

  // In/Out pipe variables for use during the chaining
  int in_pipe[2];
  int out_pipe[2];

  // The last (and possibly only) executable
  char *executable = locate_executable(pgm->pgmlist[0]);

  if (executable == NULL) {
    printf("lsh: command not found: %s\n", pgm->pgmlist[0]);
    return;
  }

  // Initialize pipes for the last executable and check for errors
  if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
    fprintf(stderr, "ERROR: system call pipe failed (%d)\n", errno);
  }

  // Redirect if redirection target was successfully opened
  if (fd_out >= 0) {
    close(out_pipe[1]);
    out_pipe[1] = fd_out;
  }
  if (pgm->next == NULL && fd_in >= 0) {
    close(in_pipe[0]);
    in_pipe[0] = fd_in;
  }

  // Start last executable and connect pipes
  if (pgm->next == NULL) {
    // This means that this is the first and only command to fork
    if (fd_out >= 0) {
      if (fd_in >= 0) {
        //Both input and output is to be redirected
        pid_array[pid_array_cursor++] = fork_executable(executable, pgm->pgmlist, in_pipe, out_pipe, cmd->bakground);
        pid_array[pid_array_cursor] = 0;
      } else {
        //Only output is to be redirected
        pid_array[pid_array_cursor++] = fork_executable(executable, pgm->pgmlist, NULL, out_pipe, cmd->bakground);
        pid_array[pid_array_cursor] = 0;
      }
    } else if (fd_in >= 0) {
      //Only input is to be redirected
      pid_array[pid_array_cursor++] = fork_executable(executable, pgm->pgmlist, in_pipe, NULL, cmd->bakground);
      pid_array[pid_array_cursor] = 0;
    } else {
      //Only output is to be redirected
      pid_array[pid_array_cursor++] = fork_executable(executable, pgm->pgmlist, NULL, NULL, cmd->bakground);
      pid_array[pid_array_cursor] = 0;
    }
  } else {
    //This means that this is the last command in the chain and there exists a fork that pipes its output here
    if (fd_out >= 0) {
      //Output is to be redirected
      pid_array[pid_array_cursor++] = fork_executable(executable, pgm->pgmlist, in_pipe, out_pipe, cmd->bakground);
      pid_array[pid_array_cursor] = 0;
    } else {
      //The fork's output is to be to parent's standard out
      pid_array[pid_array_cursor++] = fork_executable(executable, pgm->pgmlist, in_pipe, NULL, cmd->bakground);
      pid_array[pid_array_cursor] = 0;
    }
  }

  // The input pipes of the last executable is now the output pipe of the next executable
  out_pipe[0] = in_pipe[0];
  out_pipe[1] = in_pipe[1];

  // Goes through all executables left
  while (pgm->next != NULL) {

    pgm = pgm->next;
    executable = locate_executable(pgm->pgmlist[0]);

    if (executable == NULL) {
      printf("lsh: command not found: %s\n", pgm->pgmlist[0]);

      //Kill all forks started up till this point
      int index = 0;
      while (pid_array[index] > 0) {
        kill(pid_array[index], SIGKILL);
        waitpid(pid_array[index], NULL, 0);
        index++;
      }

      close(out_pipe[0]);
      close(out_pipe[1]);
      return;
    }

    // Initialize a pipe to connect to the next executable (or redirect if last loop)
    pipe(in_pipe);

    if (pgm->next == NULL && fd_in >= 0) {
      close(in_pipe[0]);
      in_pipe[0] = fd_in;
    }

    // Start executable and connect pipes for a new child
    pid_array[pid_array_cursor++] = fork_executable(executable,
                                                    pgm->pgmlist,
                                                    in_pipe,
                                                    out_pipe,
                                                    cmd->bakground);
    pid_array[pid_array_cursor] = 0;

    // Closes the parents pipes between children
    close(out_pipe[0]);
    close(out_pipe[1]);

    // Prepare for the next loop
    out_pipe[0] = in_pipe[0];
    out_pipe[1] = in_pipe[1];
  }

  // At this point the first command has been forked and directed so it is safe to close input pipes
  close(in_pipe[0]);
  close(in_pipe[1]);

  // We can wait for all in our process group (foreground) because we are the only parent
  if (cmd->bakground == 0) {
    while (waitpid(0, NULL, 0) > 0);
  }
}

//Forks an executable and redirects standard input and output to provided pipes and closes unused pipes on child's end
pid_t fork_executable(char *executable, char **argv, int *in_pipe, int *out_pipe, int background) {
  pid_t pid = fork();

  if (pid == 0) {
    //Child
    if (background == 1) {
      setpgid(0, 0);
    }

    if (out_pipe != NULL) {
      dup2(out_pipe[1], STDOUT_FILENO);
      close(out_pipe[0]);
      close(out_pipe[1]);
    }
    if (in_pipe != NULL) {
      dup2(in_pipe[0], STDIN_FILENO);
      close(in_pipe[0]);
      close(in_pipe[1]);
    }

    execv(executable, argv);
    return -1; //Only gets here if running of executable failed
  }

  //Parent
  return pid;
}

char *locate_executable(char *name) {
  size_t name_len = strlen(name);
  size_t executable_location_len;
  size_t env_entry_len;
  char *executable_location = NULL;
  char *path_env = getenv("PATH");
  char *token_str;
  char *env_entry;

  if (strstr(name, "/") == NULL) {
    token_str = malloc(sizeof(char) * (strlen(path_env) + 1));

    if (token_str == NULL) { return NULL; }

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
  } else {
    //name is a path
    if (access(name, X_OK) == 0) {
      executable_location = name;
    }
  }

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
