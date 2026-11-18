#include "shell.h"

/* ---------- tokenize (Feature-5 mein nayi likhi) ----------
 * Command line ko tokens mein torta hai:
 *  - space/tab tokens ko alag karte hain
 *  - "double" ya 'single' quotes ke andar ka text ek hi token rehta hai,
 *    aur quotes khud hata diye jate hain:  echo "a b"  ->  echo | a b
 *  - < > |  hamesha alag token bante hain, chahe space na ho:
 *    ls>out.txt  ->  ls | > | out.txt
 * NULL-terminated array deta hai, khali line par NULL.
 */
static int is_special(char c) {
    return c == '<' || c == '>' || c == '|' || c == ';' || c == '&';
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n')
        return NULL;

    char** arglist = malloc(sizeof(char*) * (MAXARGS + 1));
    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t' || *cp == '\n' || *cp == '\r') cp++;      // spaces chhor do
        if (*cp == '\0') break;

        char token[ARGLEN];
        int len = 0;

        if (is_special(*cp)) {                       // < > | akela token
            token[len++] = *cp++;
        } else {
            char quote = 0;                          // 0 = quotes ke andar nahi
            while (*cp != '\0' && len < ARGLEN - 1) {
                if (quote) {                         // "..." ya '...' ke andar
                    if (*cp == quote) quote = 0;     // band hone wala quote: chhor do
                    else token[len++] = *cp;         // andar ka har char (space bhi) rakho
                } else {
                    if (*cp == ' ' || *cp == '\t' || *cp == '\n' || *cp == '\r' || is_special(*cp))
                        break;                       // token khatam
                    if (*cp == '"' || *cp == '\'') quote = *cp;  // khulne wala quote: chhor do
                    else token[len++] = *cp;
                }
                cp++;
            }
        }
        token[len] = '\0';
        arglist[argnum++] = strdup(token);           // token ki apni copy
    }

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }
    arglist[argnum] = NULL;
    return arglist;
}

/* ---------- Feature-2: Built-in commands ---------- */
int handle_builtin(char** arglist) {
    char* cmd = arglist[0];
    if (strcmp(cmd, "set") == 0) {      // Feature-8: saare variables dikhao
        print_vars();
        return 1;
    }

    if (strcmp(cmd, "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(cmd, "cd") == 0) {
        char* dir = arglist[1];
        if (dir == NULL)
            dir = getenv("HOME");
        if (chdir(dir) != 0)
            perror("cd");
        return 1;
    }

    if (strcmp(cmd, "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>   Change the current directory\n");
        printf("  exit       Exit the shell\n");
        printf("  help       Show this help message\n");
        printf("  jobs       List background jobs\n");
        printf("  history    Show command history\n");
        printf("  !n         Re-run command number n from history\n");
        printf("  set        List all shell variables\n");
        return 1;
    }

    if (strcmp(cmd, "jobs") == 0) {
        print_jobs();
        return 1;
    }

    if (strcmp(cmd, "history") == 0) {   // Feature-3
        print_history();
        return 1;
    }

    return 0;
}

/* ---------- Feature-3: Command history ---------- */
static char* cmd_history[HISTORY_SIZE];
static int   hist_count = 0;

void add_to_history(const char* cmdline) {
    if (hist_count == HISTORY_SIZE) {
        free(cmd_history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++)
            cmd_history[i - 1] = cmd_history[i];
        hist_count--;
    }
    cmd_history[hist_count++] = strdup(cmdline);
}

void print_history(void) {
    for (int i = 0; i < hist_count; i++)
        printf("%3d  %s\n", i + 1, cmd_history[i]);
}

char* get_history(int n) {
    if (n < 1 || n > hist_count)
        return NULL;
    return cmd_history[n - 1];
}

/* ---------- Feature-4: Tab completion (GNU Readline) ---------- */
static const char* builtin_names[] = { "cd", "exit", "help", "jobs", "history", "set", NULL };

static char* command_generator(const char* text, int state) {
    static int    b_index;
    static char*  path_copy;
    static char*  dir_name;
    static DIR*   dir;
    static size_t len;

    if (state == 0) {
        b_index = 0;
        len = strlen(text);
        free(path_copy);
        path_copy = getenv("PATH") ? strdup(getenv("PATH")) : NULL;
        dir_name  = path_copy ? strtok(path_copy, ":") : NULL;
        if (dir) closedir(dir);
        dir = NULL;
    }

    while (builtin_names[b_index] != NULL) {
        const char* name = builtin_names[b_index++];
        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }

    while (dir_name != NULL) {
        if (dir == NULL)
            dir = opendir(dir_name);
        if (dir != NULL) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strncmp(entry->d_name, text, len) == 0 &&
                    strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                    return strdup(entry->d_name);
            }
            closedir(dir);
            dir = NULL;
        }
        dir_name = strtok(NULL, ":");
    }
    return NULL;
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0)
        return rl_completion_matches(text, command_generator);
    return NULL;
}

void init_readline(void) {
    rl_attempted_completion_function = shell_completion;
}
