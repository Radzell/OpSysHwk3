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
#define ERROR "ERROR"
#define ACK "ACK"
int folder_exist(char *foldername);
void folder_management(char* foldername);
int file_counter(char* path);
void *connection_handler(void *);
void parseRecv(int sock,char* cmds,char* path);
void storeCMD(int sock,char* cmd,char* path);
void appendCMD(int sock,char* cmd,char* path);
void dirlistCMD (int sock,char*cmd,char* path);
void retrieveCMD( int sock,char* cmd, char* path);
char *trimwhitespace(char *str);
char* listFiles(char sdir[], int *count,char prefix[]);
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

	//int clientlen = sizeof(client_addr);	
	socklen_t clientlen;
	

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
	

	//read until space
	while(1){
		
		char* cmd=readtospace(sock);
		parseRecv(sock,cmd,path);
		
	}
	

	printf("[thread %u] connection closed\n",(int)pthread_self());
	close(sock);
}

void parseRecv(int socketfd, char* cmds, char* path)
{	
	//printf("cmd: %s\n",cmds);
	//printf("test %d\n",strcmp(cmds,""));
		
	if(!strcmp(cmds,"STORE"))
	{
		storeCMD(socketfd, cmds,path);
	}
		
	if(!strcmp(cmds,"APPEND"))
	{
		appendCMD(socketfd,cmds,path);	
	}
		
	if(!strcmp(cmds,"DIRLIST"))
	{	
		dirlistCMD(socketfd,cmds,path);
	}
	if(!strcmp(cmds,"RETRIEVE"))
	{
		retrieveCMD(socketfd,cmds,path);
	}
	return;	
}


void storeCMD(int sock, char* cmd,char* path)
{
	char* filename = readtospace(sock);
	char* bytesstring = readtospace(sock);
	int bytesnum = atoi(bytesstring);
	char fullpath[200];
	
	printf("[thread %u] Rcvd: STORE %s %s\n",(int)pthread_self(),filename,bytesstring);
	strcpy(fullpath,"./");
	strcat(fullpath,path);
	strcat(fullpath,"/");
	strcat(fullpath,filename);
		
	char buffer[bytesnum];
	int byteRead;
	
	if((byteRead = recv(sock, buffer, bytesnum, 0)) <= 0)
	{
		printf("[thread %u] Sent: ERROR\n",(int)pthread_self());
		send(sock,ERROR,strlen(ERROR),0);	
		return; 
	}

	FILE* pFile = fopen(fullpath,"wb");
	if(!pFile){
		printf("[thread %u] Sent: ERROR\n",(int)pthread_self());
		send(sock,ERROR,strlen(ERROR),0);
		return;
	}
	fwrite(buffer,1,sizeof(buffer),pFile);
	fclose(pFile);
	printf("[thread %u] Sent: ACK\n",(int)pthread_self());
	send(sock,ACK,strlen(ACK),0);
}
void appendCMD(int sock, char* cmd,char* path)
{
	char* filename = readtospace(sock);
	char* bytesstring = readtospace(sock);
	int bytesnum = atoi(bytesstring);
	char fullpath[200];
	strcpy(fullpath,"./");
	strcat(fullpath,path);
	strcat(fullpath,"/");
	strcat(fullpath,filename);
	
		
	char buffer[bytesnum];
	int byteRead;

	if((byteRead = recv(sock, buffer, bytesnum, 0)) <= 0)
	{
		printf("[thread %u] Sent: ERROR\n",(int)pthread_self());
		send(sock,ERROR,strlen(ERROR),0);   
		return; 
	}

	FILE* pFile = fopen(fullpath,"a+");
	if(!pFile){
		printf("[thread %u] Sent: ERROR\n",(int)pthread_self());
		send(sock,ERROR,strlen(ERROR),0);   
		return;
	}
	fwrite(buffer,1,sizeof(buffer),pFile);
	fclose(pFile);
	printf("[thread %u] Sent: ACK\n",(int)pthread_self());
	send(sock,ACK,strlen(ACK),0);
	
}
void dirlistCMD (int sock, char* cmd,char * path)
{	
	//check for prefix
	char reader[3];
	char filecount[15];

	char* prefix="";	
	if(read(sock,reader,1)>0)
	{
		
		if(reader[0]!='\n'){
			prefix = append(prefix,reader[0]);
			strcat(prefix, readtospace(sock));
		}
	}
	char* filelist;
	int count=0;
		
	filelist=listFiles(path,&count,prefix);
	char message[strlen(filelist)+strlen(ACK)+15];
	strcpy(message,ACK);
	sprintf(filecount,"%d",count);
	strcat(message," ");
	strcat(message,filecount);
	strcat(message," ");
	strcat(message,filelist);
	strcat(message,"\n");

	printf("[thread %u] Sent: %s\n",message,(int)pthread_self());
	send(sock,message,strlen(message),0);
}	
void retrieveCMD(int sock,char* cmd,char * path)
{
	char* filename = readtospace(sock);
	char fullfile[200];
	strcpy(fullfile,path);
	strcpy(fullfile,"/");
	strcpy(fullfile,filename);
	printf("filename: %s\n",fullfile);
	if(!folder_exist(fullfile)){
		//send do not exist
	}

	FILE * pFile;
	long lSize;
  	char * buffer = NULL;
  	size_t result;

	pFile = fopen ( filename , "rb" );
	char* message = "NO SUCH FILE";
  	if (pFile==NULL) {send(sock,message,strlen(message),0), fputs ("File error",stderr); exit (1);}

 	 // obtain file size:
  	fseek (pFile , 0 , SEEK_END);
  	lSize = ftell (pFile);
  	rewind (pFile);

  	// allocate memory to contain the whole file:
  	buffer = (char*) malloc (sizeof(char)*lSize);
	bzero(buffer, strlen(buffer));
  	if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}

  	// copy the file into the buffer:
  	result = fread (buffer,1,lSize,pFile);
  	if (result != lSize) {fputs ("Reading error",stderr); exit (3);}
  	/*  the whole file is now loaded in the memory buffer. */
  	// terminate
  	fclose (pFile);
	//preing
	char* msg = NULL;
  	msg = (char*) malloc ((strlen(buffer)+5)*sizeof(char) + sizeof(lSize));
	bzero(msg, strlen(msg));
	msg[strlen(msg)-1] = '\0';
	sprintf(msg, "ACK %lu %s\n", lSize , buffer);
	int n = send( sock, msg, strlen( msg ), 0 );
	if ( n < strlen( msg ) ) {
		perror( "send()" );
	}
  	free (buffer);
  	free (msg);
	return;
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
	return file_count;	
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
	while(*str==' ') str++;

	if(*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && *end==' ') end--;

	// Write new null terminator
	*(end+1) = 0;

	return str;
}
char* listFiles(char sdir[], int *count,char prefix[]){	

	    
    DIR * dir = opendir(sdir);
    if ( dir == NULL ) {
        perror( "opendir() failed" );
        return NULL;
    }
	
    struct dirent * file;
    (*count) =0;
    char filelist[900]; 
    
    
    while ( ( file = readdir( dir ) ) != NULL ) {
        /*  don't include . and .. in the listing */
        if(strcmp(file->d_name,"..")==0 || strcmp(file->d_name, ".")==0) {
            continue;
        }
	 if(strncmp(file->d_name, prefix, (int)strlen(prefix)) != 0 ) {
            continue;
        }

	
	strcat(filelist,file->d_name);
	strcat(filelist," ");
       	(*count)++;
   }
  return trimwhitespace(filelist);	   	 
}

