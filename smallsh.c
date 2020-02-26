/* NAME: smallsh.c
 *
 * SYNOPSIS: 
 *
 * DESCRIPTION
 * AUTHOR: Gerson Lindor Jr.
 * DATE CREATED: February 16, 2020
 * DATE LAST MODIFIED:
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

const int LINE = 2048;
const int ARG_LIMIT = 512;
const int CMD_TOKEN = 518; // command + arg limit + input + output + background tokens

struct commandLine {
    char **cmdLine;
    int argCount;
    bool isBackground;
    char *inputFile;
    char *outputFile;
    int status;
};


struct backgroundPID {
    int bgPid[200];
    int count;
};

int foregroundMode = 0;



int verifyInput(char *userInput);
void bashManager(struct commandLine *cmd, struct backgroundPID *bgPD);
int builtInManager(struct commandLine *cmd, struct backgroundPID *bg);
void parser(struct commandLine *cmd, char *user);
char *getString(char *data);
void destroy(struct commandLine *cmd);
struct commandLine * create();
void prompt(struct commandLine *cmd, int *exitValue, struct backgroundPID *bgPD);
void directoryCmd(struct commandLine *cmd);
int handleRedirect (struct commandLine *cmd);
int getExitStatus(int childProcess, struct commandLine *cmd);
void ignoreSignal(int signal);
void foregroundModeSignal();
void setFgMode();

/*** unit tests ***/
void print(struct commandLine *cmd) {
    int index = 0;

    while (cmd->cmdLine[index]) {
        printf("%s\n", cmd->cmdLine[index]);
        ++index;
    }
    printf("argCount = %d\n", cmd->argCount);
    printf("input file = %s\n", cmd->inputFile);
    printf("output file = %s\n", cmd->outputFile);
    if (cmd->isBackground) {
        printf("In background mode\n");
    }
}
/**********************/


int main() {
    struct commandLine *line;
    struct backgroundPID bgProcess;
    int quitShell = 1;
    int previousExitStatus = 0;
    int exitMethod;

    bgProcess.count = 0;
    // initialize all background pid to -1
    for (int i = 0; i < 200; ++i) {
        bgProcess.bgPid[i] = -1;
    }
    do {
        line = create();
        line->status = previousExitStatus;
        prompt(line, &quitShell, &bgProcess);
       // search and find status of background processes 
       for (int i = 0; i < bgProcess.count; ++i) {
           if (bgProcess.bgPid[i] != -1) {
               // check the status of each process in the array
               if (waitpid(bgProcess.bgPid[i], &exitMethod, WNOHANG) != 0) {
                   printf("background pid %d is done: ", bgProcess.bgPid[i]);
                   //get the exit status
                   if(getExitStatus(exitMethod, line)) {
                        printf("Exit value %d\n", line->status);
                   }
                   bgProcess.bgPid[i] = -1;
                }
           }
       }
        previousExitStatus = line->status;
        destroy(line);
    } while (quitShell != 0);
    return 0;
}


/*
 *
 */
void setFgMode() {
    char *onMsg = "\nEntering foreground-only mode (& is now ignored)\n: ";
    char *offMsg = "\nExiting foreground-only mode\n: ";

    // 52 size
    if (foregroundMode) {
        foregroundMode = 0;  // turn of mode
        write(STDOUT_FILENO, offMsg, 32);
    } else {
        foregroundMode = 1; // turn on mode
        write(STDOUT_FILENO, onMsg, 52);
    }
}


/*
 *
 */
void foregroundModeSignal() {
    struct sigaction SIGTSTP_action = {0};

    // setup sigaction signal
    SIGTSTP_action.sa_handler = setFgMode;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    // activate sigaction
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

/*
 *
 *
 */
void ignoreSignal(int signal) {
    struct sigaction ignore_action = {0};

    // setup signal handling
    ignore_action.sa_handler = SIG_IGN;
    // activate sigaction for ignoring signals
    sigaction(signal, &ignore_action, NULL);
}


// Deallocates dynamic memory in commandLine struct
void destroy(struct commandLine *cmd) {
    int index = 0;
    if (cmd) {
        // delete command line
        if (cmd->cmdLine) {
            while (cmd->cmdLine[index] && index < CMD_TOKEN) {
                free(cmd->cmdLine[index]);
                cmd->cmdLine[index] = NULL;
                ++index;
            }
            free(cmd->cmdLine);
            cmd->cmdLine = NULL;
        }
        // delete inputFile
        if (cmd->inputFile) {
            free(cmd->inputFile);
            cmd->inputFile = NULL;
        }
        // delete outputFile
        if (cmd->outputFile) {
            free(cmd->outputFile);
            cmd->outputFile = NULL;
        }
    }
}


/* Redirects stdout and stdin when command line has ">", "<", or a background
 * command with no arguments.
 * @precondition: cmd->outFile or cmd->inFile must contain a file name, or cmd->isBackground
 *               is true
 * @postcondition: stdout or stdin is redirected to a file or to /dev/null 
 * @return: 0 - redirection failed
 *          1 - standard I/O redirection is successful
 */
int handleRedirect (struct commandLine *cmd) {
    int fDOut, fDIn, fDNull,result, flag = 0;
    if (cmd->inputFile) {
        fDIn = open(cmd->inputFile, O_RDONLY);
        if (fDIn == -1) { fprintf(stderr, "cannot open %s for input\n", cmd->inputFile); exit(1);}
        result = dup2(fDIn, 0);
        if (result == -1) { fprintf(stderr, "source dup2() on fDIn\n"); exit(1);}
        fcntl(fDIn, F_SETFD, FD_CLOEXEC);
        flag = 1;

    }
    if (cmd->outputFile) {
        fDOut = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fDOut == -1) { fprintf(stderr, "cannot open %s for output\n", cmd->outputFile); exit(1);}
        result = dup2(fDOut, 1);
        if (result == -1) { fprintf(stderr, "source dup2() on fDOut\n"); exit(1);}
        fcntl(fDOut, F_SETFD, FD_CLOEXEC);
        flag = 1;
    }
    // Background commands with no arguments redirects stdout and stdin to /dev/null
    if (cmd->isBackground && cmd->argCount == 0) {
        fDNull = open("/dev/null", O_RDWR);
        result = dup2(fDNull, 0);
        if (result == -1) { fprintf(stderr, "source dup2() on fDNull\n"); exit(1);}
        result = dup2(fDNull, 1);
        if (result == -1) { fprintf(stderr, "source dup2() on fDNull\n"); exit(1);}
        fcntl(fDNull, F_SETFD, FD_CLOEXEC);
    }
    // call exec and catch exit value
    if (flag) {
        if (execlp(cmd->cmdLine[0], cmd->cmdLine[0], NULL) == -1) {
            fprintf(stderr, "No such file or directory");
            exit(1);
        }
    }
    return flag;
}


/* Checks the exit status of a process
 * @precondition: none
 * @postcondition:  Exit status of process is assigned to cmd->status.   
 *                  If process is terminated by a signal, the program notifies the user
 * @return: 0 - the process did not terminate normally
 *          1 - the process terminated normally
 */
int getExitStatus(int childProcess, struct commandLine *cmd) {
    int signal, flag = 0;
    // determine if child terminated normally
    if (WIFEXITED(childProcess)) {
        cmd->status = WEXITSTATUS(childProcess);
        flag = 1;
    } else if (WIFSIGNALED(childProcess)) {
        signal = WTERMSIG(childProcess);
        printf("Terminated by signal %d\n", signal);
        fflush(stdout);
   }
   return flag;
}


/* Manages non-built in commands of this shell. Manages foreground and background commands, and
 * standard I/O redirection.  The non-built in commands are bash shell commands.
 * @precondition: There exist an array that holds the process id of background process.
 * @postcondition: A new process is created and executed for a non-built in command
 */
void bashManager(struct commandLine *cmd, struct backgroundPID *bgPD) {
    pid_t spawnpid = -5;
    int exitChildMethod = -5, flag;

    // call another process to handle non-built in commands
    // Parent process ignores ^C
    ignoreSignal(SIGINT);
    // catch CTRL-Z (SIGTSP signal)
    foregroundModeSignal();
    spawnpid = fork();
    switch (spawnpid) {
        case -1:
            fprintf(stderr, "Hull Breached!");
            exit(1);
            break;
        case 0:
            // ignore CTRL-Z (SIGTSTP signal)
            ignoreSignal(SIGTSTP);
            // handle redirect
            if (!handleRedirect(cmd)) {
                // handle foreground
                if (execvp(cmd->cmdLine[0], cmd->cmdLine) < 0) {
                    fprintf(stderr, "No such file or directory.\n");
                    exit(1);
                }
            }
            exit(0);
            break;
        default:
            // wait for foreground commands to complete in child process
           if (!cmd->isBackground) {
                waitpid(spawnpid, &exitChildMethod, 0); 
                flag = getExitStatus(exitChildMethod, cmd);
           } else {
                // handle background commands
                waitpid(spawnpid, &exitChildMethod, WNOHANG);
                printf("background pid is %d\n", spawnpid);
                fflush(stdout);
                // keep track of process id in an array
                bgPD->bgPid[bgPD->count] = spawnpid;
                ++bgPD->count;
            }
            break;
    }
}


/* Changes from current directory to a specified directory
 * @precondition: To execute command, cmd->cmdLine[0] should me "cd" and 
 *                argument count cannot be greater than 1
 * @postcondition: The current directory should not be the same location as this program, 
 *                 unless the user enters "cd ."
 */
void directoryCmd(struct commandLine *cmd) {
    char path[200];

    memset(path, '\0', 200);
    // verify argument count is not greater than 1
    if (cmd->argCount > 1) { return; }
    // change directory to HOME - 0 arguments
    if (cmd->argCount == 0) {
        strcpy(path, getenv("HOME"));
    } else {
        strcpy(path, cmd->cmdLine[1]);
    }
    // relative and absolute directory change
    if(chdir(path) == -1) {
        fprintf(stderr, "Error: cd: No such file or directory\n");
    }
}


/* Determines which built-in commands to execute according to the value of
 * cmd->cmdLine[0]
 * @precondtion: cmd->cmdLine[0] must be "cd", "status", or "exit"
 * @postcondition: Function executes any options defined in this precondition
 * @return: 0 - signals the program to exist smallSh
 *          1 - signals the program to continue running smallsh
 */
int builtInManager(struct commandLine *cmd, struct backgroundPID *bg) {
    int flag = 1;
    if (!strcmp(cmd->cmdLine[0], "cd")) {
        directoryCmd(cmd);
    } else if (!strcmp(cmd->cmdLine[0], "status")) {
        printf("exit value %d\n", cmd->status);
        fflush(stdout);
    } else {
        // use exit command
        //  Kill all processes and jobs before exiting
        for (int i = 0; i < bg->count; ++i) {
            if (bg->bgPid[i] != -1) {
                kill(bg->bgPid[i], SIGKILL);
            }
        }
        flag = 0;
    }
    return flag;
}


/*  Verifies that the input does not match # or only a newline, and this 
 *  function removes the new line character from a user input
 *  @return: 0 - the command line consist of "#" as first character or its only a newline character
 *           1 - the command line is not a new line or a comment
 *           2 - the command is a background command because it consist of '&'
 */
int verifyInput( char *userInput) {
    int flag = 1;
    char symbol= '#';
    char format = '\n';
    int size = -1;

    // remove the newline character at the end of user input string
    size = strlen(userInput);
    //  verify that command line is not a new line or a comment #
    if (userInput[0] == symbol || (size == 1 && userInput[0] == format)) {
        return 0;;
    }
    for (int i = size; i >= 0; --i) {
        if (userInput[i] == '\n') {
            userInput[i] = '\0';
        }
        if (userInput[i] == '&') {
            userInput[i] = '\0';
            flag = 2;
            break;
        }
    }
    return flag;
}


/* Copies a string to a dynamically allocated string.
 @return:  returns the new dynamically allocated string with  the same data as the parameter string
*/
char *getString(char *data) {
    char *newStr = NULL;
    int size;

    if (data) {
        size = strlen(data) + 1;
        if (size == 1) { return NULL; }
        newStr = (char *)malloc(size * sizeof(char));
        assert(newStr != 0);
        strcpy(newStr, data);
    }
    return newStr;
}


/* Takes a string and breaks it down into tokens (with word spaces as a
 * a delimiter).  Each token is copied to an index of the commandLine's array
 * of tokens.
 * @params: cmd - the struct with the array of strings used to hold tokens
 *          user - the command line string provided by a user
 */
void parser(struct commandLine *cmd, char *user) {
    char *temp = NULL;
    char *overwrite = NULL;
    int index = 0;
    int setFile = 0; // 0 - no files, 1 - input file, 2 - output file
    int flag = 0;
    int pid;
    char pidStr[6];
    char pidName[200];
    
    memset(pidName, '\0', 200);
    cmd->cmdLine = (char **)malloc(CMD_TOKEN * sizeof(char *));
    assert(cmd->cmdLine != 0);
    for (int i = 0; i < CMD_TOKEN; ++i) {
        cmd->cmdLine[i] = NULL;
    }
    // parse command
    temp = strtok(user, " ");
    // parse arguments according to argument count
    do {
        // search and replace $$ with shell process id
        overwrite = strstr(temp, "$$");
        if (setFile == 1) {
            // find ">" symbol, then copy the input file
            cmd->inputFile = getString(temp);
            setFile = 0;
        } else if (setFile == 2) {
            // find ">" symbol, then copy the output file
            cmd->outputFile = getString(temp);
            setFile = 0;
        }
        if (overwrite) {
            // copy the temp into pidName and save the index
            strcpy(pidName, temp);
            overwrite = strstr(pidName, "$$");
            // get the shell process id and change it to a string
            pid = getpid();
            flag = snprintf(pidStr, 10, "%d", pid);
            if (flag) {
                strcpy(overwrite, pidStr);
            } 
            cmd->cmdLine[index] = getString(pidName);
        } else {
            cmd->cmdLine[index] = getString(temp);
        }
        // find "<" or ">" symbol, then parse the filename and copy it
        if (temp) {
            if (!strcmp(temp, "<")) {
                setFile = 1;
            }
            if (!strcmp(temp, ">")) {
                setFile = 2;
            }
        }
        temp = strtok(NULL, " ");
        ++index;
    } while (temp != NULL && index < CMD_TOKEN);
}


/* Receive user input as command line, and process the input for execution.
 * @precondition: user input only executes shell commands if its not a comment or a newline
                  user input cannot exceed 512 arguments( input redirection are not counted as arguments)
 * @postcondion:  The user command line is parsed and broken down into tokens, and it is process by other methods for shell execution
 */
void prompt(struct commandLine *cmd, int *exitValue, struct backgroundPID *bgPD) {
    bool  isBuiltIn = false;
    char user[LINE];
    int count = 0, flag = 0;
    char *builtInCmd[3] = { "cd", "exit", "status" };
    int index = 1;

    memset(user, '\0', LINE);
    do {
        // show prompt
        printf(": ");
        fflush(stdout);
        // receive user input
        fgets(user, LINE, stdin);
        // verify input
        flag = verifyInput(user);
    } while (flag == 0);
    // parse the input
    if (flag == 2 && foregroundMode == 0) { // verifyInput signaled a background command exist
        cmd->isBackground = true;
    }
    parser(cmd, user);
    // verify argument count
    while (cmd->cmdLine[index] && index < CMD_TOKEN) {
        if (strcmp(cmd->cmdLine[index], ">") == 0 || strcmp(cmd->cmdLine[index], "<") == 0) {
            break;
        }
        ++count;
        ++index;
    }
    cmd->argCount = count;
    if (count == ARG_LIMIT) {
        fprintf(stderr, "argument count is greater than 512 argument limit\n");
        exit(EXIT_FAILURE);
    }
    index = 0;
    // if it's built-in, send to built-in manager
    while ( index < 3 && !isBuiltIn) {
        if (!strcmp(cmd->cmdLine[0], builtInCmd[index])) {
            *exitValue = builtInManager(cmd, bgPD);
            isBuiltIn = true;
        }
        ++index;
    }
    if (!isBuiltIn) {
        // send to bash manager
       bashManager(cmd, bgPD);
    }
}


// Allocates memory for the commandLine struct and initializes its data members
struct commandLine * create() {
    struct commandLine *cmd = (struct commandLine *)malloc(sizeof(struct commandLine));
    cmd->cmdLine = NULL;
    cmd->isBackground = false;
    cmd->inputFile = NULL;
    cmd->outputFile = NULL;
    cmd->argCount = -5;
    //cmd->status = 0;
    return cmd;
}

