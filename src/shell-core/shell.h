#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <glob.h>

#include "../api/entry.h"

#define BEGIN_WITH_DELIMITER 0100

#define DELIMITER_CONCAT 0200

#define EMPTY_BETWEEN_DEMILITER 0400

static const char *sys_home_directory;

static const char *app_home_directory = "../commands";

static int exec(char *line);
