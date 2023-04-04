#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>

#define BSH_TOK_BUFSIZE 64
#define BSH_TOK_DELIM " \t\r\n\a"

/*
  Function Declarations for builtin shell commands:
 */
int bsh_cd(char **args);
int bsh_pwd(char **args);
int bsh_ls(char **args);
int bsh_touch(char **args);
int bsh_mkdir(char **args);
int bsh_echo(char **args);
int bsh_cat(char **args);
int bsh_whoami(char **args);
int bsh_host(char **args);
int bsh_help(char **args);
int bsh_exit(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
struct Builtin {
    char *name;
    int (*func) (char **);
};
struct Builtin builtins[] = {
    { "cd", &bsh_cd },
    { "pwd", &bsh_pwd },
    { "ls", &bsh_ls },
    { "touch", &bsh_touch },
    { "mkdir", &bsh_mkdir },
    { "echo", &bsh_echo },
    { "cat", &bsh_cat },
    { "whoami", &bsh_whoami },
    { "host", &bsh_host },
    { "help", &bsh_help },
    { "exit", &bsh_exit }
};

int bsh_num_builtins()
{
    return sizeof(builtins) / sizeof(struct Builtin);
}

int bsh_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "bsh: expected argument to \"cd\"\n");
    }
    else if (chdir(args[1]) != 0)
    {
        perror("bsh");
    }
    
    return 1;
}

int bsh_pwd(char **args)
{
    char* buffer = getcwd(NULL, 0);

    if (buffer == NULL) {
        perror("failed to get current directory\n");
    } else {
        printf("%s\n", buffer);
        free(buffer);
    }

    return 1;
}

int bsh_ls(char **args)
{
    struct dirent *dir;
    DIR *directory = opendir(args[1] != NULL ? args[1] : ".");

    if (!directory) {
        perror("bsh");
    } else {
        while ((dir = readdir(directory)) != NULL) {
            if(strcmp(dir -> d_name, ".") != 0 && strcmp(dir -> d_name, "..") != 0) printf("%s\n", dir -> d_name);
        }
        closedir(directory);
    }

    return 1;
}

int bsh_touch(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "bsh: expected argument to \"touch\"\n");
    } else {
        FILE* file_ptr = fopen(args[1], "w");
        fclose(file_ptr);
    }

    return 1;
}

int bsh_mkdir(char **args)
{
    while (*++args) {
        struct stat st = {0};
        if (stat(*args, &st) == -1) {
            mkdir(*args, 0700);
        }
    }
    return 1;
}

int bsh_echo(char **args)
{
    while (*++args) {
        printf("%s", *args);
        if (args[1]) printf(" ");
    }
    printf("\n");
    return 1;
}

int bsh_cat(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "bsh: expected argument to \"cat\"\n");
    } else {
        const int bufferSize = 4096;
        char buffer[bufferSize];

        FILE* file_ptr = fopen(args[1], "rb");

        if (file_ptr == NULL) {
            perror("bsh");
        } else {
            while (fgets(buffer, bufferSize, file_ptr)) {
                int length = strlen(buffer);
                buffer[length - 1] = '\0';
                fprintf(stdout, "%s\n", buffer);
            }
            fclose(file_ptr);
        }
    }

    return 1;
}

int bsh_whoami(char **args)
{
    struct passwd *p = getpwuid(getuid());
    printf("%s\n", p -> pw_name);
    return 1;
}

int bsh_host(char **args)
{
    const int HOSTNAME_MAX_SIZE = 1024;
    char hostname[1024];
    gethostname(hostname, sizeof(hostname));
    printf("%s\n", hostname);
    return 1;
}

int bsh_help(char **args)
{
    int i;
    printf("Awesome bsh\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < bsh_num_builtins(); i++)
    {
        printf("  %s\n", builtins[i].name);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int bsh_exit(char **args)
{
    return 0;
}

int bsh_launch(char **args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        if (execvp(args[0], args) == -1)
        {
            perror("bsh");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        // Error forking
        perror("bsh");
    }
    else
    {
        // Parent process
        int status;
        do
        {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int bsh_execute(char **args)
{
    if (args[0] == NULL)
    {
        // An empty command was entered.
        return 1;
    }

    for (int i = 0; i < bsh_num_builtins(); i++)
    {
        if (strcmp(args[0], builtins[i].name) == 0)
        {
            return (*builtins[i].func)(args);
        }
    }

    return bsh_launch(args);
}

char *bsh_read_line(void)
{
    char *line = NULL;
    ssize_t bufsize = 0; // have getline allocate a buffer for us
    if (getline(&line, &bufsize, stdin) == -1)
    {
        if (feof(stdin))
        {
            exit(EXIT_SUCCESS); // We received an EOF
        }
        else
        {
            perror("bsh: getline\n");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char **bsh_split_line(char *line)
{
    int bufsize = BSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token, **tokens_backup;

    if (!tokens)
    {
        fprintf(stderr, "bsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, BSH_TOK_DELIM);
    while (token != NULL)
    {
        tokens[position] = token;
        position++;

        if (position >= bufsize)
        {
            bufsize += BSH_TOK_BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                free(tokens_backup);
                fprintf(stderr, "bsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, BSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

void bsh_loop(void)
{
    int status;

    do
    {
        printf("> ");
        char *line = bsh_read_line();
        char **args = bsh_split_line(line);
        status = bsh_execute(args);

        free(line);
        free(args);
    } while (status);
}

int main(int argc, char **argv)
{
    bsh_loop();
    return EXIT_SUCCESS;
}
