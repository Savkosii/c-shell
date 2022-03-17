#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc == 1) {
        fputc('\n', stdout);
        return 0;
    }

    for (size_t i = 1; i < argc - 1; i++) {
        fprintf(stdout, "%s ", argv[i]);
    }

    fprintf(stdout, "%s\n", argv[argc - 1]);
}