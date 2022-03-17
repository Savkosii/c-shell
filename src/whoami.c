#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

#include "api/error.h"

static void print_username() {
    fprintf(stdout, "%s\n", getpwuid(getuid())->pw_name);
}

int main() {
    setbuf(stdout, NULL);
    print_username();
    return 0;
}