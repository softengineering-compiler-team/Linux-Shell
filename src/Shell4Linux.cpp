#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>

#define RL_BUFSIZE 1024
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\r\n\a"

int cd(char **args);
int help(char **args);
int exit(char **args);

char *builtin_str[] = {
  "cd",
  "help",
  "exit"
};

int num_builtins() {
  	return sizeof(builtin_str) / sizeof(char *);
}

int (*builtin_func[]) (char **) = {
  &cd,
  &help,
  &exit
};

int help(char **args) {
  	int i;
  	printf("Type program names and arguments, and hit enter.\n");
  	printf("The following are built in:\n");

  	for (i = 0; i < num_builtins(); i++) {
    	printf("  %s\n", builtin_str[i]);
  	}

  	printf("Use the man command for information on other programs.\n");
  	return 1;
}

int cd(char **args) {
  if(args[1] == NULL) {
    fprintf(stderr, "Expected argument to \"cd\" not found\n!");
  } else {
    if (chdir(args[1]) != 0) {
      perror("Linux-Shell:");
    }
  }
  return 1;
}

int launch(char **args) {
  	pid_t pid;
  	int status;

 	pid = fork();
  	if (pid == 0) {
    	// Child process
    	if (execvp(args[0], args) == -1) {
      		perror("Linux-Shell:");
    	}
    	exit(EXIT_FAILURE);
	} else if (pid < 0) {
    	// Error forking
    	perror("Linux-Shell:");
  	} else {
    	// Parent process
    	do {
      		waitpid(pid, &status, WUNTRACED);
    	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
  	}
  	return 1;
}

int exit(char **args) {
  return 0;
}

char * read_line() {
	int bufsize = RL_BUFSIZE;
	char *buffer = (char *)malloc(sizeof(char) * bufsize);
	int position = 0;
	int c;
	if (!buffer) {
    	fprintf(stderr, "Allocation Error!\n");
    	exit(EXIT_FAILURE);
  	}

  	while(true) {
  		c = getchar();
  		if (c == EOF) {
      		exit(EXIT_SUCCESS);
    	} else if (c == '\n') {
      		buffer[position] = '\0';
      		return buffer;
    	} else {
      		buffer[position] = c;
    	}
    	position++;
    	
    	if (position >= bufsize) {
      		bufsize += RL_BUFSIZE;
      		buffer = (char *)realloc(buffer, bufsize);
      		if (!buffer) {
        		fprintf(stderr, "Allocation Error!\n");
        		exit(EXIT_FAILURE);
      		}
    	}
  	}
}

char **split_line(char *line) {
  int bufsize = TOK_BUFSIZE, position = 0;
  char **tokens = (char **)malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "Allocation Error!\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;
    
    if (position >= bufsize) {
      bufsize += TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = (char **)realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
		free(tokens_backup);
        fprintf(stderr, "Allocation Error\n!");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

int execute(char **args) {
  	int i;

  	if (args[0] == NULL) {
    	return 1;
  	}

  	for(i = 0; i < num_builtins(); i++) {
    	if (strcmp(args[0], builtin_str[i]) == 0) {
      		return (*builtin_func[i])(args);
    	}
  	}

  	return launch(args);
}

void loop() {
	char *line;
  	char **args;
  	int status;
  	do {
    	printf("> ");
    	line = read_line();
    	args = split_line(line);
    	status = execute(args);
    	free(line);
    	free(args);
  	} while (status);
}

int main() {
	loop();
}
