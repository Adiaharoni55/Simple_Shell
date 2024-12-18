// 211749361
// Adi Aharoni

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>
#define ARG_SIZE (6) // arg[0] = name of command, arg[1, 2, 3, 4] = arguments, arg[5] space for NULL
#define INPUT_SIZE (1025)

// struct NodeList for aliases and its functions:
struct NodeList{
    char* alias;
    char* command;
    struct NodeList* next;

} typedef NodeList;

// struct Jobs:
struct Jobs{
    pid_t pid;
    int count;
    char command[INPUT_SIZE];
    struct Jobs *next;
} typedef Jobs;

// data struct to keep elements the functions use
struct Data {
    NodeList* alias_lst;
    int cmd_count,
            alias_count,
            script_lines_count,
            apostrophes_count;
    bool error_flag,
            exit_flag,
            wait_flag,
            is_quotes,
            error_to_file;
    // wait_status;
    FILE* error_file;
    char* multiple_commands;
} typedef Data;

// functions for struct Jobs:
void delete_job(pid_t pid);
void add_job(pid_t pid, char* command);
void print_jobs(Jobs* jobs_head);
void free_jobs();

// functions for struct NodeLIst:
void free_lst(NodeList* head);
void free_node(NodeList** node);
NodeList* push(NodeList *head, char* name, char* command, Data* data, char* args[]);
NodeList* delete_node(NodeList* head, char* name, Data* data);
void print_lst(NodeList* head);

// functions to run the terminal:
void print_prompt(Data* data);
void word_to_arg(const char* input, int arg_ind, int* input_ind, char* args[], Data* data);
void input_to_arg(const char* input, char* args[], Data* data, int *input_ind, int *arg_ind);
void run_script_file(const char* file_name, char* args[], Data* data);
void run_error_to_file_command(const char * file_name, char* args[], Data *data);
void run_shell_command(char* args[], Data* data);
void run_input_command(const char* input, Data* data, int start_index_input);
void free_arg(char* args[]);
void skip_spaces_tabs(const char* str, int *index);
void run_arg_command(char* args[], Data* data);
char* arg_into_str(char* args[]);
#define ASSERT_MALLOC(condition) if(!(condition)) { perror("malloc"); free_lst(data->alias_lst); free_jobs(); free_arg(args); exit(EXIT_FAILURE); }
#define ASSERT_AND_FREE(condition) if (!(condition)) { data->error_flag = true; if(data->error_to_file){fprintf(data->error_file, "ERR\n");} \
else{printf("ERR\n");} free_arg(args); return; }

// global elements so could change by signal:
Jobs* jobs = NULL;
int job_count = 1;
Data *global_data = NULL;

void sigchld_handler();

int main() {
    char input[INPUT_SIZE];
    Data data = {NULL, 0, 0, 0, 0,
                 false, false, false, false, false,NULL, NULL};
    struct sigaction sa;
    sa.sa_sigaction = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    while (true) {
        sigaction(SIGCHLD, &sa, NULL);
        print_prompt(&data);
        if (fgets(input, INPUT_SIZE, stdin) == NULL) {
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        if (strlen(input) == 0) continue;
        run_input_command(input, &data, 0);
        if (data.exit_flag) {
            printf("%d", data.apostrophes_count);
            break;
        }
    }
    free_lst(data.alias_lst);
    free_jobs();
    return 0;
}

void sigchld_handler() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            global_data->cmd_count--;
            global_data->error_flag = true;
        }
        else {
            if (global_data->is_quotes && !global_data->error_flag) {
                global_data->apostrophes_count++;
            }
        }
        delete_job(pid);
    }
}

void print_prompt(Data* data){
    printf("#cmd:%d|#alias:%d|#script lines:%d> ", data->cmd_count, data->alias_count, data->script_lines_count);
    fflush(stdout);
}


void run_input_command(const char* input, Data* data, int start_index_input){
    data->multiple_commands = NULL;
    data->error_flag = false;
    data->wait_flag = true;
    data->is_quotes = false;
    char* args[ARG_SIZE];
    for(int i = 0; i < ARG_SIZE; i++){
        args[i] = NULL;
    }
    int args_ind = 0;
    input_to_arg(input, args, data, &start_index_input, &args_ind);
    if(data->error_flag && (!data->error_to_file || data->error_file != NULL)){
        if(data->error_to_file){fprintf(data->error_file, "ERR\n");}
        else{printf("ERR\n");}
    }
    else if(data->exit_flag){
        free_arg(args);
        return;
    }
    if(data->multiple_commands == NULL) {
        if(args[args_ind] == NULL) {
            free_arg(args);
            return;
        }
        if(strcmp(args[args_ind], "&") == 0){
            free(args[args_ind]);
            args[args_ind] = NULL;
            data->wait_flag = false;
        }
        else if(args[args_ind][strlen(args[args_ind]) - 1] == '&'){
            args[args_ind][strlen(args[args_ind]) - 1] = '\0';
            data->wait_flag = false;
        }
        if(data->error_to_file && data->error_file == NULL){
            free(args[args_ind]);
            args[args_ind] = NULL;
            word_to_arg(input, args_ind, &start_index_input, args, data);
            if(args[args_ind] == NULL){
                free_arg(args);
                return;
            }
            char *file_name = strdup(args[args_ind]);
            free(args[args_ind]);
            args[args_ind] = NULL;
            args_ind--;
            run_error_to_file_command(file_name, args, data);
            free(file_name);
            data->error_to_file = false;
            free_arg(args);
            return;
        }
        run_arg_command(args, data);
        free_arg(args);
        return;
    }
    free(args[args_ind]);
    args[args_ind--] = NULL;

    if(args[args_ind] != NULL && args_ind >= 0){
        if(strcmp(args[args_ind], "&") == 0){
            free(args[args_ind]);
            args[args_ind] = NULL;
            data->wait_flag = false;
        }
        else if(args[args_ind][strlen(args[args_ind]) - 1] == '&'){
            args[args_ind][strlen(args[args_ind]) - 1] = '\0';
            data->wait_flag = false;
        }
        run_arg_command(args, data);
    }
    free_arg(args);
    args_ind = 0;
    if((strcmp(data->multiple_commands, "&&") == 0 && data->error_flag)
       || (strcmp(data->multiple_commands, "||") == 0 && !(data->error_flag))
       || input[start_index_input] == '\0'){
//        if(strcmp(data->multiple_commands, "||") == 0){ // check if there's a && command after || command:
//            word_to_arg(input, args_ind, &start_index_input, args, data);
//            while(args[0] != NULL){
//                if(data->error_flag){
//                    break;
//                }
//                else if(strcmp(args[0], "&&") == 0){
//                    data->multiple_commands = strdup(args[0]);
//                    free_arg(args);
//                    return run_input_command(input, data, start_index_input);
//                }
//                free(args[0]);
//                args[0] = NULL;
//                word_to_arg(input, args_ind, &start_index_input, args, data);
//            }
//        }
//        else if(strcmp(data->multiple_commands, "&&") == 0){ // check if there's an || after a wrong &&:
//            data->error_flag = false;
//            word_to_arg(input, args_ind, &start_index_input, args, data);
//            while(args[0] != NULL){
//                if(data->error_flag){
//                    break;
//                }
//                else if(strcmp(args[0], "||") == 0){
//                    data->multiple_commands = strdup(args[0]);
//                    free_arg(args);
//                    return run_input_command(input, data, start_index_input);
//                }
//                free(args[0]);
//                args[0] = NULL;
//                word_to_arg(input, args_ind, &start_index_input, args, data);
//            }
//        }

        free(data->multiple_commands);
        data->multiple_commands = NULL;
        free_arg(args);
        return;
    }
    free(data->multiple_commands);
    data->multiple_commands = NULL;
    free_arg(args);
    return run_input_command(input, data, start_index_input);
}

/**
 * 1. turn input into array of argument by other function.
 * 2. checks if it's alias/unalias/shell command and runs it accordingly.
 * @param input command from user / line from file.
 */
void run_arg_command(char* args[], Data* data){
    if(strcmp(args[0], "alias") == 0){
        if(args[1] == NULL){
            print_lst(data->alias_lst);
            data->cmd_count++;
            return;
        }
        ASSERT_AND_FREE(args[2]!=NULL)
        if(args[1][(int) strlen(args[1]) - 1] == '='){
            ASSERT_AND_FREE(args[3] == NULL)
            args[1][(int) strlen(args[1]) - 1] = '\0';
            ASSERT_AND_FREE(strlen(args[1]) != 0)
            data->alias_lst = push(data->alias_lst ,args[1], args[2], data, args);
            free_arg(args);
            if(data->is_quotes) data->apostrophes_count++;
            data->cmd_count++;
            return;
        }
        ASSERT_AND_FREE(strcmp(args[2], "=") == 0 && args[3] != NULL && args[4] == NULL)
        data->alias_lst = push(data->alias_lst, args[1], args[3], data, args);
        free_arg(args);
        if(data->is_quotes) data->apostrophes_count++;
        data->cmd_count++;
        return;
    }
    if(strcmp(args[0], "unalias") == 0){
        ASSERT_AND_FREE(args[1] != NULL && args[2] == NULL)
        data->alias_lst = delete_node(data->alias_lst, args[1], data);
        free_arg(args);
        if(!data->error_flag){
            if(data->is_quotes) data->apostrophes_count++;
            data->cmd_count++;
            return;
        }
        if(data->error_to_file){ fprintf(data->error_file, "ERR\n");}
        else{printf("ERR\n");}
        return;
    }
    if(strcmp(args[0], "source") == 0){
        ASSERT_AND_FREE(args[1] != NULL && args[2] == NULL)
        int name_len = (int)strlen(args[1]);
        ASSERT_AND_FREE(name_len > 3 && args[1][name_len - 3] == '.' && args[1][name_len - 2] == 's' && args[1][name_len - 1] == 'h')
        run_script_file(args[1], args, data);
        free_arg(args);
        if(!data->error_flag) {
            if(data->is_quotes) data->apostrophes_count++;
            data->cmd_count++;
        }
        return;
    }
    if(strcmp(args[0], "jobs") == 0){
        ASSERT_AND_FREE(args[1] == NULL)
        print_jobs(jobs);
        data->cmd_count++;
        return;
    }
    if(strcmp(args[0], "cd") == 0){
        ASSERT_AND_FREE(args[1] != NULL)
        if (chdir(args[1]) == -1) {
            if(data->error_to_file) {
                fprintf(data->error_file, "cd: %s\n", strerror(errno));
            }
            else{
                perror("cd");
            }
            return;
        }
        data->cmd_count++;
        return;
    }
    run_shell_command(args, data);
}

void run_error_to_file_command(const char * file_name ,char *args[], Data*data){
    if(file_name == NULL){
        printf("ERR\n");
        return;
    }
    if(args[0] == NULL){
        return;
    }
    data->error_file = fopen(file_name, "w");
    if(data->error_file == NULL){
        perror("error opening file");
        return;
    }
    char*command = NULL;
    if(data->error_flag){
        fprintf(data->error_file, "ERR\n");
        fclose(data->error_file);
        data->error_file = NULL;
        return;
    }
    if(args[0][0] == '('){
        int i = 1, size = 0;
        for(; i < strlen(args[0]) - 1; i++, size++);
        int j = 0;
        command = (char*) malloc(size + 1);
        for(i = 1; j < size; j++, i++){
            command[j] = args[0][i];
        }
        command[size] = '\0';
        free_arg(args);
        run_input_command(command, data, 0);
    }
    else{
        run_shell_command(args, data);
    }

    fclose(data->error_file);
    data->error_file = NULL;
    free(command);
    command = NULL;
}
/**
 * Find one argument and put it in args[arg_ind];
 * @param input command from user / line from file.
 * @param arg_ind the place in th array to put the argument in.
 * @param input_ind where to start reading the input.
 * @return  if the argument was an apostrophe type. if invalid apostrophe: raises error flag;
 */
void word_to_arg(const char* input, int arg_ind, int* input_ind, char* args[], Data* data) {
    if (args[arg_ind] != NULL) {
        free(args[arg_ind]);
        args[arg_ind] = NULL;
    }
    skip_spaces_tabs(input, input_ind);
    if (input[*input_ind] == '\0') return;
    int start = *input_ind, arg_len = 1;
    if (arg_ind == 0) {// finding the name of command
        bool brackets = (input[*input_ind] == '(');
        while (input[*input_ind] != '\0' && input[*input_ind] != ' ' && input[*input_ind] != '\t') {
            if (input[*input_ind] == '\'' || input[*input_ind] == '"' || brackets) {
                char type;
                if(brackets)
                    type = ')';
                else
                    type = input[(*input_ind)];
                (*input_ind)++, arg_len++;
                if (input[*input_ind] == '\0') {
                    data->error_flag = true;
                    return;
                }
                while (input[*input_ind] != type) {
                    if (input[*input_ind] == '\0') {
                        data->error_flag = true;
                        return;
                    }
                    (*input_ind)++, arg_len++;
                }
                (*input_ind)++, arg_len++;
                if(!brackets) {
                    data->is_quotes = true;
                }
                else{
                    break;
                }
            } else {
                (*input_ind)++, arg_len++;
            }
        }
        if (data->is_quotes && (input[start] == '\'' || input[start] == '"') &&
            input[start] == input[(*input_ind) - 1]) {
            start++, arg_len -= 2;
        }
    } else if (input[*input_ind] == '\'' || input[*input_ind] == '"') {
        char type = input[(*input_ind)++];
        start++;
        if (input[*input_ind] == '\0') {
            data->error_flag = true;
            return;
        }
        while (input[*input_ind] != type) {
            if (input[*input_ind] == '\0') {
                data->error_flag = true;
                return;
            }
            (*input_ind)++, arg_len++;
        }
        data->is_quotes = true;
    } else {
        while (input[*input_ind] != '\0' && input[*input_ind] != ' ' && input[*input_ind] != '\t' &&
               input[*input_ind] != '\'' && input[*input_ind] != '"') {
            arg_len++, (*input_ind)++;
        }
    }
    args[arg_ind] = (char *) malloc(arg_len);
    ASSERT_MALLOC(args[arg_ind] != NULL)
    for (int i = 0; i < arg_len - 1; i++, start++) {
        args[arg_ind][i] = input[start];
    }
    args[arg_ind][arg_len - 1] = '\0';
    if (strcmp(args[arg_ind], "&&") == 0 || strcmp(args[arg_ind], "||") == 0) {
        data->multiple_commands = strdup(args[arg_ind]);
    }
    else if(strcmp(args[arg_ind], "2>") == 0){
        data->error_to_file = true;
    }
    if (data->is_quotes) {
        (*input_ind)++;
    }
}

/**
 * turns input into an array of arguments using word_to_arg function.
 * @return how many argument are apostrophe type, if invalid apostrophe -> returns -1;
 */
void input_to_arg(const char input[], char* args[], Data* data, int* input_ind, int*arg_ind){
    word_to_arg(input, *arg_ind, input_ind, args, data);
    if(args[*arg_ind] == NULL || data->error_flag || data->multiple_commands != NULL || (data->error_to_file && data->error_file == NULL)){
        return;
    }
    if(strcmp(args[*arg_ind], "exit_shell") == 0){
        data->exit_flag = true;
        return;
    }
    NodeList *temp = data->alias_lst;
    while(temp != NULL) {
        if (strcmp(temp->alias, args[*arg_ind]) == 0) {
            int command_ind = 0;
            word_to_arg(temp->command, *arg_ind, &command_ind, args, data);
            if(data->error_flag || data->multiple_commands != NULL || (data->error_to_file && data->error_file == NULL)){
                return;
            }
            while (args[*arg_ind] != NULL) {
                if (*arg_ind >= ARG_SIZE - 1) {
                    data->error_flag = true;
                    // find if there is an or command that need to be executed after the error.
                    *arg_ind = 0;
                    while(args[*arg_ind] != NULL){
                        free_arg(args);
                        word_to_arg(input, *arg_ind, input_ind, args, data);
                        if(data->multiple_commands != NULL || (data->error_to_file && data->error_file == NULL)){
                            return;
                        }
                    }
                    return;
                }
                (*arg_ind)++;
                word_to_arg(temp->command, *arg_ind, &command_ind, args, data);
                if(data->multiple_commands != NULL || data->error_flag || (data->error_to_file && data->error_file == NULL)){
                    return;
                }
            }
            (*arg_ind)--;
        }
        temp = temp->next;
    }
    while(args[*arg_ind] != NULL){
        if(*arg_ind >= ARG_SIZE - 1){
            data->error_flag = true;
            // find if there is an or command that need to be executed after the error.
            *arg_ind = 0;
            while(args[*arg_ind] != NULL){
                free_arg(args);
                word_to_arg(input, *arg_ind, input_ind, args, data);
                if(data->multiple_commands != NULL || (data->error_to_file && data->error_file == NULL)){
                    return;
                }
            }
            return;
        }
        (*arg_ind)++;
        word_to_arg(input, *arg_ind, input_ind, args, data);
        if(data->multiple_commands != NULL || data->error_flag || (data->error_to_file && data->error_file == NULL)){
            return;
        }
    }
    (*arg_ind)--;
}

/**
 * go over file and runs evey line as a command.
 * skips empty lines and comment lines(#).
 */
void run_script_file(const char* file_name, char* args[], Data* data){
    FILE *fp = fopen(file_name, "r");
    free_arg(args);
    if(fp == NULL){
        if(data->error_to_file){
            fprintf(data->error_file, "error opening file: %s\n", strerror(errno));
        }
        else {
            perror("error opening file");
        }
        return;
    }
    //check if first there is #!/bin/bash
    char command[INPUT_SIZE];
    ASSERT_AND_FREE(fgets(command, INPUT_SIZE, fp) != NULL && strcmp(command, "#!/bin/bash\n") == 0)
    while(fgets(command, INPUT_SIZE, fp) != NULL){
        data->script_lines_count++;
        command[strcspn(command, "\n")] = '\0';
        int i = 0;
        skip_spaces_tabs(command, &i);
        if (command[i] != '\0' && command[i] != '#') {
            run_input_command(command, data, 0);
        }
    }
    free_arg(args);
    fclose(fp);
}

/**
 * uses fork and args to run the command.
 * @return status: 0 for success, otherwise - error.
 */
void run_shell_command(char* args[], Data *data){
    global_data = data; // make data global so that sig_child handler can use the data.
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free_arg(args);
        free_lst(data->alias_lst);
        free_jobs();
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // parent
        int status;
        if (data->wait_flag) {
//             data->wait_status = true;
            pid_t child_pid = pid;
            waitpid(child_pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                if(data->is_quotes){
                    data->apostrophes_count++;
                }
                data->cmd_count++;
            } else {
                data->error_flag = true;
            }
//             data->wait_status = false;
        } else {
            add_job(pid, arg_into_str(args));
            usleep(10000);
            data->cmd_count++;
        }
        free_arg(args);
    }
    else { // child
        if (data->error_to_file) {
            int error_fd = fileno(data->error_file);
            dup2(error_fd, STDERR_FILENO);
            close(error_fd);
        }
        execvp(args[0], args);
        perror("exec");
        free_arg(args);
        _exit(EXIT_FAILURE);
    }
}

void free_arg(char* args[]){
    for(int i = 0; i < ARG_SIZE; i++){
        if(args[i] != NULL) {
            free(args[i]);
            args[i] = NULL;
        }
    }
}

void free_node(NodeList** node){
    // free one node in the list
    if(node == NULL) return;
    free((*node)->alias);
    free((*node)->command);
    (*node)->alias = NULL;
    (*node)->command = NULL;
    free((*node));
    (*node) = NULL;
}

/**
 * insert new node to the list, if name exits runs over the surrent command with the new one.
 * @param name the shortcut the user want to use for the command.
 * @return the new head of the list.
 */
NodeList* push(NodeList *head, char* name, char* command, Data* data, char* args[]){
    NodeList* temp = head;
    while (temp != NULL) {
        if (strcmp(name, temp->alias) == 0) {
            free(temp->command);
            temp->command = strdup(command);  // Alias already exists, run over the old command
            return head;
        }
        temp = temp->next;
    }

    // Create a new node
    NodeList* new_node = (NodeList*)malloc(sizeof(NodeList));
    ASSERT_MALLOC(new_node != NULL)

    // Initialize the new node
    new_node->alias = strdup(name);
    new_node->command = strdup(command);
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
    NodeList * current = head;
    NodeList * prev = NULL;
    while (current != NULL && strcmp(current->alias, name) != 0) {
        prev = current;
        current = current->next;
    }
    if (current == NULL) {
        data->error_flag = true;
        return head;
    }
    if (prev == NULL) {
        head = head->next;
    } else {
        prev->next = current->next;
    }
    free_node(&current);
    data->alias_count--;
    return head;
}

void print_lst(NodeList* head){
    while (head != NULL && head->alias != NULL && head->command != NULL){
        printf("%s='%s'\n", head->alias, head->command);
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

char* arg_into_str(char* args[]){
    if(args[0] == NULL){
        return NULL;
    }
    int res_ind = 0;
    static char res[INPUT_SIZE];
    for(int j = 0; args[0][j] != '\0'; j++){
        res[res_ind++] = args[0][j];
    }
    for(int i = 1; args[i] != NULL; i++){
        res[res_ind++] = ' ';
        for(int j = 0; args[i][j] != '\0'; j++){
            res[res_ind++] = args[i][j];
        }
    }
    res[res_ind] = '\0';
    return res;
}

/**
 * delete a job and print it on the screen-> if error "exit", otherwise: "done".
 * @param pid of job to delete
 * @param status if a job got an error, the status will be printed.
 */
void delete_job(pid_t pid) {
    if (jobs == NULL) {
        return;
    }
    Jobs* current = jobs;
    Jobs* previous = NULL;

    while (current != NULL) {
        if (current->pid == pid) {
            // to delete from here
            // for the running process to work like the terminal:
//            if(global_data->error_flag){
//                printf("[%d] exit %d\t %s\n", current->count, status ,current->command);
//                print_prompt(global_data);
//            }
//            else if(global_data->wait_status){
//                printf("[%d] Done \t %s\n", current->count, current->command);
//            }
//            else {
//                printf("\n[%d] Done \t %s\n", current->count, current->command);
//                print_prompt(global_data);
//            }
            // to delete until here
            if (previous == NULL) {
                jobs = current->next;
                if(current->next == NULL){
                    job_count = 1;
                }
                else{
                    job_count = current->next->count + 1;
                }
            }
            else {
                previous->next = current->next;
            }
            if(jobs == NULL){
                job_count = 1;
            }
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

/**
 * for commands that finishes with & - add to jobs to the beginning of the list.
 * @param pid the pid of the job to add
 * @param command of the job ro add.
 */
void add_job(pid_t pid, char* command){
    Jobs *temp = jobs;
    jobs = (Jobs*) malloc(sizeof(Jobs));
    jobs->pid = pid;
    jobs->count = job_count;
    job_count++;
    strcpy(jobs->command, command);
    jobs->next = temp;
}

void print_jobs(Jobs* jobs_head){
    if (jobs_head == NULL) {
        return;
    }
    print_jobs(jobs_head->next);
    printf("[%d] %s &\n", jobs_head->count, jobs_head->command);
}

void free_jobs() {
    Jobs * temp;
    while (jobs != NULL) {
        temp = jobs;
        jobs = jobs->next;
        free(temp);
    }
}
