#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>


#define BUFFER_SIZE 0
#define PARALLEL_DEL "&"
#define REDIRECTION_SYMBOL ">"

char path_list[100][100] = {"/bin/"};
char * cmdWithArgs[100];
int path_size = 1;
int redirection_flag_loc = 0;
char output_file_name[100];

// sample error msg

void prompt_error() {
  int result =0;
  char error_message[30] = "An error has occurred\n";
  result = write(STDERR_FILENO, error_message, strlen(error_message));
  if(result);
     return; 

  }

// remove  \n from input buffer
char * remove_new_line(char * input_str, int length){
  if ((input_str)[length - 1] == '\n'){
    (input_str)[length - 1] = '\0';
    --length;
  }
  return input_str;
}

// parse parallel command 
char ** parse_parallel_cmd(char * input_str, int * count){

  char * tok;
  char ** parallel_cmd;
  parallel_cmd = malloc(256 * sizeof(char));
  tok = strtok(input_str, PARALLEL_DEL);
  parallel_cmd[( * count) ++] = tok;
  while (tok != 0) {
    // go through other tokens

    tok = strtok(0, PARALLEL_DEL);
    parallel_cmd[( * count) ++] = tok;
  }
  return parallel_cmd;
}

// parse the input command
int parse_cmd(char * input_str, int * counter){

  int redirection_flag = 0;
  const char delimiter[4] = " ";
  char * tok;

  tok = strtok(input_str, delimiter);
  cmdWithArgs[( * counter)] = tok;

  while (tok != 0) {
    if (!strcmp(tok, REDIRECTION_SYMBOL)){
      cmdWithArgs[( * counter)] = NULL;
      redirection_flag = 1;
      tok = strtok(0, delimiter);
      strcpy(output_file_name, tok);
      strcat(output_file_name, ".txt");
    }

    if (redirection_flag) {
      cmdWithArgs[( * counter)] = NULL;
      tok = strtok(0, delimiter);
      } 
    else {
      tok = strtok(0, delimiter);
      cmdWithArgs[++( * counter)] = tok;
     }
  }
  return redirection_flag;
}

// execute command with fork and execv
void execute_cmd(int red_flag){

  int pathFound = 0;
  int searchPathCounter = 0;
  char * result_path = NULL;

  if (path_size > 0) {

      while (searchPathCounter < path_size) {

           int length = strlen(path_list[searchPathCounter]);
           char * result = malloc(length * sizeof(char));
           strcpy(result, path_list[searchPathCounter]);
           searchPathCounter++;
           strcat(result, cmdWithArgs[0]);

          if (access(result, X_OK) == 0) {
             pathFound = 1;
             result_path = result;
             break;
          }
      }
  }

  if (pathFound) {
    
    int status;
    pid_t pid;
    pid = fork();
    if (pid == 0) {
      if (red_flag) {
        int fd = open(output_file_name, O_WRONLY | O_TRUNC | O_CREAT, 1000);
             if ((fd < 0) || (dup2(fd, 1) < 0)) {
              prompt_error();
              }
        dup2(fd, 1);
        close(fd);
       }
    if (execv(result_path, cmdWithArgs)); {
        printf("%s\n", "command does not exist");
          }
        } 
     else {
         wait( & status);
        }

  } else{
    prompt_error();
        }
}

// built in to set path 
void builtin_path(int count){

  if (count == 1) {

    strcpy(path_list[0], "");
    path_size = 0;

  } else {
      path_size = 0;
      strcpy(path_list[0], "");
      int i;
      for (i=0; i < count - 1; i++) {
      strcpy(path_list[i], cmdWithArgs[i + 1]);
      path_size = i + 1;
      }
  }
}

// built in change directory
void builtin_cd(int count) {
  int result = 0;
  if (count == 2) {
    result = chdir(cmdWithArgs[1]);
    if(result)
      return;
    } 
  else {
    prompt_error();
    }
}

// check if its built-in or not and send for execution accordingly
void check_builtin_and_excecute(int count, int red_flag){

  // check builtin function and then send commands for execution
  char in_built[3][5] = {"exit","cd","path"};

  // check if built-in command or not

  if (!strcmp(in_built[0], cmdWithArgs[0])) {

    exit(0);

  } else if (!strcmp(in_built[1], cmdWithArgs[0])) {

    builtin_cd(count);

  } else if (!strcmp(in_built[2], cmdWithArgs[0])) {

    builtin_path(count);

  } else {

    execute_cmd(red_flag);

  }

}

int main(int argc, char * argv[]) {

  if (argc == 1) {

    while (1) {

      char * input_buffer = NULL;
      char * cleaned_cmd = NULL;
      char ** parallel_list;
      int parallel_count = 0;
      //int result;
      int red_flag;
      int count_val = 0;
      size_t buffer_size = BUFFER_SIZE;
      size_t input_characters;

      printf("dash> ");

      input_characters = getline( & input_buffer, & buffer_size, stdin);

      cleaned_cmd = remove_new_line(input_buffer, input_characters);

      if (input_characters == 1) continue;

      if (strstr(input_buffer, PARALLEL_DEL) != NULL) {
        parallel_list = parse_parallel_cmd(cleaned_cmd, & parallel_count);
        int i;
        for (i = 0; i < parallel_count - 1; ++i) {
          red_flag = parse_cmd(parallel_list[i], & count_val);
          check_builtin_and_excecute(count_val, red_flag);
          // reset counter
          count_val = 0;
         }
         } 
      else {

        red_flag = parse_cmd(cleaned_cmd, & count_val);
        check_builtin_and_excecute(count_val, red_flag);
      }

      // reset value or free memory
      count_val = 0;
      cmdWithArgs[0] = NULL;
      free(input_buffer);
      input_buffer = NULL;
    }

  } else if (argc == 2) {

    // batch mode
    char * batch_filename = argv[1];
    FILE * fp = fopen(batch_filename, "r");
    int line_count = 1;
    int red_flag;
    char * input_buffer = NULL;
    char * cleaned_cmd = NULL;

    size_t buffer_size = BUFFER_SIZE;
    size_t input_characters;
    input_characters = getline( &input_buffer, &buffer_size, fp);

    while (!feof(fp)) {
      int count_val = 0;
      printf("\n line count%d , command:%s\n\n", line_count, input_buffer);

      /* Increment our line count */

      line_count++;

      input_characters = getline( & input_buffer, & buffer_size, fp);
      cleaned_cmd = remove_new_line(input_buffer, input_characters);
      red_flag = parse_cmd(cleaned_cmd, & count_val);
      check_builtin_and_excecute(count_val, red_flag);
      count_val = 0;

    }

    /* Free memory */

    cmdWithArgs[0] = NULL;
    free(input_buffer);
    input_buffer = NULL;

    /* Close the file */
    fclose(fp);

    } 
  else {

    // no arguments or a single argument; anything else is an error.
    prompt_error();

  }
  return (0);
}
