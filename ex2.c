#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>


void prompt();
void done(int cm_count, int command_length);
char **make_argv(char *command, int *aurg_num);
char *redir_check(int *flag, char *command);
void dir(int *rid, char *file, int *value);
void freeArgv(char **argv, int size);
void execvp_pipe(char **argv_left, char **argv_right, int aurg_num, int aurg_num2, int *rid, char *file);
void execvp_noPipe(char **argv_left, int aurg_num, int *rid, char *file);

int main()
{
    int cm_count = 0;       //count number of commands
    int command_length = 0; // count total length of commands

    while (1)
    {
        int i = 0, aurg_num = 1, aurg_num2 = 1;
        char command[512]; //array for max command length(include '\n','\0')
        prompt();          //print the prompt line

        char *fget = fgets(command, 510, stdin);
        if (fget == NULL) 
            continue;
        if (strcmp(command, "\n") != 0)
            command_length += strlen(command);

        int rid = 0;// rid("redirection") is a flag that shows us which redirection we using
        char *file = redir_check(&rid, command);//get the file name and update the rid flag

        int pipe_exist = 0;//the varible tell us if there is a '|' in the command
        if (strchr(command, '|') != NULL)
            pipe_exist = 1;

        char *left_cmd,*right_cmd;
        if (pipe_exist == 1)
        {
            right_cmd = strchr(command, '|');
            right_cmd++;
            while (*right_cmd == ' ') //"delete" the " " before the string
                right_cmd++;
        }
        left_cmd = strtok(command, "|");

        //the program not supported "cd" command yet, so we have to check this and continue to next command
        left_cmd = strtok(left_cmd, " "); 
        if (pipe_exist!=1 &&rid==0&& (strcmp(left_cmd, "cd") == 0 || strcmp(left_cmd, "cd\n") == 0))
        {
            cm_count++;
            printf("Command not supperted (Yet)\n");
            continue;
        }
        //check if the user want to finish the progrm by enter the command "done"
        else if (pipe_exist!=1 &&rid==0&& (strcmp(left_cmd, "done") == 0 || strcmp(left_cmd, "done\n" )== 0))
        {
            cm_count++;
            done(cm_count, command_length); //the function will calculate and print the results of the program
            exit(0);
        }
        //check if not entered only  "\n"
        else if (strcmp(left_cmd, "\n") == 0)
            continue;

        char **argv_left = make_argv(left_cmd, &aurg_num);
        char **argv_right = NULL;
        if (pipe_exist == 1)
        {
            right_cmd = strtok(right_cmd, " "); 
            argv_right = make_argv(right_cmd, &aurg_num2);
        }

         if (pipe_exist == 1)
            execvp_pipe(argv_left, argv_right, aurg_num, aurg_num2, &rid, file);
        else
            execvp_noPipe(argv_left, aurg_num, &rid, file);

        cm_count++;
        freeArgv(argv_left, aurg_num);
        if (argv_right != NULL)
            freeArgv(argv_right, aurg_num2);
    }
    return 0;
}


/*This function executes the process when the command does not have the "|" sign, meaning no pipe is needed*/
void execvp_noPipe(char **argv_left, int aurg_num, int *rid, char *file)
{
    pid_t x;
    int a, value;
    x = fork();
    if (x == 0)
    {
        if (*rid == 0) //no redirection
        {
            a = execvp(argv_left[0], argv_left);
            if (a == -1)
            {
                freeArgv(argv_left, aurg_num);
                exit(0);
            }
        }
        else if (*rid == 3 || *rid == 2 || *rid == 1 || *rid == 4) //>,>>,2>,<
        {
            dir(rid, file, &value);
            if (value == -1)
            {
                fprintf(stderr, "dup2 failed\n");
                freeArgv(argv_left, aurg_num);
                exit(1);
            }
            a = execvp(argv_left[0], argv_left);
            if (a == -1)
                freeArgv(argv_left, aurg_num);

            exit(0);
        }
    }
    else if (x < 0)
        fprintf(stderr, "problem with fork \n");
    else
        wait(NULL);
}

/*This function executes the process when the command has the '|', that means that you need to make a pipe*/
void execvp_pipe(char **argv_left, char **argv_right, int aurg_num, int aurg_num2, int *rid, char *file)
{
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe faild");
        freeArgv(argv_left, aurg_num);
        freeArgv(argv_right, aurg_num2);
        exit(EXIT_FAILURE);
    }
    pid_t x;
    x = fork();
    if (x < 0)
        fprintf(stderr, "problem with fork \n");
    if (x == 0) //first 'child' process for the left section of the command
    {
        close(pipe_fd[0]);
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
        {
            fprintf(stderr, "dup2 failed\n");
            freeArgv(argv_left, aurg_num);
            freeArgv(argv_right, aurg_num2);
            exit(1);
        }
        close(pipe_fd[1]);
        int val = execvp(argv_left[0], argv_left);
        if (val == -1)
        {
            freeArgv(argv_left, aurg_num);
            freeArgv(argv_right, aurg_num2);
            exit(0);
        }
    }
    else
    {
        pid_t y;
        y = fork();

        if (y < 0)
        {
            freeArgv(argv_left, aurg_num);
            freeArgv(argv_right, aurg_num2);
            fprintf(stderr, "problem with fork \n");
            exit(EXIT_FAILURE);
        }

        if (y == 0)//second 'child' process for the right section of the command
        {
            int valu = 0;
            close(pipe_fd[1]);
            if (dup2(pipe_fd[0], STDIN_FILENO) == -1)
            {
                fprintf(stderr, "dup2 failed\n");
                freeArgv(argv_left, aurg_num);
                freeArgv(argv_right, aurg_num2);
                exit(1);
            }
            close(pipe_fd[0]);

            dir(rid, file, &valu);
            if (valu == -1)
            {
                fprintf(stderr, "dup2 failed\n");
                freeArgv(argv_left, aurg_num);

                freeArgv(argv_right, aurg_num2);
                exit(1);
            }
            int e = execvp(argv_right[0], argv_right);
            if (e == -1)
            {
                freeArgv(argv_right, aurg_num2);
                freeArgv(argv_left, aurg_num);
                exit(0);
            }
        }
        else
        {
            close(pipe_fd[1]);
            close(pipe_fd[0]);
            wait(NULL);
            wait(NULL);
        }
    }
}

//free memory
void freeArgv(char **argv, int size)
{
    int i;
    for (i = 0; i <= size; i++)
        free(argv[i]);
    free(argv);
}

/*The function checks which flag of 'redirection' was received, open the appropriate permissions
 and routing for each flag*/
void dir(int *rid, char *file, int *value)
{
    int fd;
    if (*rid == 3)
    {
        fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        *value = dup2(fd, STDOUT_FILENO);
    }
    else if (*rid == 2)
    {
        fd = open(file, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
        *value = dup2(fd, STDOUT_FILENO);
    }
    else if (*rid == 1)
    {
        fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        *value = dup2(fd, STDERR_FILENO);
    }
    else if (*rid == 4)
    {
        fd = open(file, O_RDONLY, 0);
        *value = dup2(fd, STDIN_FILENO);
    }
}


/*The function checks which redirection the user used,  assign each relevant flag number to 
each redirection and returns the name of the relevant file*/
char *redir_check(int *flag, char *command)
{
    int i;
    char *file = NULL;
    for (i = 0; i < strlen(command); i++)
    {
        if (command[i] == '2' && i < strlen(command) - 1 && command[i + 1] == '>')
        { 
            *flag = 1;
            file = strchr(command, '2');
            file += 2; //where the file name will start
            strtok(command, "2");
            while (*(file) == ' ')//the file name will statr after the " "
                file++;
        }
        else if (command[i] == '>' && i < strlen(command) - 1 && command[i + 1] == '>')
        {
            *flag = 2;
            file = strchr(command, '>');
            file += 2;
            strtok(command, ">");
            while (*(file) == ' ')
                file++;
        }
        else if (command[i] == '>')
        {
            *flag = 3;
            file = strchr(command, '>');
            file += 1;
            strtok(command, ">");
            while (*(file) == ' ')
                file++;
        }
        else if (command[i] == '<')
        {
            *flag = 4;
            file = strchr(command, '<');
            file += 1;
            strtok(command, "<");
            while (*(file) == ' ')
                file++;
        }
        else
            *flag = 0;
    }

    if (file != NULL)
        strtok(file, "\n");
    return file;
}

/*The function will break the command that is received as a string according to arguments
 before sending to execute,
 each cell in the array will have a string and the last one will be NULL*/
char **make_argv(char *token, int *aurg_num)
{
    char **argv = (char **)malloc(*aurg_num * sizeof(char *));
    if (argv == NULL)
    {
        free(argv);
        fprintf(stderr, "Error: cannot allocate memory");
        exit(1);
    }
    argv[0] = (char *)malloc((strlen(token) + 1) * sizeof(char));
    if (argv[0] == NULL)
    {
        fprintf(stderr, "Error: cannot allocate memory");
        free(argv);
        free(argv[0]);
        exit(1);
    }
    strncpy(argv[0], token, strlen(token) + 1);

    token = strtok(NULL, " ");
    //loop for inserting the arguments to the array, each argument for seperate cell in the array
    while (token != NULL)
    {
        if (strcmp(token, "\n") == 0)
            break;
        (*aurg_num)++; //number of argumrnts that enterted
        argv = (char **)realloc(argv, (*aurg_num) * sizeof(char *));
        argv[(*aurg_num) - 1] = (char *)malloc((strlen(token) + 1) * sizeof(char));
        if (argv[(*aurg_num) - 1] == NULL)
        {
            free(argv[*(aurg_num)-1]);
            fprintf(stderr, "Error: cannot allocate memory");
            exit(1);
        }
        strncpy(argv[(*aurg_num) - 1], token, strlen(token) + 1);
        token = strtok(NULL, " ");
    }

    strtok(argv[(*aurg_num) - 1], "\n");                               //remove '\n' from the last argument
    argv = (char **)realloc(argv, ((*aurg_num) + 1) * sizeof(char *)); //add one "NULL" cell
    argv[*aurg_num] = NULL;
    if (argv == NULL)
    {
        free(argv);
        fprintf(stderr, "Error: cannot allocate memory");
        exit(1);
    }

    return argv;
}
//The function will print the prompt line
void prompt()
{
    struct passwd *pw = getpwuid(0);
    if (pw == NULL)
    {
        fprintf(stderr, "getpwuid: no password entry\n");
        exit(1);
    }
    printf("%s@", pw->pw_name);
    char cwd[256];
    char *getcw = getcwd(cwd, sizeof(cwd));
    if (getcw == NULL)
    {
        fprintf(stderr, "getcwd() error\n");
        exit(1);
    }
    printf("%s>", cwd);
}
/*The function will print the number of commands entered and the total length of the commands.
 The function will calculate the average characters per command and print it.*/
void done(int cm_count, int command_length)
{
    printf("Num of commands: %d\n", cm_count);
    printf("Total length of all commands: %d\n", command_length);
    double avg_length = (double)command_length / cm_count;
    printf("Average length of all commands: %f\n", avg_length);
    printf("See you Next time!\n");
}