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


int verifyInput(char *userInput);
void bashManager();
int builtInManager(struct commandLine *cmd);
void parser(struct commandLine *cmd, char *user);
char *getString(char *data);
void destroy(struct commandLine *cmd);
struct commandLine * create();
void prompt(struct commandLine *cmd, int *exitValue);
void directoryCmd(struct commandLine *cmd);

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
    int exitValue = 1;

    do {
        line = create();
        prompt(line, &exitValue);
     /**   print(line);  **/
        destroy(line);
    } while (exitValue != 0);
    return 0;
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


/*
 *
 */
void bashManager() {
    printf("Hi I'm Bash Manager\n");
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
int builtInManager(struct commandLine *cmd) {
    int flag = 1;
    if (!strcmp(cmd->cmdLine[0], "cd")) {
        directoryCmd(cmd);
    } else if (!strcmp(cmd->cmdLine[0], "status")) {
        printf("exit value %d\n", cmd->status);
        fflush(stdout);
    } else {
        // use exit command
        //  Kill all processes and jobs before exiting
        flag = 0;
    }
    return flag;
}


/*  Verifies that the input does not match # or only a newline, and this 
 *  function removes the new line character from a user input
 *  @return: 0 - the command line consist of "#" as first character or its only a newline character
 *           1 - the command line is not a new line or a comment
 */
int verifyInput( char *userInput) {
    int flag = 1;
    char symbol= '#';
    char format = '\n';
    int size = -1;

    // remove the newline character at the end of user input string
    size = strlen(userInput);
    for (int i = size; i >= 0; --i) {
        if (userInput[i] == '\n') {
            userInput[i] = '\0';
        }
    }
    //  verify that command line is not a new line or a comment #
    if (userInput[0] == symbol || (size == 1 && userInput[0] == format)) {
        flag = 0;
    }
    return flag;
}


/* Copies a string to a dynamically allocated string.
 @return:  returns the new dynamically allocated string with  the same data as the parameter string
*/
char *getString(char *data) {
    char *newStr = NULL;
    int size = strlen(data) + 1;

    if (size == 1) { return NULL; }
    newStr = (char *)malloc(size * sizeof(char));
    assert(newStr != 0);
    strcpy(newStr, data);
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
    int index = 0;
    int setFile = 0; // 0 - no files, 1 - input file, 2 - output file
    
    cmd->cmdLine = (char **)malloc(CMD_TOKEN * sizeof(char *));
    assert(cmd->cmdLine != 0);
    for (int i = 0; i < CMD_TOKEN; ++i) {
        cmd->cmdLine[i] = NULL;
    }
    // parse command
    temp = strtok(user, " ");
    // parse arguments according to argument count
    do {
        if (setFile == 1) {
            // find ">" symbol, then copy the input file
            cmd->inputFile = getString(temp);
            setFile = 0;
        } else if (setFile == 2) {
            // find ">" symbol, then copy the output file
            cmd->outputFile = getString(temp);
            setFile = 0;
        }
        cmd->cmdLine[index] = getString(temp);
        // find "<" symbol, then parse the filename and copy it
        if (!strcmp(temp, "<")) {
            setFile = 1;
        }
        if (!strcmp(temp, ">")) {
            setFile = 2;
        } 
        if (!strcmp(temp, "&")) {
           // find "&", change isBackground to true
           cmd->isBackground = true;
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
void prompt(struct commandLine *cmd, int *exitValue) {
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
            *exitValue = builtInManager(cmd);
            isBuiltIn = true;
        }
        ++index;
    }
    if (!isBuiltIn) {
        // send to bash manager
       bashManager();
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
    cmd->status = -1;
    return cmd;
}

