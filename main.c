//211749361

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#include <signal.h>
#define ARG_SIZE 6 // arg[0] = name of command, arg[1, 2, 3, 4] = arguments, arg[5] space for NULL
#define INPUT_SIZE 1025

// struct NodeList for aliases and its functions:
struct NodeList{
    char* alias;
    char* commend;
    struct NodeList* next;

} typedef NodeList;
void free_lst(NodeList* head);
void free_node(NodeList** node);
NodeList* push(NodeList *head, char* name, char* commend);
NodeList* delete_node(NodeList* head, char* name);
void print_lst(NodeList* head);

// functions to run the terminal:
int input_to_arg(const char* input, char* arg[]);
void run_script_file(const char* file_name, char* arg[]);
int run_shell_command(char* arg[]);
void run_input_command(const char* input);
void free_arg(char* arg[]);

// helper functions:
#define CHECK_FILE_NULL(fp) if(fp == NULL){ perror("Error opening file"); return; }
#define MALLOC_ERR(element, re) if(element == NULL) { perror("malloc"); return re; }
void skip_spaces_tabs(const char* str, int *index);

//static elements:
NodeList* alias_lst = NULL;
int cmd_count = 0, alias_count = 0, script_lines_count = 0, apostrophes_count = 0, EXIT_FLAG = 0, ERROR_FLAG;

int main() {
    char input[INPUT_SIZE];
    while(true){
        printf("#cmd:%d|#alias:%d|#script lines:%d> ", cmd_count, alias_count, script_lines_count);
        if(fgets(input, INPUT_SIZE, stdin) == NULL){
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        run_input_command(input);
        if(EXIT_FLAG != 0){
            printf("%d\n", apostrophes_count);
            break;
        }
    }
    free_lst(alias_lst);
    return 0;
}

/**
 * 1. turn input into array of argument by other function.
 * 2. checks if it's alias/unalias/shell command and runs it accordingly.
 * @param input command from user / line from file.
 */
void run_input_command(const char* input){
    ERROR_FLAG = 0;
    char* arg[ARG_SIZE];
    for(int i = 0; i < ARG_SIZE; i++){
        arg[i] = NULL;
    }
    int apostrophes = input_to_arg(input, arg);
    if(EXIT_FLAG != 0){
        free_arg(arg);
        return;
    }
    if(apostrophes < 0) {
        printf("ERR\n");
        free_arg(arg);
        return;
    }
    if(arg[0] == NULL){
        printf("ERR\n");
        free_arg(arg);
        return;
    }

    if(strcmp(arg[0], "alias") == 0){
        if(arg[1] == NULL){
            print_lst(alias_lst);
            cmd_count++;
            free_arg(arg);
            return;
        }
        if(arg[2] == NULL){
            printf("ERR\n"); // invalid alias
            free_arg(arg);
            return;
        }
        if(arg[1][(int) strlen(arg[1]) - 1] == '='){
            if(arg[3] != NULL){
                printf("ERR\n"); // invalid alias
                free_arg(arg);
                return;
            }
            arg[1][(int) strlen(arg[1]) - 1] = '\0';
            if(strlen(arg[1]) == 0){
                printf("ERR\n"); // no name for alias
                free_arg(arg);
                return;
            }
            alias_lst = push(alias_lst ,arg[1], arg[2]);
            free_arg(arg);
            apostrophes_count += apostrophes;
            cmd_count++;
            return;
        }
        if(strcmp(arg[2], "=") != 0|| arg[3] == NULL || arg[4] != NULL){
            printf("ERR\n"); // no name for alias
            free_arg(arg);
            return;
        }
        alias_lst = push(alias_lst, arg[1], arg[3]);
        free_arg(arg);
        apostrophes_count += apostrophes;
        cmd_count++;
        return;
    }
    if(strcmp(arg[0], "unalias") == 0){
        if(arg[1] == NULL || arg[2]!= NULL){
            printf("ERR\n"); // no name for alias
            free_arg(arg);
            return;
        }
        alias_lst = delete_node(alias_lst, arg[1]);
        free_arg(arg);
        if(ERROR_FLAG == 0){
            cmd_count++;
            return;
        }
        printf("ERR\n");
        return;
    }
    if(strcmp(arg[0], "source") == 0){
        if(arg[1] == NULL || arg[2] != NULL){
            printf("ERR\n");
            free_arg(arg);
            return;
        }
        int name_len = (int)strlen(arg[1]);
        // check if file is .sh
        if(name_len < 4 || arg[1][name_len - 3] != '.' || arg[1][name_len - 2] != 's' || arg[1][name_len - 1] != 'h') {
            printf("ERR\n");
            free_arg(arg);
            return;
        }
        run_script_file(arg[1], arg);
        free_arg(arg);
        cmd_count++;
        return;
    }
    int status = run_shell_command(arg);
    if (status == 0) {
        cmd_count++;
        apostrophes_count += apostrophes;
    }
    free_arg(arg);
}

/**
 * Find one argument and put it in args[arg_ind];
 * @param input command from user / line from file.
 * @param arg_ind the place in th array to put the argument in.
 * @param input_ind where to start reading the input.
 * @return  if the argument was an apostrophe type, if invalid apostrophe -> returns -1;
 */
int word_to_arg(const char* input, int arg_ind, int* input_ind, char* args[]){
    if(args[arg_ind] != NULL){
        free(args[arg_ind]);
        args[arg_ind] = NULL;
    }
    skip_spaces_tabs(input, input_ind);
    if(input[*input_ind] == '\0') return 0;
    int start = *input_ind, arg_len = 1;
    if(input[*input_ind] == '\'' || input[*input_ind] == '"'){
        char type = input[(*input_ind)++];
        start++;
        while(input[*input_ind] != type){
            if(input[*input_ind] == '\0'){
                return -1;
            }
            (*input_ind)++, arg_len++;
        }
        args[arg_ind] = (char *) malloc(arg_len);
        for(int i = 0;i < arg_len-1; i++, start++){
            args[arg_ind][i] = input[start];
        }
        args[arg_ind][arg_len - 1] = '\0';
        (*input_ind)++;
        return 1;
    }
    while(input[*input_ind] != '\0' && input[*input_ind] != ' ' && input[*input_ind] != '\t' && input[*input_ind] != '\'' && input[*input_ind] != '"'){
        arg_len++, (*input_ind)++;
    }
    args[arg_ind] = (char *) malloc(arg_len);
    for(int i = 0; i < arg_len - 1; i++, start++){
        args[arg_ind][i] = input[start];
    }
    args[arg_ind][arg_len - 1] = '\0';
    return 0;
}

/**
 * turns input into an array of arguments using word_to_arg function.
 * @return how many argument are apostrophe type, if invalid apostrophe -> returns -1;
 */
int input_to_arg(const char input[], char* arg[]){
    int input_ind = 0, arg_ind = 0;
    int apostrophe_update = word_to_arg(input, arg_ind, &input_ind, arg);
    if(apostrophe_update < 0){
        return -1;
    }
    if(arg[arg_ind] == NULL) return -1;
    if(strcmp(arg[arg_ind], "exit_shell") == 0){
        EXIT_FLAG++;
        return 0;
    }
    if(strcmp(arg[arg_ind], "alias") == 0 || strcmp(arg[arg_ind], "unalias") == 0){
        while(arg[arg_ind] != NULL){
            if(arg_ind >= ARG_SIZE - 1){
                return -1;
            }
            arg_ind++;
            int AU = word_to_arg(input, arg_ind, &input_ind, arg);
            if(AU < 0){
                return -1;
            }
            apostrophe_update += AU;
        }
        return apostrophe_update;
    }
    while(arg[arg_ind] != NULL){
        if(arg_ind >= ARG_SIZE - 1){
            return -1;
        }
        NodeList *temp = alias_lst;
        while(temp != NULL){
            if(strcmp(temp->alias, arg[arg_ind]) == 0){
                int command_ind = 0;
                int AU = word_to_arg(temp->commend, arg_ind, &command_ind, arg);
                if(AU < 0){
                    return - 1;
                }
                apostrophe_update+=AU;
                while(arg[arg_ind] != NULL){
                    if(arg_ind >= ARG_SIZE - 1){
                        return - 1;
                    }
                    arg_ind++;
                    AU = word_to_arg(temp->commend, arg_ind, &command_ind, arg);
                    if(AU < 0){
                        return - 1;
                    }
                    apostrophe_update += AU;
                }
                arg_ind--;
            }
            temp = temp->next;
        }
        arg_ind++;
        int AU = word_to_arg(input, arg_ind, &input_ind, arg);
        if(AU < 0){
            return -1;
        }
        apostrophe_update += AU;
    }
    return apostrophe_update;
}

/**
 * go over file and runs evey line as a command.
 * skips emty lines and comment lines(#).
 */
void run_script_file(const char* file_name, char* arg[]){
    FILE *fp = fopen(file_name, "r");
    CHECK_FILE_NULL(fp)
    free_arg(arg);
    char command[INPUT_SIZE];
    while(fgets(command, INPUT_SIZE, fp) != NULL){
        command[strcspn(command, "\n")] = '\0';
        int i = 0;
        skip_spaces_tabs(command, &i);
        if (command[i] == '\0' || command[i] == '#') {
            continue;
        }
        script_lines_count++;
        run_input_command(command);

    }
    free_arg(arg);
    fclose(fp);
}

/**
 * uses fork and args to run the command.
 * @return status: 0 for success, otherwise - error.
 */
int run_shell_command(char* arg[]){
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork"); // fork error
        return -1;
    } else if (pid > 0) { // parent
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            // Child process did not terminate normally
            return -1;
        }
    } else { // child
        execvp(arg[0], arg);
        perror("exec");
        _exit(EXIT_FAILURE);
    }
}

void free_arg(char* arg[]){
    for(int i = 0; i < ARG_SIZE; i++){
        if(arg[i] != NULL) {
            free(arg[i]);
            arg[i] = NULL;
        }
    }
}

void free_node(NodeList** node){
    // free one node in the list
    if(node == NULL)
        return;
    free((*node)->alias);
    free((*node)->commend);
    (*node)->alias = NULL;
    (*node)->commend = NULL;
    free((*node));
    (*node) = NULL;
}

/**
 * insert new node to the list, if name exits runs over the surrent command with the new one.
 * @param name the shortcut the user want to use for the command.
 * @return the new head of the list.
 */
NodeList* push(NodeList *head, char* name, char* commend){
    NodeList* temp = head;
    while (temp != NULL) {
        if (strcmp(name, temp->alias) == 0) {
            free(temp->commend);
            temp->commend = strdup(commend);  // Alias already exists, run over the old command
            return head;
        }
        temp = temp->next;
    }

    // Create a new node
    NodeList* new_node = (NodeList*)malloc(sizeof(NodeList));
    MALLOC_ERR(new_node, head)

    // Initialize the new node
    new_node->alias = strdup(name);
    new_node->commend = strdup(commend);
    new_node->next = head;

    alias_count++;
    return new_node;
}

/**
 * Delete specific node by finding its name.
 * If name doesnt exist: The list will stay the same, and print ERR.
 * @return the head of the list.
 */
NodeList* delete_node(NodeList* head, char* name){
    if(head == NULL){
        ERROR_FLAG++;
        return NULL;
    }
    if(strcmp(head->alias, name) == 0){
        NodeList *temp = head->next;
        free_node(&head);
        alias_count--;
        return temp;
    }
    NodeList *del = NULL;
    while(head->next != NULL){
        if(strcmp(head->next->alias, name) == 0){
            del = head->next;
            head->next = head->next->next;
            free_node(&del);
            alias_count--;
            return head;
        }
        head = head->next;
    }
    ERROR_FLAG++;
    return head;
}

void print_lst(NodeList* head){
    while (head != NULL && head->alias != NULL && head->commend != NULL){
        printf("%s='%s'\n", head->alias, head->commend);
        head = head->next;
    }
}

void free_lst(NodeList* head){
    while(head != NULL){
        NodeList* temp = head->next;
        free_node(&head);
        head = NULL;
        head = temp;
    }
}

/**
 * skips spaces and tabs from str, starting by index, changes the index to be after the spaces/tabs.
 */
void skip_spaces_tabs(const char* str, int *index){
    while(str[*index] == ' ' || str[*index] == '\t'){
        (*index)++;
    }
}
