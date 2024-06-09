//211749361

#include <stdio.h>
#include <signal.h>
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
    int alias_count,
            script_lines_count,
            apostrophes_count;
    bool error_flag,
            exit_flag,
            wait_flag;
    char* multiple_commands;
} typedef Data;


// functions for struct Jobs:
void  delete_job(pid_t pid);
void add_job(pid_t pid, char* command);
void print_jobs(Jobs* jobs_head);
void free_jobs();

// functions for struct NodeLIst:
void free_lst(NodeList* head);
void free_node(NodeList** node);
NodeList* push(NodeList *head, char* name, char* commend, Data* data, char*args[]);
NodeList* delete_node(NodeList* head, char* name, Data* data);
void print_lst(NodeList* head);

// functions to run the terminal:
int word_to_arg(const char* input, int arg_ind, int* input_ind, char* args[], Data* data);
int input_to_arg(const char* input, char* arg[], Data* data, int *input_ind, int *arg_ind);
void run_script_file(const char* file_name, char* arg[], Data* data);
void run_shell_command(char* arg[], Data* data, int is_quotes);
void run_input_command(const char* input, Data* data, int start_index_input);
void free_arg(char* arg[]);
void skip_spaces_tabs(const char* str, int *index);
void run_arg_command(char* args[], Data* data, int is_quotes);
char* arg_into_str(char* args[]);
#define ASSERT_MALLOC(condition) if(!(condition)) { perror("malloc"); free_lst(data->alias_lst); free_jobs(); free_arg(args); exit(EXIT_FAILURE); }
#define ASSERT_AND_FREE(condition) if (!(condition)) { data->error_flag = true; printf("ERR\n"); free_arg(args); return; }

// global elements so could change by signal:
int cmd_count = 0;
Jobs* jobs = NULL;
int job_count = 1;

void child_error_handler(){
    cmd_count--;
}

void handle_sigchld(int sig, siginfo_t *info, void *ucontext){
    pid_t pid = info->si_pid;
    delete_job(pid);
    kill(pid, SIGKILL);
}

int main() {
    struct sigaction sa;
    sa.sa_sigaction = handle_sigchld;  // Use sa_sigaction instead of sa_handler
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;  // Set the SA_SIGINFO flag
    sigaction(SIGCHLD, &sa, NULL);
    signal(SIGUSR1, child_error_handler);
    char input[INPUT_SIZE];
    Data data = {NULL,0, 0, 0, false, false, NULL};
    while(true){
        printf("#cmd:%d|#alias:%d|#script lines:%d> ",cmd_count, data.alias_count, data.script_lines_count);
        if(fgets(input, INPUT_SIZE, stdin) == NULL){
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        if(strlen(input) == 0) continue;
        run_input_command(input, &data, 0);
        if(data.exit_flag){
            printf("%d", data.apostrophes_count);
            break;
        }
    }
    free_lst(data.alias_lst);
    free_jobs();
    return 0;
}

void run_input_command(const char* input, Data* data, int start_index_input){
//    Jobs *temp = jobs;
//    while(temp != NULL){
//        if(!is_process_running(temp->pid)){
//            delete_job(temp->pid);
//        }
//    }
    data->multiple_commands = NULL;
    data->error_flag = false;
    data->wait_flag = true;
    char* args[ARG_SIZE];
    for(int i = 0; i < ARG_SIZE; i++){
        args[i] = NULL;
    }
    int args_ind = 0;
    int is_quotes = input_to_arg(input, args, data, &start_index_input, &args_ind);
    ASSERT_AND_FREE(is_quotes >= 0);
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
        run_arg_command(args, data, is_quotes);
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
        run_arg_command(args, data, is_quotes);
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
void run_arg_command(char* args[], Data* data, int is_quotes){
    if(strcmp(args[0], "alias") == 0){
        if(args[1] == NULL){
            print_lst(data->alias_lst);
            cmd_count++;
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
            if(is_quotes > 0) data->apostrophes_count++;
            cmd_count++;
            return;
        }
        ASSERT_AND_FREE(strcmp(args[2], "=") == 0 && args[3] != NULL && args[4] == NULL)
        data->alias_lst = push(data->alias_lst, args[1], args[3], data, args);
        free_arg(args);
        if(is_quotes > 0) data->apostrophes_count++;
        cmd_count++;
        return;
    }
    if(strcmp(args[0], "unalias") == 0){
        ASSERT_AND_FREE(args[1] != NULL && args[2] == NULL)
        data->alias_lst = delete_node(data->alias_lst, args[1], data);
        free_arg(args);
        if(!data->error_flag){
            if(is_quotes > 0) data->apostrophes_count++;
            cmd_count++;
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
            if(is_quotes > 0) data->apostrophes_count++;
            cmd_count++;
        }
        return;
    }
    if(strcmp(args[0], "jobs") == 0){
        ASSERT_AND_FREE(args[1] == NULL)
        print_jobs(jobs);
        cmd_count++;
        return;
    }
    if(strcmp(args[0], "cd") == 0){
        ASSERT_AND_FREE(args[1] != NULL)
        if (chdir(args[1]) == -1) {
            perror("cd");
            free_arg(args);
            return;
        }
        cmd_count++;
        free_arg(args);
        return;
    }
    run_shell_command(args, data, is_quotes);
    free_arg(args);
}

/**
 * Find one argument and put it in args[arg_ind];
 * @param input command from user / line from file.
 * @param arg_ind the place in th array to put the argument in.
 * @param input_ind where to start reading the input.
 * @return  if the argument was an apostrophe type. if invalid apostrophe: raises error flag;
 */
int word_to_arg(const char* input, int arg_ind, int* input_ind, char* args[], Data* data){
    if(args[arg_ind] != NULL){
        free(args[arg_ind]);
        args[arg_ind] = NULL;
    }
    bool is_quot = false;
    skip_spaces_tabs(input, input_ind);
    if(input[*input_ind] == '\0') return 0;
    int start = *input_ind, arg_len = 1;
    if(arg_ind == 0){ // finding the name of command
        while(input[*input_ind] != '\0' && input[*input_ind] != ' ' && input[*input_ind] != '\t') {
            if (input[*input_ind] == '\'' || input[*input_ind] == '"') {
                char type = input[(*input_ind)];
                (*input_ind)++, arg_len++;
                while (input[*input_ind] != type) {
                    if (input[*input_ind] == '\0') {
                        return ERROR_VALUE;
                    }
                    (*input_ind)++, arg_len++;
                }
                is_quot = true;
                (*input_ind)++, arg_len++;
            }
            else {
                (*input_ind)++, arg_len++;
            }
        }
        if (is_quot && (input[start] == '\'' || input[start] == '"') && input[start] == input[(*input_ind) - 1]) {
            start++, arg_len-=2;
        }
    }
    else if(input[*input_ind] == '\'' || input[*input_ind] == '"'){
        char type = input[(*input_ind)++];
        start++;
        while(input[*input_ind] != type){
            if(input[*input_ind] == '\0'){
                return ERROR_VALUE;
            }
            (*input_ind)++, arg_len++;
        }
        is_quot = true;
    }
    else {
        while (input[*input_ind] != '\0' && input[*input_ind] != ' ' && input[*input_ind] != '\t' &&
               input[*input_ind] != '\'' && input[*input_ind] != '"') {
            arg_len++, (*input_ind)++;
        }
    }
    args[arg_ind] = (char *) malloc(arg_len);
    ASSERT_MALLOC(args[arg_ind] != NULL)
    for(int i = 0; i < arg_len - 1; i++, start++){
        args[arg_ind][i] = input[start];
    }
    args[arg_ind][arg_len - 1] = '\0';
    if(strcmp(args[arg_ind], "&&") == 0 || strcmp(args[arg_ind], "||") == 0){
        data->multiple_commands = strdup(args[arg_ind]);
    }
    if(is_quot) {
        (*input_ind)++;
        return 1;
    }
    return 0;
}

/**
 * turns input into an array of arguments using word_to_arg function.
 * @return how many argument are apostrophe type, if invalid apostrophe -> returns -1;
 */
int input_to_arg(const char input[], char* args[], Data* data, int* input_ind, int*arg_ind){
    int apostrophe_update = word_to_arg(input, *arg_ind, input_ind, args, data);
    if(data->multiple_commands != NULL){
        return apostrophe_update;
    }
    if(apostrophe_update < 0) {
        return ERROR_VALUE;
    }
    if(args[*arg_ind] == NULL){
        return 0;
    }
    if(strcmp(args[*arg_ind], "exit_shell") == 0){
        data->exit_flag = true;
        return apostrophe_update;
    }
    NodeList *temp = data->alias_lst;
    while(temp != NULL) {
        if (strcmp(temp->alias, args[*arg_ind]) == 0) {
            int command_ind = 0;
            int AU = word_to_arg(temp->commend, *arg_ind, &command_ind, args, data);
            if(data->multiple_commands != NULL){
                return apostrophe_update;
            }
            if (AU < 0) {
                return ERROR_VALUE;
            }
            apostrophe_update += AU;
            while (args[*arg_ind] != NULL) {
                if (*arg_ind >= ARG_SIZE - 1) {
                    return ERROR_VALUE;
                }
                (*arg_ind)++;
                AU = word_to_arg(temp->commend, *arg_ind, &command_ind, args, data);
                if(data->multiple_commands != NULL){
                    return apostrophe_update;
                }
                if (AU < 0) {
                    return ERROR_VALUE;
                }
                apostrophe_update += AU;
            }
            (*arg_ind)--;
        }
        temp = temp->next;
    }
    while(args[*arg_ind] != NULL){
        if(*arg_ind >= ARG_SIZE - 1){
            return ERROR_VALUE;
        }
        (*arg_ind)++;
        int AU = word_to_arg(input, *arg_ind, input_ind, args, data);
        if(data->multiple_commands != NULL){
            return apostrophe_update;
        }
        if(AU < 0) {
            return ERROR_VALUE;
        }
        apostrophe_update += AU;
    }
    (*arg_ind)--;
    return apostrophe_update;
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
void run_shell_command(char* args[], Data *data, int is_quotes){
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free_arg(args);
        free_lst(data->alias_lst);
        free_jobs();
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // parent
        if(data->wait_flag) {
            pause();
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) == 0){
                    cmd_count++;
                    if(is_quotes > 0){
                        data->apostrophes_count++;
                    }
                    return;
                }
                else{
                    data->error_flag = true;
                }
            } else {
                // Child process did not terminate normally
                data->error_flag = true;
            }
        }
        else{
            add_job(pid, arg_into_str(args));
            cmd_count++;
            usleep(100000);
        }
        free_arg(args);
    } else { // child
        execvp(args[0], args);
        perror("exec");
        free_arg(args);
        if(!(data->wait_flag)) {
            kill(getppid(), SIGUSR1);
        }
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
NodeList* push(NodeList *head, char* name, char* commend, Data* data, char*args[]){
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
    ASSERT_MALLOC(new_node != NULL)

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

void  delete_job(pid_t pid){
    if(jobs == NULL){
        return;
    }
    if(jobs->pid == pid){
        printf("\n[%d] \t Done \t\t %s\n", jobs->count, jobs->command);
        Jobs* temp = jobs->next;
        free(jobs);
        jobs = temp;
        if(jobs == NULL){
            job_count = 1;
        }
        return;
    }
    Jobs* prev = jobs;
    Jobs* cur = jobs->next;
    while(cur != NULL){
        if(cur->pid == pid){
            printf("\n[%d] \t Done \t\t %s\n", jobs->count, jobs->command);
            if(cur->next == NULL){
                job_count = prev->count + 1;
            }
            Jobs * temp = cur->next;
            free(cur);
            cur = NULL;
            prev->next = temp;
            return;
        }
        cur = cur->next;
        prev = prev->next;
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
