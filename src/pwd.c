#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "api/error.h"

static void print_working_directory() {
    fprintf(stdout, "%s\n", getcwd(NULL, 0));
}

int main() {
    setbuf(stdout, NULL);
    print_working_directory();
    return 0;
}