#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include<pwd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<termios.h>
#include<dirent.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MOVELEFT(y) printf("\033[%dD", (y))

#define RL_BUFSIZE 1024
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\n"
#define FILE_MAX 250
#define BUF_SZ 256
#define TRUE 1
#define FALSE 0

const char* COMMAND_EXIT = "exit";
const char* COMMAND_HELP = "help";
const char* COMMAND_CD = "cd";
const char* COMMAND_IN = "<";
const char* COMMAND_OUT = ">";
const char* COMMAND_PIPE = "|";
const char* COMMAND_MYLS = "myls";
const char* COMMAND_ALIAS = "alias";
const char* COMMAND_UNALIAS = "unalias";
const char* COMMAND_CAT = "cat";
char **args;

enum {
	RESULT_NORMAL,
	ERROR_FORK,
	ERROR_COMMAND,
	ERROR_WRONG_PARAMETER,
	ERROR_MISS_PARAMETER,
	ERROR_TOO_MANY_PARAMETER,
	ERROR_CD,
	ERROR_SYSTEM,
	ERROR_EXIT,

	/* 重定向的错误信息 */
	ERROR_MANY_IN,
	ERROR_MANY_OUT,
	ERROR_FILE_NOT_EXIST,
	
	/* 管道的错误信息 */
	ERROR_PIPE,
	ERROR_PIPE_MISS_PARAMETER
};

int cd(char **args);
int help(char **args);
int exit(char **args);
int myls(char **args);
int alias(char **args);
int unalias(char **args);
int cat(char **args);
int callCommandWithPipe(int left, int right);
int callCommandWithRedi(int left, int right);
int isCommandExist(const char* command);
int myjobs(char **args);
void set_prompt(char *prompt);//显示主机名
void history_setup();
void history_finish();
void init();
void loop();
int mytime(char **args);
int display_history_list(char **args);
int background=0;
int cmd_cnt;

char *builtin_str[] = {
	(char *)"cd",
	(char *)"help",
  	(char *)"exit",
  	(char *)"history",
	(char *)"myls",
	(char *)"mytime",
  	(char *)"myjobs",
	(char *)"alias",
	(char *)"unalias",
	(char *)"cat"
};

int num_builtins() {
  	return sizeof(builtin_str) / sizeof(char *);
}

void display_one_dimension(char *line) {
	while(*line != '\0') {
		printf("%c", *line++);
	}
	printf("\n");

}

void display(char **args) {
	int i=0;
	while(*(args+i) != NULL) {
		int j=0;
		while(*(*(args+i)+j) != '\0') {
			printf("%c", *(*(args+i)+j));
			j++;
		}
		printf("\n");
		i++;
	}
}

int (*builtin_func[]) (char **) = {
  	&cd,
  	&help,
  	&exit,
  	&display_history_list,
	&myls,
	&mytime,
    &myjobs,
	&alias,
	&unalias,
	&cat
};

//set the prompt
void set_prompt(char *prompt){
	char hostname[100];
	char cwd[100];
	char super = '#';
	//to cut the cwd by "/"	
	char delims[] = "/";	
	struct passwd* pwp;
	
	if(gethostname(hostname,sizeof(hostname)) == -1){
		//get hostname failed
		strcpy(hostname,"unknown");
	}//if
	//getuid() get user id ,then getpwuid get the user information by user id 
	pwp = getpwuid(getuid());	
	if(!(getcwd(cwd,sizeof(cwd)))){
		//get cwd failed
		strcpy(cwd,"unknown");	
	}//if
	char cwdcopy[100];
	strcpy(cwdcopy,cwd);
	char *first = strtok(cwdcopy,delims);
	char *second = strtok(NULL,delims);
	//if at home 
	if(!(strcmp(first,"home")) && !(strcmp(second,pwp->pw_name))){
		int offset = strlen(first) + strlen(second)+2;
		char newcwd[100];
		char *p = cwd;
		char *q = newcwd;

		p += offset;
		while(*(q++) = *(p++));
		char tmp[100];
		strcpy(tmp,"~");
		strcat(tmp,newcwd);
		strcpy(cwd,tmp);			
	}	
	
	if(getuid() == 0)//if super
		super = '#';
	else
		super = '$';
	sprintf(prompt, "\e[1;32m%s@%s\e[0m:\e[1;31m%s\e[0m%c",pwp->pw_name,hostname,cwd,super);	
	printf("%s",prompt);
}

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
    fprintf(stderr, "Expected argument to \"cd\" not found!\n");
  } else {
    if (chdir(args[1]) != 0) {
	printf("无文件或目录！\n");
      	perror("Linux-Shell:");
    }
  }
  return 1;
}


int exit(char **args) {
  printf("Bye Bye ! \n");
  char *argvlist[]={(char *)"/usr/local/bin/lolcat",(char *)"/home/zhuzhu/sheng/Linux-Shell/tri/exit.txt", NULL};
  execve(argvlist[0],argvlist,NULL);
  return 0;
}

char **readFileList(char *basePath) {
	int position = 0;
  	char **filename_pointer = (char **)malloc(FILE_MAX * sizeof(char*));
  	char *filename;

  	if (!filename) {
   	 	fprintf(stderr, "Allocation Error!\n");
    		exit(EXIT_FAILURE);
  	}
	

	DIR * dir;
	struct dirent *ptr;
	if((dir = opendir(basePath)) == NULL) {
		perror("Open Dir Error!\n");
		exit(1);
	}
	while((ptr = readdir(dir)) != NULL) {
		if(strcmp(ptr->d_name, ".") == 0||strcmp(ptr->d_name, "..") == 0) {
			continue;
		} else {
			filename = ptr->d_name;
			filename_pointer[position] = filename;	
			position++;
		}
	}
	filename_pointer[position] = NULL;
	closedir(dir);
	return filename_pointer;
}

int fileNum(char **filename_pointer) {
	int num = 0;
	while(*filename_pointer != NULL) {
		filename_pointer++;
		num++;
	}
	return num;
}

int myls(char **args) {
	DIR *dir;
	char basePath[250];
	memset(basePath, '\0', sizeof(basePath));
	getcwd(basePath, 249);
	char **filename_pointer = readFileList(basePath);
	printf("File Num:%d|\n", fileNum(filename_pointer));
	printf("----------\n");
	while(*filename_pointer != NULL) {
		printf("%s\n", *(filename_pointer));
		filename_pointer++;
	}
	return 1;
}

int alias(char **args) {
	FILE *fp = fopen("/home/tdye/Shell4Linux/Linux-Shell-master/src/etc/alias.alias", "a+");
	if(fp == NULL) {
		fprintf(stderr, "Error occurs while opening alias.alias\n!");
		return 1;

	}	
	int i=1;
	while(*(args+i) != NULL) {
		int j=0;
		while(*(*(args+i)+j) != '\0') {
			fputc(*(*(args+i)+j), fp);
			j++;
		}
		i++;
		fputc('\n', fp);
	}
	fclose(fp);
	return 1;
}

int unalias(char **args) {
	FILE *fp = fopen("/home/tdye/Shell4Linux/Linux-Shell-master/src/etc/alias.alias", "r+");
	int c;
	if(fp == NULL) {
		fclose(fp);
		FILE *wfp = fopen("/home/tdye/Shell4Linux/Linux-Shell-master/src/etc/alias.alias", "a");
		fclose(wfp);
		fp = fopen("/home/tdye/Shell4Linux/Linux-Shell-master/src/etc/alias.alias", "r+");
	}

	fseek(fp, 0, SEEK_SET);

	while(1) {
		int i=0;
		ftag: if(feof(fp)) {

			printf("No such alias command!\n");
			fclose(fp);
			return 1;
		}
		c = fgetc(fp);
		if( (int)c == -1) {

			printf("No such alias command!\n");
			fclose(fp);
			return 1;
		} 
		while(*(*(args+1)+i) != '\0') {
			while(c != '=') {
				if(*(*(args+1)+i) != c) {
					i = 0;
					while(c != '\n') {
						
						c = fgetc(fp);
					}
					
				
					goto ftag;
				}
				break;
			}
			i++;
			goto ftag;
		}
		if(*(*(args+1)+i) == '\0' && c == '=') {

			fseek(fp, -(i+1), SEEK_CUR);
			
			while(c != '\n') {
				fputc('\0', fp);
				c = fgetc(fp);
				fseek(fp, -1, SEEK_CUR);
			}
			fclose(fp);
			return 1;
		} else {
			printf("No such alias command!\n");
			fclose(fp);
			return 1;
		}
	}
	
}

char ** match(char **args) {
	FILE *fp = fopen("/home/tdye/Shell4Linux/Linux-Shell-master/src/etc/alias.alias", "r+");
	int c;
	if(fp == NULL) {
		FILE *wfp = fopen("/home/tdye/Shell4Linux/Linux-Shell-master/src/etc/alias.alias", "a");
		fclose(wfp);
	}

	fseek(fp, 0, SEEK_SET);

	while(1) {
		int i=0;
		ftag: if(feof(fp)) {
			fclose(fp);
			return args;
		}
		c = fgetc(fp);
		if( (int)c == -1) {
			fclose(fp);
			return args;
		} 
		while(*(*(args)+i) != '\0') {
			while(c != '=') {
				if(*(*(args)+i) != c) {
					i = 0;
					while(c != '\n') {
						c = fgetc(fp);
					}
					goto ftag;
				}
				break;
			}
			i++;
			goto ftag;
		}
		if(*(*(args)+i) == '\0' && c == '=') {
			int j=0;
			while((int)c != 10) {
				c = fgetc(fp);
				if((int)c != 39 && (int)c != 10) {
					*(*(args)+j) = c;
					j++;
				}
				
				
			}
			*(*(args)+j) = '\0';
			fclose(fp);
			return args;
		} else {
			printf("No such alias command!\n");
			fclose(fp);
			return args;
		}
	}
		
}

int cat(char **args) {
	FILE *fp = fopen(*(args+1), "r");
	char c;
	if(fp == NULL) {
		printf("No such file!\n");
		return 1;
	} else {
		while((c = fgetc(fp)) != EOF) {
			printf("%c", c);
		} 
		return 1;
	}

}

char getonechar(void) {
	struct termios stored_settings;
	struct termios new_settings;
	tcgetattr (0, &stored_settings);
	new_settings = stored_settings;
	new_settings.c_lflag &= ~(ICANON|ECHO);
	new_settings.c_cc[VTIME] = 0;
	new_settings.c_cc[VMIN] = 1;
	tcsetattr (0, TCSANOW, &new_settings);

	int ret = 0;
	char c;



	c = getchar();

	tcsetattr (0, TCSANOW, &stored_settings); 


	return c; 
}

int myjobs(char ** args){
	//可以使用ps命令来实现查看进程
	pid_t pid;
	pid=fork();//必须fork，否则会出现myshell退出这种奇怪的bug
	if(pid<0){
		perror("myshell: fork");
	}
	else if(pid==0){//子进程
		if(cmd_cnt>1){
			printf("myshell: jobs: incorrect use of jobs\n");
		}
		else{
			execlp("ps","ps","ax",NULL);//使用ps
		}
	}
	else{//父进程
		waitpid(pid,NULL,0);
	}
	return 1;
}

void init(){
	printf("hello master!\n");
	loop();
}

char * read_line() {
	int bufsize = RL_BUFSIZE;
	char *buffer = (char *)malloc(sizeof(char) * bufsize);
	char *tempBuffer = (char *)malloc(sizeof(char) * bufsize);;//为单条命令（被空格分割的命令）分配临时内存 ++
	int position = 0;
	int tempPosition = 0;
	background = 0;
	int c;
	if (!buffer) {
    	fprintf(stderr, "Allocation Error!\n");
    	exit(EXIT_FAILURE);
  	}

	int notSpaceNum = 0;
	int tabNum = 0;

	tag: while(true) {
  		c = getonechar();
  		if (c == EOF) {
    		exit(EXIT_SUCCESS);
    		} 
		else if((int)c == 127) {
				if(position <= 0) {
					continue;
				} else {
					putchar('\b');
					putchar(' ');
					MOVELEFT(1);
					position--;
					tempPosition--;
					notSpaceNum--;
					tabNum = 0;
				}		
		} 
		else if(c == '\n') {
			putchar(c);
      		if(buffer[position-1]=='&')
				{
					background = 1;   //background order
					buffer[--position] = '\0';
				}else	
					buffer[position] = '\0';
			add_history(buffer);
			tabNum = 0;
      			return buffer;
    		} 
		else if(c == '\t') {
			tabNum ++;
			tabNum = tabNum % 2;
			char basePath[250];
			memset(basePath, '\0', sizeof(basePath));
			getcwd(basePath, 249);
			char **filename_pointer = readFileList(basePath);
			if(tabNum == 1) {
				for (int i = 0; i < fileNum(filename_pointer); i++) {
					for(int j=0; j<tempPosition; j++) {
						if(*(tempBuffer+j) != *(filename_pointer[i]+j)) {
							break;			
						}
						if(j == tempPosition-1) {
							position = position - notSpaceNum;
							for(int k=0; k<notSpaceNum; k++) {
								putchar('\b');
								putchar(' ');
								MOVELEFT(1);
							}

							int t = 0;
							while(*(*(filename_pointer+i)+t) != '\0') {
								buffer[position] = *(*(filename_pointer+i)+t);
								t++;
								position++;
							}
							
							printf("%s", *(filename_pointer+i));
							goto tag;
										
						}

					} 
					
	  			}
			} else {
				printf("\n");
				for (int i = 0; i < fileNum(filename_pointer); i++) {
					for(int j=0; j<tempPosition; j++) {
						if(*(tempBuffer+j) != *(filename_pointer[i]+j)) {
							break;			
						}
						if(j == tempPosition-1) {
							printf("%s\n", *(filename_pointer+i));				
						}
					} 
					
	  			}
				tabNum = 0;
				return NULL;
			}


		} else {
			if((int)c == 32||c == '/') {
				tempPosition = 0;
				notSpaceNum = 0;
			} else {
				tempBuffer[tempPosition] = c;
				tempPosition++;
				notSpaceNum ++;		
			}
			putchar(c);
      			buffer[position] = c;
			
			tabNum = 0;
			position++;
			
			
    		}
 
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

char **split_line(char *line,int &commandNum) {
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
  	commandNum=position;
  	cmd_cnt=commandNum;
  	return tokens;
}

int isCommandExist(const char* command) { // 判断指令是否存在
	if (command == NULL || strlen(command) == 0) return FALSE;

	int result = TRUE;
	
	int fds[2];
	if (pipe(fds) == -1) {
		result = FALSE;
	} else {
		/* 暂存输入输出重定向标志 */
		int inFd = dup(STDIN_FILENO);
		int outFd = dup(STDOUT_FILENO);

		pid_t pid = vfork();
		if (pid == -1) {
			result = FALSE;
		} else if (pid == 0) {
			/* 将结果输出重定向到文件标识符 */
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			close(fds[1]);

			char tmp[BUF_SZ];
			sprintf(tmp, "command -v %s", command);
			system(tmp);
			exit(1);
		} else {
			waitpid(pid, NULL, 0);
			/* 输入重定向 */
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO);
			close(fds[0]);

			if (getchar() == EOF) { // 没有数据，意味着命令不存在
				result = FALSE;
			}
			
			/* 恢复输入、输出重定向 */
			dup2(inFd, STDIN_FILENO);
			dup2(outFd, STDOUT_FILENO);
		}
	}

	return result;
}

int launch(char **args,int commandNum) {
  	pid_t pid;
  	int status;
	display(args);
 	pid = fork();
  	if (pid == 0) {
    		// Child process
		/* 获取标准输入、输出的文件标识符 */
		int inFds = dup(STDIN_FILENO);
		int outFds = dup(STDOUT_FILENO);
		int result = callCommandWithPipe(0, commandNum);
		
		/* 还原标准输入、输出重定向 */
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(result);
    	 	if (execvp(args[0], args) == -1) {
			printf("%s\n", *args);
			printf("%s\n", *(args+1));
      	 		perror("Linux-Shell:");
			exit(EXIT_FAILURE);
    	 	}
    	 
	} else if (pid < 0) {
    		// Error forking
    		perror("Linux-Shell:");
  	} else {
    	// Parent process
    	do {
			if(background==0)
      			waitpid(pid, &status, WUNTRACED);
			else
				printf("pid : [\e[1;32m%d] \n",pid);
    	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
  	}
  	return 1;
}

int execute(char **args,int commandNum) {
  	int i;

  	if (args[0] == NULL) {
    	return 1;
  	}
	
  	for(i = 0; i < num_builtins(); i++) {
    		if (strcmp(args[0], builtin_str[i]) == 0) {
      			return (*builtin_func[i])(args);
    		}
  	}
	match(args);//args is stored in heap memory, equal to class variable
	for(i = 0; i < num_builtins(); i++) {
    		if (strcmp(args[0], builtin_str[i]) == 0) {
      			return (*builtin_func[i])(args);
    		}
  	}
  	return launch(args,commandNum);
}

void loop() {
	char *line;
	char prompt[100];//数组不要开太小 会溢出
  	int status;
	history_setup();
  	do {
		set_prompt(prompt);
    	//printf("bug>: ");
    	line = read_line();
		int tmp=0;
		int &commandNum=tmp;
    	args = split_line(line,commandNum);
    	status = execute(args,commandNum);
    	free(line);
    	free(args);
  	} while (status);
	history_finish();
}

int callCommand(int commandNum) { // 给用户使用的函数，用以执行用户输入的命令
	pid_t pid = fork();
	if (pid == -1) {
		return ERROR_FORK;
	} else if (pid == 0) {
		/* 获取标准输入、输出的文件标识符 */
		int inFds = dup(STDIN_FILENO);
		int outFds = dup(STDOUT_FILENO);

		int result = callCommandWithPipe(0, commandNum);
		
		/* 还原标准输入、输出重定向 */
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(result);
	} else {
		int status;
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}
}

int callCommandWithPipe(int left, int right) { // 所要执行的指令区间[left, right)，可能含有管道
	if (left >= right) return RESULT_NORMAL;
	/* 判断是否有管道命令 */
	int pipeIdx = -1;
	for (int i=left; i<right; ++i) {
		if (strcmp(args[i], COMMAND_PIPE) == 0) {
			pipeIdx = i;
			break;
		}
	}
	if (pipeIdx == -1) { // 不含有管道命令
		return callCommandWithRedi(left, right);
	} else if (pipeIdx+1 == right) { // 管道命令'|'后续没有指令，参数缺失
		return ERROR_PIPE_MISS_PARAMETER;
	}

	/* 执行命令 */
	int fds[2];
	if (pipe(fds) == -1) {
		return ERROR_PIPE;
	}
	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) { // 子进程执行单个命令
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO); // 将标准输出重定向到fds[1]
		close(fds[1]);
		
		result = callCommandWithRedi(left, pipeIdx);
		exit(result);
	} else { // 父进程递归执行后续命令
		int status;
		waitpid(pid, &status, 0);
		int exitCode = WEXITSTATUS(status);
		
		if (exitCode != RESULT_NORMAL) { // 子进程的指令没有正常退出，打印错误信息
			char info[4096] = {0};
			char line[BUF_SZ];
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO); // 将标准输入重定向到fds[0]
			close(fds[0]);
			while(fgets(line, BUF_SZ, stdin) != NULL) { // 读取子进程的错误信息
				strcat(info, line);
			}
			printf("%s", info); // 打印错误信息
			
			result = exitCode;
		} else if (pipeIdx+1 < right){
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO); // 将标准输入重定向到fds[0]
			close(fds[0]);
			result = callCommandWithPipe(pipeIdx+1, right); // 递归执行后续指令
		}
	}

	return result;
}

int callCommandWithRedi(int left, int right) { // 所要执行的指令区间[left, right)，不含管道，可能含有重定向
	if (!isCommandExist(args[left])) { // 指令不存在
		return ERROR_COMMAND;
	}	

	/* 判断是否有重定向 */
	int inNum = 0, outNum = 0;
	char *inFile = NULL, *outFile = NULL;
	int endIdx = right; // 指令在重定向前的终止下标

	for (int i=left; i<right; ++i) {
		if (strcmp(args[i], COMMAND_IN) == 0) { // 输入重定向
			++inNum;
			if (i+1 < right)
				inFile = args[i+1];
			else return ERROR_MISS_PARAMETER; // 重定向符号后缺少文件名

			if (endIdx == right) endIdx = i;
		} else if (strcmp(args[i], COMMAND_OUT) == 0) { // 输出重定向
			++outNum;
			if (i+1 < right)
				outFile = args[i+1];
			else return ERROR_MISS_PARAMETER; // 重定向符号后缺少文件名
				
			if (endIdx == right) endIdx = i;
		}
	}
	/* 处理重定向 */
	if (inNum == 1) {
		FILE* fp = fopen(inFile, "r");
		if (fp == NULL) // 输入重定向文件不存在
			return ERROR_FILE_NOT_EXIST;
		
		fclose(fp);
	}
	
	if (inNum > 1) { // 输入重定向符超过一个
		return ERROR_MANY_IN;
	} else if (outNum > 1) { // 输出重定向符超过一个
		return ERROR_MANY_OUT;
	}

	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) {
		/* 输入输出重定向 */
		if (inNum == 1)
			freopen(inFile, "r", stdin);
		if (outNum == 1)
			freopen(outFile, "w", stdout);

		/* 执行命令 */
		char* comm[BUF_SZ];
		for (int i=left; i<endIdx; ++i)
			comm[i] = args[i];
		comm[endIdx] = NULL;
		execvp(comm[left], comm+left);
		exit(errno); // 执行出错，返回errno
	} else {
		//后台处理子程序出错暂未处理
		int status;
		if(background==0){
			waitpid(pid, &status, 0);
			int err = WEXITSTATUS(status); // 读取子进程的返回码

			if (err) { // 返回码不为0，意味着子进程执行出错，用红色字体打印出错信息
				printf("\e[31;1mError: %s\n\e[0m", strerror(err));
			}
		}	
		else
			printf("pid : [%d] \n",pid);
		
	}


	return result;
}

int mytime(char** args){
	int weekday;
	int month;
	time_t tvar;
	struct tm *tp;
	time(&tvar);
	tp=localtime(&tvar);//获取本地时间
	weekday=tp->tm_wday;
	char *wday[]={(char *)"Mon ",(char *)"Tues ",(char *)"Wed ",(char *)"Thur ",(char *)"Fri ",(char *)"Sat ",(char *)"Sun "};
	char *wmonth[]={(char *)"Jan ",(char *)"Feb ",(char *)"Mar ",(char *)"Apr ",(char *)"May ",(char *)"Jun ",(char *)"Jul ",(char *)"Aug ",(char *)"Sep ",(char *)"Oct ",(char *)"Nov ",(char *)"Dec "};
	printf("%s",wday[weekday-1]);
	month=1+tp->tm_mon;//必须要加1，经过查阅资料：tm_mon比实际的值少了1
	printf("%s",wmonth[month-1]);
	printf("%d ",tp->tm_mday);//日期
	printf("%d:",tp->tm_hour);//小时
	printf("%d:",tp->tm_min);//分钟
	printf("%d ",tp->tm_sec);//秒
	printf("CST ");//CST，意思是China Standard Time
	printf("%d\n",1900+tp->tm_year);//必须加上1900，返回的值并不是完整的年份，比真实值少了1900
	return 1;
}

void history_setup(){
	using_history();
	stifle_history(50);
	read_history("/tmp/msh_history");	
}

void history_finish(){
	append_history(history_length, "/tmp/msh_history");
	history_truncate_file("/tmp/msh_history", history_max_entries);

}

int display_history_list(char ** args){
	HIST_ENTRY** h = history_list();
	if(h) {
		int i = 0;
		while(h[i]) {
			printf("%d: %s\n", i, h[i]->line);
			i++;
		}
	}
	return 1;
}

int main() {
	init();
}
