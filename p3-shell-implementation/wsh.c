// Author:  Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2023
// Revised: John Shawger <shawgerj@cs.wisc.edu>, Spring 2024
// Revised: Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2024
// Revised: Leshna Balara <lbalara@cs.wisc.edu>, Spring 2025

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "wsh.h"

/* Print an application error message (does not exit) */
void app_error(char *msg)
{
  fprintf(stderr, "%s\n", msg);
}

void non_recoverable_error(char *msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(-1);
}

int exit_too_many_args = 0;

// This is just basic string parsing, some of the things not handled are mentioned as TODOs*/
// Exits currently do not return the correct value according to the spec,
// so you will need to change the parameter when appropriate*/
// Some helper functions available to you but currently not reachable:
// char *do_variable_substitution(const char *command)
// char *replaceCommandSubstitution(const char *command)
// print_ps_header (also provides how to print ps entries as HINT)
int main(int argc, char **argv)
{
  check_params(argv, argc);
  setenv("PATH", "/bin", 1);
  if (argc == 1)
    interactive_main();
  else
    batch_main(argv[1]);

  return 0; /* Not reached */
}


/* TODO: Print prompt where appropriate */
void interactive_main(void)
{
  if (!getcwd(cwd, MAXLINE))
  {
    perror("getcwd");
    exit(-1);
  }
  while (1)
  {
    printf("wsh> ");
    char cmdline[MAXLINE];
    if (fgets(cmdline, MAXLINE, stdin) == NULL)
    {
      if (ferror(stdin))
        app_error("fgets error");
      if (feof(stdin))
      {
        break;
      }
    }

    fflush(stdout);
    eval(cmdline);
    if(exit_too_many_args){
      printf("wsh> ");
      exit(-1);
    }
  }
}


/* Batch mode: read each line from the given script file */
int batch_main(char *scriptFile)
{
  FILE *file = fopen(scriptFile, "r");
  if (file == NULL)
  {
    perror("Error opening file");
    exit(-1);
  }

  char line[MAXLINE];
  while (fgets(line, MAXLINE, file) != NULL)
  {
    int length = strlen(line);
    if (length > 0 && line[length - 1] != '\n')
    {
      line[length] = '\n';
      line[length + 1] = '\0';
    }
    eval(line);
    if(exit_too_many_args){
      printf("wsh> ");
      exit(-1);
    }
  }

  fclose(file);
  exit(0);
  return 0;
}

typedef struct Node{
  char *var;
  char *value;
  struct Node *next;
} Node;

struct Node *head = NULL;

void export_handler(char *var){
  if (!strchr(var, '=')){//variable has no "="
    char *var_name  = strdup(var);
    setenv(var_name, "", 1);
    free(var_name);
  }else{//variable has "="
    char *var_copy = strdup(var);
    char *var_name = strtok(var_copy, "=");
    char *value = strtok(NULL, "=");
    if(value == NULL){
      value = "";
    }
    setenv(var_name, value, 1);
    free(var_copy);
  }
}

void local_handler(char *var){
  if (!strchr(var, '=')){//variable has no "="
    char *var_name  = strtok(var, "=");
    bool found = false;

    if(head == NULL){
      head = (Node *)malloc(sizeof(Node));//initialize node pointer to head
      if(!head){
        perror("malloc");
        exit(-1);
      }
      head->var = strdup(var_name);
      head->value = "";
      head->next = NULL;
    }else{
      Node *current = head;
      while(current != NULL){
        if(strcmp(current->var, var_name) == 0){ //same var name
          current->value = "";
          found = true;
          return;
        }
        if(current->next == NULL){
          break;
        }
        current = current->next;
      }
      if(found == false){
        Node *new_node = (Node *)malloc(sizeof(Node));
        if(!new_node){
          perror("malloc");
          exit(-1);
        }
        new_node->var = strdup(var_name);
        new_node->value = "";
        new_node->next = NULL;
        current->next = new_node;
      }
    }
  }else{//variable has "="
    char *var_name = strtok(var, "=");
    char *var_value = strtok(NULL, "=");
    bool found = false;
    if(var_value == NULL){
      var_value = "";
    }
    if(head == NULL){
      head = (Node *)malloc(sizeof(Node));
      if(!head){
        perror("malloc");
        exit(-1);
      }
      head->var = strdup(var_name);
      head->value = strdup(var_value);
      head->next = NULL;
    }else{
      Node *current = head;
      while(current != NULL){
        if(strcmp(current->var, var_name) == 0){//same var name
          free(current->value);
          current->value = strdup(var_value);
          found = true;
          return;
        }
        if(current->next == NULL){
          break;
        }
        current = current->next;
      }
      if(found == false){
        Node *new_node = (struct Node *)malloc(sizeof(struct Node));
        if(!new_node){
          perror("malloc");
          exit(-1);
        }
        new_node->var = strdup(var_name);
        new_node->value = strdup(var_value);
        new_node->next = NULL;
        current->next = new_node;
      }
    }
  }
}

void display_vars(){
  Node *current = head;
  while(current != NULL){
    printf("%s=%s\n", current->var, current->value);
    current = current->next;
  }
}

void ls_handler(){
  char cwd[1024];
  if(getcwd(cwd, sizeof(cwd)) == NULL){
    perror("no such directory");
    exit(-1);
  }

  DIR *directory = opendir(cwd);
  if (directory == NULL){
    perror("opendir");
    exit(-1);
  }
  struct dirent *entry;
  char *filenames [1024];
  int count = 0;
  while((entry = readdir(directory)) != NULL){
    char *entry_name = entry->d_name;
    if(entry_name[0] == '.'){
      continue;
    }
    filenames[count++] = strdup(entry_name);
  }
  closedir(directory);

  for(int i = 0; i < count - 1; i++){
    for(int j = 0; j < count - i - 1; j++){
      if(strcmp(filenames[j], filenames[j + 1]) > 0){
        char *temp = filenames[j];
        filenames[j] = filenames[j + 1];
        filenames[j + 1] = temp;
      }
    }
  }

  for(int i = 0; i < count; i++){
    printf("%s\n",filenames[i]);
    free(filenames[i]);
  }
}

void ps_handler() {
  DIR *directory = opendir("/proc"); // Open list of processes
  if (directory == NULL) {
    perror("opendir");
    return;
  }

  printf("%5s %5s %1s %s\n", "PID", "PPID", "S", "COMMAND");

  struct dirent *entry;
  typedef struct {
    int pid;
    int ppid;
    char state;
    char command[100];
  } proc_info;
  
  proc_info processes[1024];
  int proc_count = 0;

  while ((entry = readdir(directory)) != NULL) {
    char *p = entry->d_name;
    bool is_pid = true;
    while (*p) {
      if (!isdigit(*p)) {
        is_pid = false;
        break;
      }
      p++;
    }
    
    if (!is_pid) continue; 
    
    int pid = atoi(entry->d_name);
    if (pid <= 0) continue; 
    
    char path[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    FILE *stat_file = fopen(path, "r");
    if (!stat_file) continue; 
    
    int curr_pid;
    char command[100];
    char state;
    int ppid;
    
    int result = fscanf(stat_file, "%d (%[^)]) %c %d", &curr_pid, command, &state, &ppid);
    fclose(stat_file);
    
    if (result != 4) continue; 
    
    processes[proc_count].pid = curr_pid;
    processes[proc_count].ppid = ppid;
    processes[proc_count].state = state;
    strcpy(processes[proc_count].command, command);
    proc_count++;
  }
  
  closedir(directory);
  //bubble sort
  for (int i = 0; i < proc_count - 1; i++) {
    for (int j = 0; j < proc_count - i - 1; j++) {
      if (processes[j].pid > processes[j + 1].pid) {
        proc_info temp = processes[j];
        processes[j] = processes[j + 1];
        processes[j + 1] = temp;
      }
    }
  }
  
  for (int i = 0; i < proc_count; i++) {
    printf("%5d %5d %c %s\n", processes[i].pid, processes[i].ppid, processes[i].state, processes[i].command);
  }
}
/* Evaluate a command line */
/* TODO: Actual Evaluation */ //work in progress
void eval(char *cmdline)
{
  char *sub_cmd = do_variable_substitution(cmdline);
  char *final_cmd = replaceCommandSubstitution(sub_cmd);
  free(sub_cmd);
  /* If the command contains a pipe, handle it separately */
  if (strchr(final_cmd, '|') != NULL)
  {
    eval_pipe(final_cmd);
    free(final_cmd);
    return;
  }

  /* you can invoke parseline_no_subst(const char *cmdline, char **argv, int *argc)
    for basic parsing with no substitutions*/
    char *argv[MAXLINE];
    int argc;
    parseline_no_subst(final_cmd, argv, &argc);
    free(final_cmd);

    if(argc == 0) {
      return;
    }
    if (cmdline[0] == '$' && !strchr(cmdline, ' ') && !strchr(cmdline, '\t')) {
      if (argc > 0) {
        printf("%s\n", argv[0]);        
        for (int i = 0; i < argc; i++) {
          free(argv[i]);
        }
        return;
      }
    }
    if (strcmp(argv[0], "exit") == 0){
      if(argc > 1){
        fprintf(stderr, "Incorrect usage of exit. Usage: exit\n");
        exit_too_many_args = 1;
        return;
      }
      exit(0);
    }else if (strcmp(argv[0], "export") == 0){ 
      if(argc != 2){
        fprintf(stderr, "Incorrect usage of export. Usage: export {VariableName}={VariableValue}\n");
        return;
      }
      export_handler(argv[1]);
    }else if(strcmp(argv[0], "local") == 0){
      if(argc != 2){
        fprintf(stderr, "Incorrect usage of local. Usage: local {VariableName}={VariableValue}\n");
        return;
      }
      local_handler(argv[1]);
    }else if(strcmp(argv[0], "vars") == 0){
      display_vars();
    }else if(strcmp(argv[0], "ls") == 0){
      ls_handler();
    }else if(strcmp(argv[0], "ps") == 0){
      ps_handler();
    }else {
      pid_t pid = fork();
      if(pid == 0) { // child process
        char *path;
        if(argv[0][0] == '/' || argv[0][0] == '.') {
          path = strdup(argv[0]);
        } else {
          char *env_path = getenv("PATH");
          if(env_path == NULL){
            fprintf(stderr, "PATH not set\n");
          }
          char final_path[1024];
          snprintf(final_path, sizeof(final_path), "%s/%s", env_path, argv[0]); // Add separator
          path = strdup(final_path);
        }
        execv(path, argv);
        perror("execv failed"); 
        exit(1);  // Exit if execv fails
      } else if(pid > 0) {
        int status;
        waitpid(pid, &status, 0);
      } else {
        perror("fork failed");
        exit(-1);
      }
    }
    
}

/* basic check to verify we have at most 2 arguments */
void check_params(char **argv, int argc)
{
  (void)argv;
  if (argc > 2)
  {
    printf("Usage: wsh or wsh script.wsh \n");
    printf("No arguments allowed.\n");
    exit(-1);
  }
}

/* Evaluate a command line that contains a pipe */
/* TODO: The basic parsing in eval_pipe does not handle substitutions of any kind */
/* Evaluate a command line that contains a pipe */
void eval_pipe(char *cmdline)
{
  /* Split the processed command line on '|' */
  char *commands[MAXARGS];
  int num_commands = 0;
  char *token = strtok(cmdline, "|");
  while (token != NULL && num_commands < MAXARGS)
  {
    while (*token == ' ')
      token++; /* trim leading whitespace */
    commands[num_commands++] = strdup(token);
    token = strtok(NULL, "|");
  }

  /* For each pipeline segment, tokenize without re‑substitution */
  char **argv_list[num_commands];
  int argc_list[num_commands];
  for (int i = 0; i < num_commands; i++)
  {
    char *sub_cmd = do_variable_substitution(commands[i]);
    char *final_cmd = replaceCommandSubstitution(sub_cmd);

    argv_list[i] = malloc(sizeof(char *) * MAXARGS);
    parseline_no_subst(final_cmd, argv_list[i], &argc_list[i]);
    free(commands[i]);
    if (!argc_list[i])
    {
      perror("malloc");
      exit(-1);
    }
  }

  int pipefds[256];
  for (int i = 0; i < num_commands - 1; i++)
  {
    if (pipe(pipefds + i * 2) < 0)
    {
      perror("pipe");
      exit(-1);
    }
  }

  /* Now you have commands in argv_list[i][0] and their arg list in argv_list[i] */
  /* Execute and handle the command when needed */
  pid_t pids[num_commands];
  for (int i = 0; i < num_commands; i++)
  {
    pid_t pid = fork();
    pids[i] = pid;
    if (pid == 0) {  
      if (i > 0) {
        dup2(pipefds[(i - 1) * 2], 0);  
      }
      
      if (i < num_commands - 1) {
        dup2(pipefds[i * 2 + 1], 1); 
      }
      for (int j = 0; j < 2 * (num_commands - 1); j++) {
        close(pipefds[j]);
      }

      char *path = NULL;

      if (argv_list[i][0][0] == '/' || argv_list[i][0][0] == '.') {
        path = strdup(argv_list[i][0]);
      } else {
        char *env_path = getenv("PATH");
        char final_path[1024];
        char *dir = strtok(env_path, ":");
        while (dir) {
          snprintf(final_path, sizeof(final_path), "%s/%s", dir, argv_list[i][0]);
          if (access(final_path, X_OK) == 0) {
            path = strdup(final_path);
            break;
          }
          dir = strtok(NULL, ":");
        }
      }

      if (!path) {
        fprintf(stderr, "Command not found or not executable\n");
        exit(-1);
      }

      execv(path, argv_list[i]);
      perror("execv failed");
      free(path);
      exit(1);
    } else if (pid < 0) {
      perror("fork failed");
      exit(-1);
    }
  }

  /* Close all pipe file descriptors in the parent */
  for (int i = 0; i < 2 * (num_commands - 1); i++) {
    close(pipefds[i]);
  }

  /* Wait for all child processes to complete */
  for (int i = 0; i < num_commands; i++) {
    waitpid(pids[i], NULL, 0);
  }

  /* Free memory for argument lists */
  for (int i = 0; i < num_commands; i++) {
    for (int j = 0; j < argc_list[i]; j++) {
      free(argv_list[i][j]);
    }
    free(argv_list[i]);
  }
}


/* Replace variables of the form $VAR (skipping those starting with $(') */
char *do_variable_substitution(const char *command)
{
  const char *delimiters = " \t\n=";
  char *commandCopy = strdup(command);
  if (!commandCopy)
  {
    perror("strdup");
    exit(-1);
  }
  /* We work on a copy that is replaced gradually */
  char *current = strdup(command);
  if (!current)
  {
    perror("strdup");
    exit(-1);
  }

  char *saveptr;
  char *token = strtok_r(commandCopy, delimiters, &saveptr);
  while (token != NULL)
  {
    size_t token_len = strlen(token);
    if (token[0] == '$' && token_len > 1 && token[1] != '(')
    {
      char *var_name = token + 1;
      char *var_value = getenv(var_name);

      if(!var_value){
        Node *current_node = head;
        while(current_node != NULL){
          if(strcmp(current_node->var, var_name) == 0){
            var_value = current_node -> value;
            break;
          }
          current_node = current_node->next;
        }
      }
      if(var_value){
        char *token_pos = strstr(current, token);
        if(token_pos) {
          size_t prefix_len = token_pos - current;
          size_t suffix_len = strlen(current) - prefix_len - token_len;
          char *new_string = malloc(prefix_len + strlen(var_value) + suffix_len + 1);
          if(!new_string) {
            perror("malloc");
            exit(-1);
          }
          
          strncpy(new_string, current, prefix_len);
          strcpy(new_string + prefix_len, var_value);
          strcpy(new_string + prefix_len + strlen(var_value), token_pos + token_len);
          free(current);
          current = new_string;
        }
      }
    }
    else if (token[0] == '$')
    {
      /*TODO: edge case, depends on your implementation */
      //Do nothing
    }
    token = strtok_r(NULL, delimiters, &saveptr);
  }
  free(commandCopy);
  return current;
}

/* Replace command substitutions of the form $(subcommand) recursively
  Returns null in case of any errors */
char *replaceCommandSubstitution(const char *command)
{
  char *result = strdup(command);
  if (!result)
  {
    perror("strdup");
    exit(-1);
  }

  char *sub_start;
  while ((sub_start = strstr(result, "$(")) != NULL)
  {
    char *sub_end = sub_start + 2;
    int paren_count = 1;
    while (*sub_end && paren_count > 0)
    {
      if (*sub_end == '(')
        paren_count++;
      else if (*sub_end == ')')
        paren_count--;
      sub_end++;
    }
    if (paren_count != 0)
    {
      app_error("Unmatched parentheses in command substitution");
      free(result);
      return NULL;
    }

    int sub_len = (sub_end - 1) - (sub_start + 2);
    char *sub_cmd = malloc(sub_len + 1);
    if (!sub_cmd)
    {
      perror("malloc");
      exit(-1);
    }
    strncpy(sub_cmd, sub_start + 2, sub_len);
    sub_cmd[sub_len] = '\0';

    /* TODO: Execute the subcommand and capture its output */
    /* sub_cmd has the value of the innermost nested command */
    char *output = malloc(1024);
    if(!output){
      perror("malloc");
      exit(-1);
    }
    output[0] = '\0';
    FILE *fp = popen(sub_cmd, "r");

    char buffer[1024];
    size_t output_size = 1024;
    size_t output_len = 0;
    
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
      size_t buffer_len = strlen(buffer);
      if (output_len + buffer_len >= output_size) {
        output_size *= 2;
        char *new_output = realloc(output, output_size);
        if (!new_output) {
          perror("realloc");
          free(output);
          free(sub_cmd);
          pclose(fp);
          free(result);
          exit(-1);
        }
        output = new_output;
      }
      strcpy(output + output_len, buffer);
      output_len += buffer_len;
    }
    pclose(fp);
    free(sub_cmd);

    /* Remove a trailing newline if present */
    output_len = strlen(output);
    if (output_len > 0 && output[output_len - 1] == '\n')
      output[output_len - 1] = '\0';

    size_t prefix_len = sub_start - result;
    output_len = strlen(output);
    size_t suffix_len = strlen(sub_end);
    size_t new_len = prefix_len + output_len + suffix_len + 1;
    char *new_result = malloc(new_len);
    if (!new_result)
    {
      perror("malloc");
      exit(-1);
    }
    memcpy(new_result, result, prefix_len);
    memcpy(new_result + prefix_len, output, output_len);
    memcpy(new_result + prefix_len + output_len, sub_end, suffix_len + 1);
    free(output);
    free(result);
    result = new_result;
  }
  return result;
}

/* Tokenize without further substitution.
   This version now strdup()’s each token so that the returned argv tokens remain valid. */
void parseline_no_subst(const char *cmdline, char **argv, int *argc)
{
  char *buf = strdup(cmdline);
  if (!buf)
  {
    perror("strdup");
    exit(-1);
  }
  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = ' ';
  else
  {
    buf = realloc(buf, len + 2);
    if (!buf)
    {
      perror("realloc");
      exit(-1);
    }
    strcat(buf, " ");
  }

  int count = 0;
  char *p = buf;
  while (*p && (*p == ' '))
    p++; /* skip leading spaces */

  while (*p)
  {
    char *token_start = p;
    char *token;
    if (*p == '\'')
    {
      token_start = ++p;
      token = strchr(p, '\'');
      if (!token)
      {
        app_error("Missing closing quote");
        free(buf);
        return;
      }
      *token = '\0';
      p = token + 1;
    }
    else
    {
      token = strchr(p, ' ');
      if (!token)
        break;
      *token = '\0';
      p = token + 1;
    }
    argv[count++] = strdup(token_start);
    while (*p && (*p == ' '))
      p++;
  }
  argv[count] = NULL;
  *argc = count;
  free(buf);
}

typedef struct
{
  u_int16_t width;
  const char *header;
} ps_out_t;

static const ps_out_t out_spec[] = {
    {5, "PID"},
    {5, "PPID"},
    {1, "S"},
    {16, "CMD"},
};

/* Print the header with appropriate formatting for ps command */
void print_ps_header()
{
  for (size_t i = 0; i < sizeof(out_spec) / sizeof(out_spec[0]); i++)
  {
    printf("%-*s ", out_spec[i].width, out_spec[i].header);
  }
  printf("\n");

  /* HINT: of how to print the actual entry
     printf("%*d %*d %*c %-*s\n",
           out_spec[0].width, pid, // pid is int
           out_spec[1].width, ppid, // ppid is int
           out_spec[2].width, state, // state is char
           out_spec[3].width, comm); // command is char array
  */
}
