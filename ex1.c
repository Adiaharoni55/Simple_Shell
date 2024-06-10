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
            wait_status,
            is_quotes;
    char* multiple_commands;
} typedef Data;

// functions for struct Jobs:
void delete_job(pid_t pid, int status);
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
void run_shell_command(char* args[], Data* data);
void run_input_command(const char* input, Data* data, int start_index_input);
void free_arg(char* args[]);
void skip_spaces_tabs(const char* str, int *index);
void run_arg_command(char* args[], Data* data);
char* arg_into_str(char* args[]);
#define ASSERT_MALLOC(condition) if(!(condition)) { perror("malloc"); free_lst(data->alias_lst); free_jobs(); free_arg(args); exit(EXIT_FAILURE); }
#define ASSERT_AND_FREE(condition) if (!(condition)) { data->error_flag = true; printf("ERR\n"); free_arg(args); return; }

// global elements so could change by signal:
Jobs* jobs = NULL;
int job_count = 1;
bool error_in_child_process = false;
Data *global_data = NULL;

void sigchld_handler(int sig, siginfo_t *info, void *context) {
    int status;
    pid_t pid;
    if (sig == SIGCHLD) {
        // Wait for all dead processes.
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                global_data->cmd_count--;
                global_data->error_flag = true;
            }
            else {
                if (global_data->is_quotes && !error_in_child_process) {
                    global_data->apostrophes_count++;
                }
            }
            delete_job(pid, WEXITSTATUS(status));
        }
    }
}

int main() {
    char input[INPUT_SIZE];
    Data data = {NULL, 0, 0, 0, 0,
                 false, false, false, false, false, NULL};
    global_data = &data; // Set the global data pointer

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

void print_prompt(Data* data){
    printf("#cmd:%d|#alias:%d|#script lines:%d> ", data->cmd_count, data->alias_count, data->script_lines_count);
    fflush(stdout);
}


void run_input_command(const char* input, Data* data, int start_index_input){
    data->multiple_commands = NULL;
    data->error_flag = false;
    data->wait_flag = true;
    char* args[ARG_SIZE];
    for(int i = 0; i < ARG_SIZE; i++){
        args[i] = NULL;
    }
    int args_ind = 0;
    input_to_arg(input, args, data, &start_index_input, &args_ind);
    if(data->error_flag){
        printf("ERR\n"); free_arg(args); return;
    }
    if(data->exit_flag){
        free_arg(args);
        return;
    }
    if(data->multiple_commands == NULL) {
        if(strcmp(args[args_ind], "&") == 0){
            free(args[args_ind]);
            args[args_ind] = NULL;
            data->wait_flag = false;
        }
        else if(args[args_ind][strlen(args[args_ind] - 1)] == '&'){
            args[args_ind][strlen(args[args_ind] - 1)] = '\0';
            data->wait_flag = false;
        }
        run_arg_command(args, data);
        return;
    }
    free(args[args_ind]);
    args[args_ind--] = NULL;

    if(args[args_ind] != NULL){
        if(strcmp(args[args_ind], "&") == 0){
            free(args[args_ind]);
            args[args_ind] = NULL;
            data->wait_flag = false;
        }
        else if(args[args_ind][strlen(args[args_ind] - 1)] == '&'){
            args[args_ind][strlen(args[args_ind] - 1)] = '\0';
            data->wait_flag = false;
        }
        run_arg_command(args, data);
    }
    free_arg(args);
    if((strcmp(data->multiple_commands, "&&") == 0 && data->error_flag)
       || (strcmp(data->multiple_commands, "||") == 0 && !(data->error_flag))
       || input[start_index_input] == '\0'){
        free(data->multiple_commands);
        data->multiple_commands = NULL;
        return;
    }
    free(data->multiple_commands);
    data->multiple_commands = NULL;
    run_input_command(input, data, start_index_input);
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
            free_arg(args);
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
        printf("ERR\n");
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
            perror("cd");
            free_arg(args);
            return;
        }
        data->cmd_count++;
        free_arg(args);
        return;
    }
    run_shell_command(args, data);
    free_arg(args);
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
    if (arg_ind == 0) { // finding the name of command
        while (input[*input_ind] != '\0' && input[*input_ind] != ' ' && input[*input_ind] != '\t') {
            if (input[*input_ind] == '\'' || input[*input_ind] == '"') {
                char type = input[(*input_ind)];
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
                data->is_quotes = true;
                (*input_ind)++, arg_len++;
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
    if(args[*arg_ind] == NULL || data->error_flag || data->multiple_commands != NULL){
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
            if(data->error_flag || data->multiple_commands != NULL){
                return;
            }
            while (args[*arg_ind] != NULL) {
                if (*arg_ind >= ARG_SIZE - 1) {
                    data->error_flag = true;
                    return;
                }
                (*arg_ind)++;
                word_to_arg(temp->command, *arg_ind, &command_ind, args, data);
                if(data->multiple_commands != NULL || data->error_flag){
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
            return;
        }
        (*arg_ind)++;
        word_to_arg(input, *arg_ind, input_ind, args, data);
        if(data->multiple_commands != NULL || data->error_flag){
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
    if(fp == NULL){
        data->error_flag = true;
        perror("Error opening file");
        return;
    }
    free_arg(args);
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
            data->wait_status = true;
            pid_t child_pid = pid;
            pid_t waited_pid;
            while ((waited_pid = waitpid(child_pid, &status, 0)) != child_pid) {
                if (waited_pid == -1) {
                    if (errno == ECHILD) {
                        break;
                    }
                }
            }
            data->wait_status = false;
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                if(data->is_quotes){
                    data->apostrophes_count++;
                }
                data->cmd_count++;
            } else {
                data->error_flag = true;
            }
        } else {
            add_job(pid, arg_into_str(args));
            data->cmd_count++;
        }
        free_arg(args);
    }
    else { // child
        usleep(100000);
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
void delete_job(pid_t pid, int status) {
    if (jobs == NULL) {
        return;
    }

    Jobs* current = jobs;
    Jobs* previous = NULL;

    while (current != NULL) {
        if (current->pid == pid) {
            if(global_data->error_flag){
                printf("[%d] exit %d \t %s\n", current->count, status, current->command);
                print_prompt(global_data);
            }
            else if(global_data->wait_status){
                printf("[%d] Done \t %s\n", current->count, current->command);
            }
            else {
                // Print the job status
                if (!error_in_child_process) {
                    printf("\n[%d] Done \t %s\n", current->count, current->command);
                    print_prompt(global_data);
                } else {
                    printf("\n[%d] exit %d \t %s\n", current->count, status, current->command);
                    error_in_child_process = false;
                }
            }

            // Remove the job from the list
            if (previous == NULL) { // Job to delete is the head
                jobs = current->next;
            } else {
                previous->next = current->next;
            }

            free(current); // Free the memory for the job
            job_count--; // Decrease the job count
            return;
        }
        previous = current;
        current = current->next;
    }
}


void add_job(pid_t pid, char* command){
    Jobs *temp = jobs;
    jobs = (Jobs*) malloc(sizeof(Jobs));
    jobs->pid = pid;
    jobs->count = job_count;
    job_count++;
    strcpy(jobs->command, command);
    jobs->next = temp;
    printf("[%d] %d\n", jobs->count, jobs->pid);
}

void print_jobs(Jobs* jobs_head){
    if (jobs_head == NULL) {
        return;
    }
    print_jobs(jobs_head->next);
    printf("[%d] \t Running \t %s &\n", jobs_head->count, jobs_head->command);
}

void free_jobs() {
    Jobs * temp;
    while (jobs != NULL) {
        temp = jobs;
        jobs = jobs->next;
        free(temp);
    }
}