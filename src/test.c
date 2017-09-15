#include <unistd.h>

int main(int argc, char **argv) {
    char *argv1[3];
    argv1[0] = "ls";
    argv1[1] = "-al";
    argv1[2] = NULL;

    char *argv2[3];
    argv2[0] = "grep";
    argv2[1] = "test";
    argv2[2] = NULL;

    pid_t pid = fork();

    int read_pipe[2];
    pipe(read_pipe);

    if (pid < 0) { return -1; }
    if (pid == 0) {
        dup2(read_pipe[1], STDOUT_FILENO);
        close(read_pipe[0]);
        execvp("/bin/ls", argv1);
    }

    close(read_pipe[1]);
    pid = fork();

    int write_pipe[2];
    pipe(write_pipe);

    if (pid < 0) { return -1; }
    if (pid == 0) {
        dup2(write_pipe[0], STDIN_FILENO);
        close(write_pipe[1]);
        execvp("/bin/grep", argv2);
    }

    close(write_pipe[0]);

    //dup2(read_pipe[0], write_pipe[1]);
    //dup2(read_pipe[1], write_pipe[0]);
    //dup2(write_pipe[0], read_pipe[1]);
    dup2(write_pipe[1], read_pipe[0]);
}