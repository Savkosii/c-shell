#include <unistd.h>
#include <stdio.h>
#include "api/entry.h"

int main() {
    int pipes[2];
    pipe(pipes);
    dup2(pipes[0], fileno(stdin));
    write(pipes[1], "www", 4);
    close(pipes[1]);
    char buffer[MAX_LEN];
    fgets(buffer, MAX_LEN, stdin);
    close(pipes[0]);
    fputs(buffer, stdout);
}