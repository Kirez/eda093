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
pid_t fork_executable3(char *executable, char **argv, int in_pipe[2], int out_pipe[2]);
void handle_command(Command *cmd);

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

		// TODO allt stuff ligger där
		
        handle_command(&cmd);
      }
    }

    if (line) {
      free(line);
    }
  }
  return 0;
}

void handle_command(Command *cmd) {
  if (cmd == NULL) { return; }
  if (cmd->pgm == NULL) { return; }
  Pgm *pgm = cmd->pgm;
  if (pgm->pgmlist == NULL) { return; }
  
  // creates file descriptors for redirects of rstdout and rstdin
  int fd_out;
  int fd_in;
  fd_out = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  fd_in = open(cmd->rstdin, O_RDONLY);
  
  // chain_in/out is the input/output pipe for the entire chain
  int chain_in[2];
  int chain_out[2];
  // In/Out pipe variables for use during the chaining
  int in_pipe[2];
  int out_pipe[2];

  // The last (and possibly only) executable
  char *executable = locate_executable(pgm->pgmlist[0]);

  if (executable == NULL) {
    printf("lsh: command not found: %s\n", pgm->pgmlist[0]);
    return;
  }

  // Initialize pipes for te last executable and check for errors
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
  fork_executable3(executable, pgm->pgmlist, in_pipe, out_pipe);

  // Save pipes of the last executable
  chain_out[0] = out_pipe[0];
  chain_out[1] = out_pipe[1];

  // The input pipes of the last executable is now the output pipe of the next executable
  out_pipe[0] = in_pipe[0];
  out_pipe[1] = in_pipe[1];

  // Goes through all executables left
  while (pgm->next != NULL) {

    pgm = pgm->next;
    executable = locate_executable(pgm->pgmlist[0]);

    if (executable == NULL) { return; } //TODO Do not do this

	// Initialize a pipe to connect to the next executable (or redirect if last loop)
    pipe(in_pipe);

    // TODO make it work
    if (pgm->next == NULL && fd_in >= 0) {
      printf("This\n");
      close(in_pipe[0]);
      in_pipe[0] = fd_in;
    }

	// Start executable and connect pipes for a new child
    fork_executable3(executable,
                     pgm->pgmlist,
                     in_pipe,
                     out_pipe); //TODO handle zombie apocalypse (OS denies fork())
    
    // Closes the parents pipes between children
    close(out_pipe[0]);
    close(out_pipe[1]);

	// Prepare for the next loop
    out_pipe[0] = in_pipe[0];
    out_pipe[1] = in_pipe[1];
  }

/*  // Prepares for possible redirect
  chain_in[0] = in_pipe[0];
  chain_in[1] = in_pipe[1];*/

  //Close unused ends
//  close(chain_in[0]); //We do not read input
  close(in_pipe[0]);
  close(chain_out[1]); //We do not write output

  char buf[1024];
  int read_len;

  while ((read_len = read(chain_out[0], buf, 1024)) > 0) {
    write(STDOUT_FILENO, buf, read_len);
  }
  
  close(in_pipe[1]);
  close(chain_out[0]);
/*  close(chain_in[1]);
  close(chain_out[0]);*/
  
  //TODO redirects

  //TODO maybe read and write to standard out

  // Because we only have one parent
  while (wait(NULL) > 0);

}



//Forks an executable and redirects standard input and output to provided pipes and closes unused pipes on child's end
pid_t fork_executable3(char *executable, char **argv, int in_pipe[2], int out_pipe[2]) {
  pid_t pid = fork();

  if (pid == 0) {
    //Child
    //Makes it so reading from standard in is equal to reading from parent's write end
    dup2(in_pipe[0], STDIN_FILENO);
    //Makes it so writing to standard out is equal to writing to parent's read end
    dup2(out_pipe[1], STDOUT_FILENO);
    //Replaced by STDIN_FILENO
    close(in_pipe[0]);
    //close Parent's write end
    close(in_pipe[1]);
    //close Parent's read end
    close(out_pipe[0]);
    //Replaced by STDOUT_FILENO
    close(out_pipe[1]);
    execv(executable, argv);
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
  } else if (pid == 0) {
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
  } else if (pid == 0) {
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
