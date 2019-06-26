#include<stdio.h>
#include<iostream>
#include<string>
#include<stdlib.h>

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

#define MOVELEFT(y) printf("\033[%dD", (y))//将屏幕上当前光标回退y个位置

#define RL_BUFSIZE 1024
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\n"
#define FILE_MAX 250
#define BUF_SZ 256
#define TRUE 1
#define FALSE 0

#define CLOSE "\001\033[0m\002"                 // 关闭所有属性
#define BLOD  "\001\033[1m\002"                 // 强调、加粗、高亮
#define BEGIN(x,y) "\001\033["#x";"#y"m\002"    // x: 背景，y: 前景

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
char ** args_ ;
char prompt[100];//数组不要开太小 会溢出
char hostname[100];//保存用户还主机姓名
char cwd[100];//保存用户当前目录
int commandnum;


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
//自定义命令
int cd(char **args); 
int help(char **args);//用户帮助，展示该程序的基本功能
int exit(char **args);
int myls(char **args);//实现递归显示当前目录下的所有文件
int alias(char **args);//实现别名功能
int unalias(char **args);//删除别名的记录项
int cat(char **args);//显示文件内容
int pipeCommand(int left, int right);//管道指令的实现
int redirectCommand(int left, int right);//处理带有重定向的命令
int isCommandExist(const char* command);//判断指令是否存在
int myjobs(char **args);//实现展示当前后台进程
void set_prompt(char *prompt);//显示主机名
void history_setup();//命令历史功能初始化
void history_finish();//释放历史文件缓存
void init();//初始化整个系统
void loop();//不断循环等待用户输入命令
int mytime(char **args);//展示当前系统时间
int display_history_list(char **args);//展示用户之前输入过的命令历史
int background=0;//用于标记当前命令是否为背景命令
int cmd_cnt;//记录用户输入的命令总数

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
	(char *)"mycat"
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

void display(char **args) {	  //展示用户输入的命令
	int i=0;
	while(*(args+i) != NULL) {   //直到args数组不为空就循环
		int j=0;
		while(*(*(args+i)+j) != '\0') {  //遇到字符串终结符停止
			printf("%c", *(*(args+i)+j));//循环输出args数组中保存的字符
			j++; 
		}
		printf("\n");//换行显示下来一个命令
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

//设置用户进入系统的提示信息
void set_prompt(char *prompt){
	
	char super = '#';
	//遇到‘/’来截断当前字符串
	char delims[] = "/";
	struct passwd* pwp;
	
	if(gethostname(hostname,sizeof(hostname)) == -1){
		//主机名获取失败
		strcpy(hostname,"unknown");
	}//
	//getuid()获取用户id , 利用getpwuid 通过用户id 获取用户信息
	pwp = getpwuid(getuid());	
	if(!(getcwd(cwd,sizeof(cwd)))){
		//获取当前用户目录失败
		strcpy(cwd,"unknown");
	}//
	char cwdcopy[100];
	strcpy(cwdcopy,cwd);
	char *first = strtok(cwdcopy,delims);  //通过delim分割字符串
	char *second = strtok(NULL,delims);
	//如果当前为home目录
	if(!(strcmp(first,"home")) && !(strcmp(second,pwp->pw_name))){
		int offset = strlen(first) + strlen(second)+2;
		char newcwd[100];
		char *p = cwd;	//指针指向cwd数组
		char *q = newcwd;  

		p += offset;
		while(*(q++) = *(p++));
		char tmp[100];
		strcpy(tmp,"~");
		strcat(tmp,newcwd);
		strcpy(cwd,tmp);	//拷贝临时数据到工作数组		
	}	
	
	if(getuid() == 0)//如果用户权限为root
		super = '#';
	else
		super = '$';
	//打印用户主机和当前工作目录
	//sprintf(prompt, "\e[1;32m%s@%s\e[0m:\e[1;31m%s\e[0m%c",pwp->pw_name,hostname,cwd,super);
	sprintf(prompt, "\001\033[1;32m\002%s@%s\001\033[0m\002:\001\033[1;31m\002%s\001\033[0m\002%c",pwp->pw_name,hostname,cwd,super);
	//在输出的头和尾加 \001 and \002
	//printf("%s",prompt);
}

int help(char **args) {
  	int i;
  	printf("Type program names and arguments, and hit enter.\n");
  	printf("The following are built in:\n");
	//将之前保存功能的数组中的值打印出来
  	for (i = 0; i < num_builtins(); i++) {
    	printf("  %s\n", builtin_str[i]);
  	}
	//提示用户有哪些功能
  	printf("Use the man command for information on other programs.\n");
  	return 1;
}

int cd(char **args) {
  if(args[1] == NULL) {
    fprintf(stderr, "Expected argument to \"cd\" not found!\n"); //命令不含参数则向用户报错
  } else {
    if (chdir(args[1]) != 0) {//用户输入目录名，但是该目录不存在
	printf("无该目录！\n");
      	perror("Linux-Shell:");
    }
  }
  return 1;
}

int exit(char **args) {
  printf("Bye Bye ! \n"); //输出退出提示
  //调用子程序显示退出界面
  char *argvlist[]={(char *)"/usr/local/bin/lolcat",(char *)"/home/zhuzhu/sheng/Linux-Shell/tri/exit.txt", NULL};
  execve(argvlist[0],argvlist,NULL);
  return 0;
}

char **readFileList(char *basePath) {
	int position = 0;
  	char **filename_pointer = (char **)malloc(FILE_MAX * sizeof(char*));//开辟一段空间，保存指向文件名的指针
  	char *filename;//指向文件名字符串的指针变量

  	if (!filename) {
   	 	fprintf(stderr, "Allocation Error!\n");
    		exit(EXIT_FAILURE);
  	}
	

	DIR * dir;
	struct dirent *ptr;
	if((dir = opendir(basePath)) == NULL) {//打开给定文件目录，返回一指向该文件目录的指针
		perror("Open Dir Error!\n");
		exit(1);
	}
	while((ptr = readdir(dir)) != NULL) {//顺序读取文件目录，返回一指向文件索引结构体的指针
		if(strcmp(ptr->d_name, ".") == 0||strcmp(ptr->d_name, "..") == 0) {
			continue;
		} else {
			filename = ptr->d_name;//获取文件名字符串
			filename_pointer[position] = filename;//将指向文件名字符串的指针存放在二级指针中
			position++;
		}
	}
	filename_pointer[position] = NULL;//顺序访问完当前目录下的所有文件，以NULL分界，方便后续访问和统计
	closedir(dir);//文件打开后，在访问完后必须关闭
	return filename_pointer;
}

int fileNum(char **filename_pointer) {
	int num = 0;
	while(*filename_pointer != NULL) {
		filename_pointer++;//根据文件名个数 ，返回当前文件目录下的文件数量
		num++;
	}
	return num;
}

int myls(char **args) {
	DIR *dir;
	char basePath[250];//开辟一段空间，保存当前文件路径
	memset(basePath, '\0', sizeof(basePath));//将新开辟空间赋初值\0
	getcwd(basePath, 249);//获取当前文件目录
	char **filename_pointer = readFileList(basePath);//调用readFileList函数，该函数返回存储当前文件目录下的所有文件名字符串指针的一块空间的首地址
	printf("File Num:%d|\n", fileNum(filename_pointer));//调用fileNum函数，清点当前文件目录下的文件数量
	printf("----------\n");
	while(*filename_pointer != NULL) {
		printf("%s\n", *(filename_pointer));//根据readFileList返回的指针，顺序打印每个文件名
		filename_pointer++;
	}
	return 1;
}

int alias(char **args) {
	FILE *fp = fopen("/home/zhuzhu/下载/Linux-Shell-master/src/etc/alias.alias", "a+");//以追加的形式打开alias.alias文件，如果该文件在该文件夹下不存在，则自动创建该文件
	if(fp == NULL) {
		fprintf(stderr, "Error occurs while opening alias.alias\n!");//文件打开失败
		return 1;

	}	
	int i=1;
	// while(*(args+i) != NULL) {//获取重命名命令行，例如alias dirrr='cd'，该部分获取dirrr='cd'，并将其追加到alias.alias文件中，每条记录以\n分界
	// 	int j=0;
	// 	while(*(*(args+i)+j) != '\0') {
	// 		fputc(*(*(args+i)+j), fp);
	// 		j++;
	// 	}
	// 	i++;
	// 	fputc('\n', fp);//每条记录以\n分界
	// }
	while(*(args+i) != NULL) {//获取重命名命令行，例如alias dirrr='cd'，该部分获取dirrr='cd'，并将其追加到alias.alias文件中，每条记录以\n分界
		int j=0;
		while(*(*(args+i)+j) != '\0') {
			fputc(*(*(args+i)+j), fp);
			j++;
		}
		i++;
		fputc(' ',fp);
	}
	fputc('\n', fp);//每条记录以\n分界
	fclose(fp);//关闭alias.alias文件
	return 1;
}

int unalias(char **args) {
	FILE *fp = fopen("/home/zhuzhu/下载/Linux-Shell-master/src/etc/alias.alias", "r+");//以读写的方式打开alias.alias文件
	int c;
	if(fp == NULL) {//如果该文件尚不存在，则创建该文件，同时也可以直接返回错误信息给用户，因为用户尚未重命名命令
		fclose(fp);
		FILE *wfp = fopen("/home/zhuzhu/下载/Linux-Shell-master/src/etc/alias.alias", "a");
		fclose(wfp);
		fp = fopen("/home/zhuzhu/下载/Linux-Shell-master/src/etc/alias.alias", "r+");
	}

	fseek(fp, 0, SEEK_SET);//将访问指针fp置于文件头

	while(1) {
		int i=0;
		ftag: if(feof(fp)) {//如果已经访问到文件末尾，尚未检索到用户输入的需要取消重命名的命令，则给用户返回信息No such alias command!

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
		while(*(*(args+1)+i) != '\0') {//针对每一行重命名记录进行匹配，只要非完全匹配则跳过本行进行下一行匹配
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
		if(*(*(args+1)+i) == '\0' && c == '=') {//等于号之前检索到完全匹配用户输入的待取消的指令

			fseek(fp, -(i+1), SEEK_CUR);//将当前访问指针fp前移至该条记录的首地址
			
			while(c != '\n') {//从该条记录的首地址开始，逐个字符进行覆盖为空，直至该条记录的末端\n
				fputc('\0', fp);
				c = fgetc(fp);
				fseek(fp, -1, SEEK_CUR);
			}
			fclose(fp);//关闭该alias.alias文件
			return 1;
		} else {
			printf("No such alias command!\n");
			fclose(fp);
			return 1;
		}
	}
	
}

char ** match(char **args) {//当用户输入非内置的命令时，首先检索alias.alias文件,将该重命名命令翻译成内置命令
	FILE *fp = fopen("/home/zhuzhu/下载/Linux-Shell-master/src/etc/alias.alias", "r+");
	int c;
	if(fp == NULL) {
		printf("fp NULL!");
		FILE *wfp = fopen("/home/zhuzhu/下载/Linux-Shell-master/src/etc/alias.alias", "a");
		fclose(wfp);
	}

	fseek(fp, 0, SEEK_SET);
	char str[50];
	while(!feof(fp)){
		fgets(str,50,fp);
		int i=0;
		while(str[i]!=char(NULL)&&*(*(args)+i)!=char(NULL)&&str[i]==*(*(args)+i)&&str[i+1]!='=')
			i++;
		if(str[i+1]=='='){
			//printf("matched!!!\n");
			i+=3;//引号
			// printf("%c\n",str[i]);
			// printf("%c\n",str[i+1]);
			char **tokens = (char **)malloc(FILE_MAX * sizeof(char*));
			
			int j=0,k=0;
			while(int(str[i])!=39){
				char *token=(char *)malloc(sizeof(char)*10);
				j=0;
				while(int(str[i])!=39&&str[i]!=' '){
					token[j++]=str[i++];
				}
				if(str[i]==' ')
					{
						token[j]='\0';
						tokens[k++]=token;
						i++;
					}
					else {
						token[j]='\0';
						tokens[k++]=token;
					}
			}
			tokens[k]=NULL;
			commandnum=k;
			return tokens;
		}
	}
	//printf("no response!\n");
	return args;
		
}

int cat(char **args) {//打开文件，并依次读取文件内容，输出到终端
	FILE *fp = fopen(*(args+1), "r");
	char c;
	if(fp == NULL) {
		printf("No such file!\n");//需要cat的文件不存在
		return 1;
	} else {
		while((c = fgetc(fp)) != EOF) {
			printf("%c", c);//打印文件内容到终端
		} 
		return 1;
	}

}

char getonechar(void) {//将输入缓冲区置一，每次读取一个字符进行判断是否需要命令补全，准确说就是为了实时检测TAB键
	struct termios stored_settings;
	struct termios new_settings;
	tcgetattr (0, &stored_settings);//获取原输入缓冲设置
	new_settings = stored_settings;//将原输入缓冲设置赋值给一新结构体
	new_settings.c_lflag &= ~(ICANON|ECHO);
	new_settings.c_cc[VTIME] = 0;//输入缓冲区响应时间置0，立即回显到屏幕
	new_settings.c_cc[VMIN] = 1;//将输入缓冲区大小置为1
	tcsetattr (0, TCSANOW, &new_settings);//使修改后输入缓冲设置生效

	int ret = 0;
	char c;



	c = getchar();//获取单个字符

	tcsetattr (0, TCSANOW, &stored_settings); //将原输入缓冲设置复位


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
	pid_t pid = fork();
  	if (pid == 0) {
		char *argvlist[]={(char *)"/usr/local/bin/lolcat",(char *)"/home/zhuzhu/sheng/Linux-Shell/tri/wel.txt", NULL};
		execve(argvlist[0],argvlist,NULL);
	} else if (pid < 0) {
    	// Error forking
    	perror("Linux-Shell:");
  	} else {
    	// Parent process
		int status;
		waitpid(pid, &status, 0);
    	loop();
  	}
}

char * read_line() {
	int bufsize = RL_BUFSIZE;
	char *buffer = (char *)malloc(sizeof(char) * bufsize);//为当前输入指令分配内存
	char *tempBuffer = (char *)malloc(sizeof(char) * bufsize);;//为单条命令（被空格分割的命令）分配临时内存 ++
	int position = 0; //保存指令长度
	int tempPosition = 0;//当用户输入TAB键时，保存当前输入参数字段（以空格分开）的大小或者字符位置
	background = 0;//标记该指令是否为后台函数
	int c;
	if (!buffer) {
    	fprintf(stderr, "Allocation Error!\n");
    	exit(EXIT_FAILURE);
  	}

	int notSpaceNum = 0;//非空格个数，即当前输入字段的字符数量，用于中断光标回退
	int tabNum = 0;//检测TAB键个数，0（2）个时返回当前文件夹下的所有文件，1个时，匹配文件名

	tag: while(true) {
  		c = getonechar();
  		if (c == EOF) {
    		exit(EXIT_SUCCESS);
    		} 
		else if((int)c == 127) {//当用户键入backspace时，用position控制光标回退，防止屏幕光标回退越界
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
      		if(buffer[position-1]=='&')//命令含有后台标记
				{
					background = 1;   //标记为后台函数
					buffer[--position] = '\0';//修改之前标记位为字符串终止符
				}else	
					buffer[position] = '\0';//终结字符串
			add_history(buffer);//将该条命令添加到用户缓存之中
			write_history("/home/zhuzhu/下载/Linux-Shell-master/src/etc/msh_history.txt");
			tabNum = 0;
      			return buffer;
    		} 
		else if(c == '\t') {
			tabNum ++;
			tabNum = tabNum % 2;//检测当前用户键入TAB键的数量
			char basePath[250];//获取当前文件目录
			memset(basePath, '\0', sizeof(basePath));
			getcwd(basePath, 249);
			char **filename_pointer = readFileList(basePath);
			if(tabNum == 1) {//当用户输入一次TAB时，顺序匹配当前文件目录下的所有文件，返回最大匹配的文件名
				for (int i = 0; i < fileNum(filename_pointer); i++) {
					for(int j=0; j<tempPosition; j++) {
						if(*(tempBuffer+j) != *(filename_pointer[i]+j)) {
							break;			
						}
						if(j == tempPosition-1) {
							position = position - notSpaceNum;//将当前输入的非完全文件名清空
							for(int k=0; k<notSpaceNum; k++) {
								putchar('\b');
								putchar(' ');
								MOVELEFT(1);
							}

							int t = 0;
							while(*(*(filename_pointer+i)+t) != '\0') {//将匹配到的文件名送赋值到输入参数的相应位置
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
				printf("\n");//用户键入两次TAB键，将当前文件夹下所有文件名显示到屏幕上
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
			if((int)c == 32||c == '/') {//当用户需要执行可执行文件时，匹配/
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
 
    	if (position >= bufsize) {  //如果之前 分配的数组空间不够保存命令
      		bufsize += RL_BUFSIZE;
      		buffer = (char *)realloc(buffer, bufsize); //再次分配一定空间用于保存
      		if (!buffer) {
        		fprintf(stderr, "Allocation Error!\n");
        		exit(EXIT_FAILURE);
      		}
    	}
  	}
}

char **split_line(char *line,int &commandNum) {
  	int bufsize = TOK_BUFSIZE, position = 0;   //设定 缓冲区的大小和当前位置指针
  	char **tokens = (char **)malloc(bufsize * sizeof(char*));  //指针数组用于指向分割后的命令
  	char *token, **tokens_backup; //一级指针暂存单个单词

  	if (!tokens) {  //内存分配失败报错
    	fprintf(stderr, "Allocation Error!\n");
    	exit(EXIT_FAILURE);  //失败退出
  	}

  	token = strtok(line, TOK_DELIM);  //根据指定字符分割命令
  	while (token != NULL) {
    	tokens[position] = token;
    	position++; //位置向前一步
    
    	if (position >= bufsize) { //如果超过指定内存大小
      		bufsize += TOK_BUFSIZE;
      		tokens_backup = tokens;
      		tokens = (char **)realloc(tokens, bufsize * sizeof(char*)); //再次分配新的空间
      		if (!tokens) {
				free(tokens_backup);
        		fprintf(stderr, "Allocation Error\n!"); //分配失败报错
        		exit(EXIT_FAILURE);
      		}
    	}

    	token = strtok(NULL, TOK_DELIM);  //再次调用strtok继续分割
  	}
  	tokens[position] = NULL;
  	commandNum=position; //保存个数用于函数间通信
  	cmd_cnt=commandNum; //保存个数
  	return tokens;  //返回分割好的命令
}

int isCommandExist(const char* command) { // 判断指令是否存在
	if (command == NULL || strlen(command) == 0) return FALSE;

	int result = TRUE;
	
	int fds[2];  //用于指向管道的读端和写端
	if (pipe(fds) == -1) {  //管道初始化失败报错
		result = FALSE;
	} else {
		/* 暂存输入输出重定向标志 */
		int inFd = dup(STDIN_FILENO);  //将标准输入暂时保存到临时变量
		int outFd = dup(STDOUT_FILENO);  //将标准输出暂时保存到临时变量

		pid_t pid = vfork(); //创建子进程执行指令
		if (pid == -1) {  //fork失败报错
			result = FALSE;
		} else if (pid == 0) { 
			//子进程调用返回
			/* 将结果输出重定向到文件标识符 */
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);  //复制标准输出到管道的写端
			close(fds[1]);

			char tmp[BUF_SZ];
			sprintf(tmp, "command -v %s", command); //打印系统内部指令
			system(tmp);
			exit(1);
		} else {
			waitpid(pid, NULL, 0); //父进程等待子进程的执行完成
			/* 输入重定向 */
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO); //将标准输入重定向到管道的读端
			close(fds[0]);

			if (getchar() == EOF) { // 没有数据，意味着命令不存在
				result = FALSE;
			}
			
			/* 恢复输入、输出重定向 */
			dup2(inFd, STDIN_FILENO);
			dup2(outFd, STDOUT_FILENO);
		}
	}

	return result; //返回执行状态
}

int launch(char **args,int commandNum) {  //执行其他指令的运行
  	pid_t pid;  //创建新的进程号变量
  	int status;
	//printf("launch:");
	//display(args);  //打印用户输入的命令
 	pid = fork();  //创建子进程
  	if (pid == 0) {
    		// Child process
		/* 获取标准输入、输出的文件标识符 */
		int inFds = dup(STDIN_FILENO);
		int outFds = dup(STDOUT_FILENO);
		int result = pipeCommand(0, commandNum);  //调用含管道的执行函数
		
		/* 还原标准输入、输出重定向 */
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(result);
    	 
	} else if (pid < 0) {
    		// fork执行失败
    		perror("Linux-Shell:");
  	} else {
    	// 父进程返
		// if(background==0)  //不是后台进程则等待执行结果
			waitpid(pid, &status, WUNTRACED);
		// else
		// 	printf("pid : [\e[1;32m%d] \n",pid); //打印后台进程的进程号
  	}
  	return 1;
}

int execute(char **args,int commandNum) { //分析执行用户输入的命令
  	int i;
    args_ = (char **)malloc(TOK_BUFSIZE * sizeof(char*));
  	if (args[0] == NULL) {
    	return 1;
  	}
	//循环遍历检查改命令是否出现在之前的数组之中
  	for(i = 0; i < num_builtins(); i++) {
    		if (strcmp(args[0], builtin_str[i]) == 0) { //与标准字符串进行比较
      			return (*builtin_func[i])(args);  //返回符合的函数调用
    		}
  	}
	args_ = match(args);//args 被储存在一个堆中 等价于类变量
	//display(args_);
	for(i = 0; i < num_builtins(); i++) {
    		if (strcmp(args_[0], builtin_str[i]) == 0) {
      			return (*builtin_func[i])(args_);
    		}
  	}
	if(args==args_)
		return launch(args,commandNum);
  	return launch(args_,commandnum); //若不出现在该数组中调用launch函数执行
}
char* check_line(char *line){
	int i=0;
	background=0;
	while(line[i]!=(char)NULL){
		if(line[i+1]==(char)NULL){
			if(line[i]==' '){
				//printf("Error:Redundent Space !");
				return line;
			}
			if(line[i]=='&'){
				background=1;
				while(line[--i]==' ');
                char * tmp=(char * )malloc(sizeof(char)*(i+1));
                for(int j=0;j<=i;j++)
                    tmp[j]=line[j];
                tmp[i+1]=(char)NULL;
                return tmp;	
			}		
			
		}
		i++;
	}
	return line;
}
void loop() { //循环等待用户的指令
	char *line;
  	int status;  //执行的状态
	history_setup(); //历史初始化 
  	do {
		set_prompt(prompt); //设置提示信息
    	//line = read_line();  //读取用户输入的字符串命令
		line=readline(prompt);
		line=check_line(line);
		int tmp=0;
		int &commandNum=tmp;  //用于临时保存通信变量
    	args = split_line(line,commandNum); //调用分割函数分析命令
    	status = execute(args,commandNum); //status保存函数调用后返回的结果
    	free(line);  //释放内存
    	free(args);
		//free(args_);
  	} while (status);
	history_finish();  //历史保存空间回收
}

int callCommand(int commandNum) { // 给用户使用的函数，用以执行用户输入的命令
	pid_t pid = fork();
	if (pid == -1) {
		return ERROR_FORK;
	} else if (pid == 0) {
		/* 获取标准输入、输出的文件标识符 */
		int inFds = dup(STDIN_FILENO);  //获取标准输入
		int outFds = dup(STDOUT_FILENO);  //获取标准输出

		int result = pipeCommand(0, commandNum);  //调用管道指令处理管道
		
		/* 还原标准输入、输出重定向 */
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(result);
	} else {
		int status;
		waitpid(pid, &status, 0); //等待子进程的返回结果
		return WEXITSTATUS(status);
	}
}

int pipeCommand(int left, int right) { // 所要执行的指令区间[left, right)，可能含有管道
	if (left >= right) return RESULT_NORMAL;
	/* 判断是否有管道命令 */
	int pipeIdx = -1;   //设置指向当前字符的变量
	for (int i=left; i<right; ++i) {  //从左至右依次比较
		if (strcmp(args_[i], COMMAND_PIPE) == 0) {
			pipeIdx = i;
			break;
		}
	}
	if (pipeIdx == -1) { // 不含有管道命令
		return redirectCommand(left, right);
	} else if (pipeIdx+1 == right) { // 管道命令'|'后续没有指令，参数缺失
		return ERROR_PIPE_MISS_PARAMETER;
	}

	/* 执行命令 */
	int fds[2];
	if (pipe(fds) == -1) {
		return ERROR_PIPE;
	}
	int result = RESULT_NORMAL;
	pid_t pid = vfork();   //创建新的子进程
	if (pid == -1) {  //进程创建失败
		result = ERROR_FORK;
	} else if (pid == 0) { // 子进程执行单个命令
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO); // 将标准输出重定向到fds[1]
		close(fds[1]); //关闭管道读端
		
		result = redirectCommand(left, pipeIdx);  //可能还会有重定向的指令
		exit(result);
	} else { // 父进程递归执行后续命令
		int status;
		waitpid(pid, &status, 0);  //等待子进程的执行
		int exitCode = WEXITSTATUS(status);
		
		if (exitCode != RESULT_NORMAL) { // 子进程的指令没有正常退出，打印错误信息
			char info[4096] = {0};   //初始化数组
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
			result = pipeCommand(pipeIdx+1, right); // 递归执行后续指令
		}
	}

	return result;
}

int redirectCommand(int left, int right) { // 所要执行的指令区间[left, right)，不含管道，可能含有重定向
	if (!isCommandExist(args_[left])) { // 指令不存在
		return ERROR_COMMAND;
	}	

	/* 判断是否有重定向 */
	int inNum = 0, outNum = 0;
	char *inFile = NULL, *outFile = NULL;
	int endIdx = right; // 指令在重定向前的终止下标

	for (int i=left; i<right; ++i) {
		if (strcmp(args_[i], COMMAND_IN) == 0) { // 输入重定向
			++inNum;
			if (i+1 < right)
				inFile = args_[i+1];
			else return ERROR_MISS_PARAMETER; // 重定向符号后缺少文件名

			if (endIdx == right) endIdx = i;
		} else if (strcmp(args_[i], COMMAND_OUT) == 0) { // 输出重定向
			++outNum;
			if (i+1 < right)
				outFile = args_[i+1];
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
			comm[i] = args_[i];
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
			printf("pid : [\e[1;31m%d] \n",pid);
		
	}


	return result;
}

int mytime(char** args){
	int weekday;  //临时变量保存当前周几
	int month; //临时变量保存当前月份
	time_t tvar;  //系统保存时间的结构体
	struct tm *tp;
	time(&tvar);
	tp=localtime(&tvar);//获取本地时间
	weekday=tp->tm_wday;
	//索引返回当前的日期信息
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
	using_history(); //调用函数启动历史保存
	stifle_history(50);
	read_history("/home/zhuzhu/下载/Linux-Shell-master/src/etc/msh_history.txt");	//设置缓存路径
}

void history_finish(){
	append_history(history_length, "/home/zhuzhu/下载/Linux-Shell-master/src/etc/msh_history.txt");  //结束历史数据的保存
	history_truncate_file("/home/zhuzhu/下载/Linux-Shell-master/src/etc/msh_history.txt", history_max_entries);

}

int display_history_list(char ** args){
	HIST_ENTRY** h = history_list(); //获取保存历史的缓存数组
	if(h) {
		int i = 0;
		while(h[i]) {
			printf("%d: %s\n", i, h[i]->line);  //依次显示之前输入过得历史命令
			i++;
		}
	}
	return 1; //返回调用
}

int main() { //主函数
	
	init();
}
