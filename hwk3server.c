#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <sys/stat.h>
#include <dirent.h>

#define PORT 12345
#define IP_ADDR "127.0.0.1"
#define DEFAULT_MODE      S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH
#define CHUNK_SIZE 1

int folder_exist(char *foldername);
void folder_management(char* foldername);
int file_counter(char* path);
void *connection_handler(void *);
void parseRecv(int sock,char* cmds,char* path);
void storeCMD(int sock,char* cmd,char* path);
void appendCMD(char* cmd);
void dirlistCMD (char* cmd);
void retrieveCMD(char* cmd);
char *trimwhitespace(char *str);
char *append(const char *orig, char c)
{
	size_t sz = strlen(orig);
	char *str = malloc(sz + 2);
	strcpy(str, orig);
	str[sz] = c;
	str[sz + 1] = '\0';
	return str;
}
struct arg_struct {
	int socket_desc;
	char* path;
};


int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		fprintf(stderr,"ERROR: Invalid arguments\n");
		fprintf(stderr,"USAGE: file-server <port-number> <storage-directory>\n");
		return 1;
	}	


	//Retrieve the ports for the sockets	
	int port = atoi(argv[1]);
	char* stordirectory = argv[2];

	/**check port window**/
	if(!(port>=8000&&port<=9000))
	{
		fprintf(stderr,"ERROR: Invalid arguments\n");
		fprintf(stderr,"USAGE: port must be between 8000 and 9000\n");
		return 1;
	}	

	/**begin intial print statement**/
	printf("Started File Storage Server\n");        
	//check for folder if not create it
	folder_management(stordirectory);	


	printf("Listening on port %d\n",port);
	/**end of init print statements**/

	int listenfd =0, connfd = 0;
	struct sockaddr_in serv_addr;	
	struct sockaddr_in client_addr;

	int clientlen = sizeof(client_addr);	

	time_t ticks;

	//creates an un-named socket inside the kernal and returns a socket descriptor(like a file descriptor)
	listenfd = socket(AF_INET,SOCK_STREAM,0);

	//this function takes an domain as it's argument
	memset(&serv_addr,'0', sizeof(serv_addr));


	//setting the iformation for the sock and the port
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	int yes=1; 

	// lose the pesky "Address already in use" error message 
	if (setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) { 
		perror("setsockopt"); 
		exit(1); 
	}

	//check if we can bind to the server address at the port specified
	if(bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)))
	{
		fprintf(stderr,"Error cannot bind to port %d\n",port);
		return 1;
	}	

	listen(listenfd,10);	

	pthread_t thread_id;	
	while(1)
	{
		//connfd = accept(listenfd,(struct sockaddr*)NULL,NULL);
		connfd = accept(listenfd, (struct sockaddr *) &client_addr, &clientlen);		
		struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&client_addr;
		int ipAddr = pV4Addr->sin_addr.s_addr;
		char ip_addr[INET_ADDRSTRLEN];
		inet_ntop( AF_INET, &ipAddr, ip_addr, INET_ADDRSTRLEN );

		printf("Received incoming connection from %s\n",ip_addr);		

		struct arg_struct args;
		args.socket_desc = connfd;
		args.path = stordirectory;

		if(pthread_create(&thread_id, NULL, connection_handler, (void *) &args) < 0)
		{
			printf("Error: Could not create thread\n");
		}	
	}
}
char* readtospace(int sock){
	char reader[3];
	char* cmd="";
	while(read(sock,reader,1)>0)
	{
		//printf("recv: %c\n",reader[0]);
		//printf("recvlen: %d\n",(int)strlen(reader));
		if(reader[0]==' '||reader[0]=='\n')
			break;
		cmd=append(cmd,reader[0]);
	}
	return cmd;
}
void *connection_handler(void *arguments){
	struct arg_struct *args = arguments;	
	int sock = args->socket_desc;
	char * path = (char*)args->path;	
	char *message ="This is a message to send \n\r";

	//read until space
	char* cmd=readtospace(sock);
	printf("cmd %s\n",cmd);
	char* bytesstring = readtospace(sock);
	printf("bytesfromstring %s\n",bytesstring);
	//printf("commmandlen: %d\n",(int)strlen(cmd));

	/* while(1)
	   {	
	//Gonna Test some ideas






	char* buffer;
	int n;
	char cmd[1024];



	//old code
	recv(sock,cmd,sizeof(cmd),0);
	printf("[thread %u] Rcvd: %s",(int)pthread_self(),cmd);
	parseRecv(sock,cmd,path);
	send(sock,message,strlen(message),0);	
	bzero(cmd,1024);
	}*/


	printf("[thread %u] connection closed\n",(int)pthread_self());
	close(sock);
}

void parseRecv(int socketfd, char* cmds, char* path)
{
	char* cmdtok = strtok(cmds, " ");
	printf("Cmds, %d\n",(int)strlen(cmdtok));
	if(!strcmp(cmdtok,"STORE"))
	{
		storeCMD(socketfd, cmdtok,path);
	}
	if(!strcmp(cmdtok,"APPEND"))
	{
		appendCMD(cmdtok);	
	}
	if(!strcmp(cmdtok,"DIRLIST"))
	{
		dirlistCMD(cmdtok);
	}
	if(!strcmp(cmdtok,"RETRIEVE"))
	{
		retrieveCMD(cmdtok);
	}	
}


void storeCMD(int socketfd, char* cmd,char* path)
{
	char * message = "";
	char* filename = strtok(NULL, " ");
	char* bytenumstring = strtok(NULL, " ");

	if(!filename)
		return;

	if(!bytenumstring)
		return;

	strcat(path,"/");
	strcat(path,filename);

	if(folder_exist(path))
	{
		strcat(message,"FILE EXISTS");
	}

	int filebytes = atoi(bytenumstring);
	char filebuffer[filebytes];


	//read in the bytes we filealready we have
	char* filecontent = strtok(NULL,""); 
	trimwhitespace(filecontent);

	//remove char
	if(strlen(filecontent)>	filebytes)
	{

	}
	if(strlen(filecontent)<filebytes)
	{

	}           

	FILE *fp = fopen(filename, "ab+");

	//read in the file	

	int size_recv , total_size= 0;
	char chunk[CHUNK_SIZE];

	while(total_size<filebytes)
	{
		memset(chunk ,0 , CHUNK_SIZE);  //clear the variable
		if((size_recv =  recv(socketfd , chunk , CHUNK_SIZE , 0) ) < 0)
		{
			break;
		}
		else
		{
			total_size += size_recv;
			printf("%s" , chunk);
			//fwrite(chunk, CHUNK_SIZE, 1, fp);
		}
	}
	printf("Later\n");
	return;
}
void appendCMD(char* cmd)
{

}
void dirlistCMD (char* cmd)
{

}	
void retrieveCMD(char* cmd)
{

}
/* Functions For Folder and File Management */
int file_counter(char* path)
{
	int file_count = 0;
	DIR * dirp;
	struct dirent * entry;

	dirp = opendir(path); /* There should be error handling after this */
	while ((entry = readdir(dirp)) != NULL) {
		if (entry->d_type == DT_REG) { /* If the entry is a regular file */
			file_count++;
		}
	}
	closedir(dirp);	
}

int folder_exist(char* foldername)
{
	struct stat buffer;
	return (stat (foldername, &buffer) == 0);
}


void folder_management(char* foldername){
	if(folder_exist(foldername)){
		printf("Storage directory \"%s\" already exists (contains %d files)\n",foldername,file_counter(foldername));
	}else{
		if(!mkdir(foldername,DEFAULT_MODE))
		{
			printf("Storage directory \"%s\" created successfully\n",foldername);
		}
	}
}
char *trimwhitespace(char *str)
{
	char *end;

	// Trim leading space
	while(isspace(*str)) str++;

	if(*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;

	// Write new null terminator
	*(end+1) = 0;

	return str;
}
