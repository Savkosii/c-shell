#define log_error(format, ...) do{ fprintf(stderr, format "\n", ## __VA_ARGS__); } while(0)

#define die(format, ...) do{ fprintf(stderr, format "\n", ## __VA_ARGS__); exit(1); } while(0)