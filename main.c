//211749361

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#define ARG_SIZE (6) // arg[0] = name of command, arg[1, 2, 3, 4] = arguments, arg[5] space for NULL
#define INPUT_SIZE (1025)
#define ERROR_VALUE (-1)

// struct NodeList for aliases and its functions:
struct NodeList{
    char* alias;
    char* commend;
    struct NodeList* next;

} typedef NodeList;

// data struct to keep elements the functions use
struct Data {
    NodeList* alias_lst;
    int cmd_count,
            alias_count,
            script_lines_count,
            apostrophes_count;
    bool exit_flag,
            error_flag;
} typedef Data;

void free_lst(NodeList* head);
void free_node(NodeList** node);
NodeList* push(NodeList *head, char* name, char* commend, Data* data);
NodeList* delete_node(NodeList* head, char* name, Data* data);
void print_lst(NodeList* head);

// functions to run the terminal:
int word_to_arg(const char* input, int arg_ind, int* input_ind, char* args[]);
int input_to_arg(const char* input, char* arg[], Data* data);
void run_script_file(const char* file_name, char* arg[], Data* data);
int run_shell_command(char* arg[]);
void run_input_command(const char* input, Data* data);
void free_arg(char* arg[]);
void skip_spaces_tabs(const char* str, int *index);
#define MALLOC_ERR(element, re) if(element == NULL) { perror("malloc"); return re; }

int main() {
    char input[INPUT_SIZE];
    Data data = {NULL, 0, 0, 0, 0, false, false};
    while(true){
        printf("#cmd:%d|#alias:%d|#script lines:%d> ",
               data.cmd_count, data.alias_count, data.script_lines_count);
        if(fgets(input, INPUT_SIZE, stdin) == NULL){
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        run_input_command(input, &data);
        if(data.exit_flag){
            printf("%d\n", data.apostrophes_count);
            break;
        }
    }
    free_lst(data.alias_lst);
    return 0;
}

/**
 * 1. turn input into array of argument by other function.
 * 2. checks if it's alias/unalias/shell command and runs it accordingly.
 * @param input command from user / line from file.
 */
void run_input_command(const char* input, Data* data){
    data -> error_flag = false;
    char* arg[ARG_SIZE];
    for(int i = 0; i < ARG_SIZE; i++){
        arg[i] = NULL;
    }
    int apostrophes = input_to_arg(input, arg, data);
    if(data->exit_flag){
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
            print_lst(data->alias_lst);
            data->cmd_count++;
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
            data->alias_lst = push(data->alias_lst ,arg[1], arg[2], data);
            free_arg(arg);
            if(apostrophes > 0) data->apostrophes_count++;
            data->cmd_count++;
            return;
        }
        if(strcmp(arg[2], "=") != 0|| arg[3] == NULL || arg[4] != NULL){
            printf("ERR\n"); // no name for alias
            free_arg(arg);
            return;
        }
        data->alias_lst = push(data->alias_lst, arg[1], arg[3], data);
        free_arg(arg);
        if(apostrophes > 0) data->apostrophes_count++;
        data->cmd_count++;
        return;
    }
    if(strcmp(arg[0], "unalias") == 0){
        if(arg[1] == NULL || arg[2]!= NULL){
            printf("ERR\n"); // no name for alias
            free_arg(arg);
            return;
        }
        data->alias_lst = delete_node(data->alias_lst, arg[1], data);
        free_arg(arg);
        if(!data->error_flag){
            if(apostrophes > 0) data->apostrophes_count++;
            data->cmd_count++;
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
        run_script_file(arg[1], arg, data);
        free_arg(arg);
        if(!data->error_flag) {
            if(apostrophes > 0) data->apostrophes_count++;
            data->cmd_count++;
        }
        return;
    }
    int status = run_shell_command(arg);
    if (status == 0) {
        data->cmd_count++;
        if(apostrophes > 0) data->apostrophes_count++;
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
                return ERROR_VALUE;
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
int input_to_arg(const char input[], char* arg[], Data* data){
    int input_ind = 0, arg_ind = 0;
    int apostrophe_update = word_to_arg(input, arg_ind, &input_ind, arg);
    if(apostrophe_update < 0){
        return ERROR_VALUE;
    }
    if(arg[arg_ind] == NULL) return ERROR_VALUE;
    if(strcmp(arg[arg_ind], "exit_shell") == 0){
        data->exit_flag = true;
        return 0;
    }
    if(strcmp(arg[arg_ind], "alias") == 0 || strcmp(arg[arg_ind], "unalias") == 0){
        while(arg[arg_ind] != NULL){
            if(arg_ind >= ARG_SIZE - 1){
                return ERROR_VALUE;
            }
            arg_ind++;
            int AU = word_to_arg(input, arg_ind, &input_ind, arg);
            if(AU < 0){
                return ERROR_VALUE;
            }
            apostrophe_update += AU;
        }
        return apostrophe_update;
    }
    while(arg[arg_ind] != NULL){
        if(arg_ind >= ARG_SIZE - 1){
            return ERROR_VALUE;
        }
        NodeList *temp = data->alias_lst;
        while(temp != NULL){
            if(strcmp(temp->alias, arg[arg_ind]) == 0){
                int command_ind = 0;
                int AU = word_to_arg(temp->commend, arg_ind, &command_ind, arg);
                if(AU < 0){
                    return ERROR_VALUE;
                }
                apostrophe_update+=AU;
                while(arg[arg_ind] != NULL){
                    if(arg_ind >= ARG_SIZE - 1){
                        return ERROR_VALUE;
                    }
                    arg_ind++;
                    AU = word_to_arg(temp->commend, arg_ind, &command_ind, arg);
                    if(AU < 0){
                        return ERROR_VALUE;
                    }
                    apostrophe_update += AU;
                }
                arg_ind--;
            }
            temp = temp->next;
        }
        arg_ind++;
        int AU = word_to_arg(input, arg_ind, &input_ind, arg);
        if(AU < 0) {
            return ERROR_VALUE;
        }
        apostrophe_update += AU;
    }
    return apostrophe_update;
}

/**
 * go over file and runs evey line as a command.
 * skips empty lines and comment lines(#).
 */
void run_script_file(const char* file_name, char* arg[], Data* data){
    FILE *fp = fopen(file_name, "r");
    if(fp == NULL){
        data->error_flag = true;
        perror("Error opening file");
        return;
    }
    free_arg(arg);
    //check if first there is #!/bin/bash
    char command[INPUT_SIZE];
    if(fgets(command, INPUT_SIZE, fp) == NULL || strcmp(command, "#!/bin/bash\n") != 0){
        printf("ERR\n");
        data->error_flag = true;
        return;
    }
    data->script_lines_count ++; // for the first line of bash command
    while(fgets(command, INPUT_SIZE, fp) != NULL){
        data->script_lines_count++;
        command[strcspn(command, "\n")] = '\0';
        int i = 0;
        skip_spaces_tabs(command, &i);
        if (command[i] != '\0' && command[i] != '#') {
            run_input_command(command, data);
        }
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
        return ERROR_VALUE;
    } else if (pid > 0) { // parent
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            // Child process did not terminate normally
            return ERROR_VALUE;
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
    if(node == NULL) return;
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
NodeList* push(NodeList *head, char* name, char* commend, Data* data){
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

    data->alias_count++;
    return new_node;
}

/**
 * Delete specific node by finding its name.
 * If name doesnt exist: The list will stay the same, and print ERR.
 * @return the head of the list.
 */
NodeList* delete_node(NodeList* head, char* name, Data* data){
    if(head == NULL){

        data->error_flag = true;
        return NULL;
    }
    if(strcmp(head->alias, name) == 0){
        NodeList *temp = head->next;
        free_node(&head);
        data->alias_count--;
        return temp;
    }
    NodeList *del = NULL;
    while(head->next != NULL){
        if(strcmp(head->next->alias, name) == 0){
            del = head->next;
            head->next = head->next->next;
            free_node(&del);
            data->alias_count--;
            return head;
        }
        head = head->next;
    }
    data->error_flag = true;
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
