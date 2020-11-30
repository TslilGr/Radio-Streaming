/*
 ============================================================================
 Name        : server_control.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

void OpenWelcomeSocket(int TCP_port);
void *Connect(void * st);
int Hello(int client_socket);
int SendMessage(int client_socket, int command_type, int num_of_station, char* message);
void *PlaySong(void *st);
int UploadSong(int client_socket, int song_size, char* song_name);
int Close_and_Free();
int Print();
int GetMessage(int client_socket);

#define NUM_OF_CLIENTS 100
#define BUFFER_SIZE 1024
#define FIXED_SIZE 500
#define Mib 8388608

pthread_t * array_of_theards;	//thread for the multicast stations
pthread_t * array_of_clients[NUM_OF_CLIENTS];	//thread for the clients socket
struct sockaddr_in array_of_clints_ip[NUM_OF_CLIENTS];

struct sockaddr_in multicast;
uint16_t num_of_stations;
int welcome_socket;
int client_sockets[NUM_OF_CLIENTS];
int flag_new_station;// 0-advertised , 1-not
int array_flag_new_station[NUM_OF_CLIENTS];	//0-Didnt receive new station ; 1-received
int permit;	//1-free, 0-busy
int num_of_active_clients;
struct sockaddr_in info;
struct station *array_of_stations;

typedef struct station {
	int multigroup_addr;
	int UDP_port;
	int index;
	FILE* fp;
	char song_name[200];
	int song_name_size;
} station;

int main(int argc,char*input[]) {
	int select_flag=0,i=0,TCP_port,UDP_port,check_new_station=0,fd=0,read_flag=0;
	unsigned char recv_buff[BUFFER_SIZE]={0};

	/*initiate global variables*/
	num_of_active_clients=0;
	array_of_stations=malloc((argc-3)*sizeof(station));
	num_of_stations=argc-4;
	flag_new_station=0;
	permit=1;
	array_of_theards=(pthread_t*)malloc((num_of_stations)*sizeof(pthread_t));
	for(i=0;i<NUM_OF_CLIENTS;i++)
		array_flag_new_station[i]=0;

	/*initiate variables from manager*/
	if(argc<3)
		perror("not enough inputs");
	TCP_port=atoi(input[1]);
	multicast.sin_addr.s_addr=inet_addr(input[2]);
	UDP_port=atoi(input[3]);
	for(i=0;i<num_of_stations;i++)
		array_of_theards[i]=(pthread_t*)malloc(sizeof(array_of_theards));
	for(i=0;i<num_of_stations;i++){//Initiate song to stations
		strcpy(array_of_stations[i].song_name,input[i+4]);
		array_of_stations[i].song_name_size=sizeof(input[i+4]);
		array_of_stations[i].multigroup_addr=input[2];
		array_of_stations[i].UDP_port=UDP_port;
		array_of_stations[i].index=i;
		array_of_stations[i].fp=fopen(array_of_stations[i].song_name,"rb");
		if(array_of_stations[i].fp==NULL){
			perror("open file error");
			return EXIT_FAILURE;
		}
	}
	OpenWelcomeSocket(TCP_port);	//create welcome socket
	if(welcome_socket<0){
		perror("error opening welcome socket");
		EXIT_FAILURE;
	}

	//start select
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(welcome_socket,&readfds);
	FD_SET(fd,&readfds);
	
	for(i=0;i<num_of_stations;i++)
		pthread_create((pthread_t*)array_of_theards[i],NULL,PlaySong,i);
	puts("Please type q to quit or p to print the information\n");
	for (i=0;i<NUM_OF_CLIENTS;i++){
		select_flag=select(FD_SETSIZE,&readfds,NULL,NULL,NULL);
		if(select_flag<0)
			perror("select error\n");
		else if(select_flag==0)
			perror("select reading error\n");
		else
		{
			if(FD_ISSET(fd,&readfds))
			{
				read_flag=read(fd,recv_buff,sizeof(recv_buff));
				if(read_flag<0)
				{
					perror("read () error\n");
					return -1;
				}
				if(recv_buff[0]=='q')
				{
					Close_and_Free();
					return 0;
				}
				if(recv_buff[0]=='p')
				{
					 Print();
					 i--;
					 continue;
				}
			} 
				array_of_clients[num_of_active_clients]=(pthread_t*)malloc(sizeof(array_of_clients));
				pthread_create((pthread_t*)array_of_clients[num_of_active_clients],NULL,Connect,&num_of_active_clients);
				if(flag_new_station==1){
					for(i=0;i<num_of_active_clients;i++){
						check_new_station*=array_flag_new_station[i];
					}
					if(check_new_station==1){
						flag_new_station=0;
						for(i=0;i<num_of_active_clients;i++)
							array_flag_new_station[i]=0;
					}
				}

		}
		select_flag=0;
	}
	while(1){
		if(flag_new_station==1){
			for(i=0;i<num_of_active_clients;i++){
				check_new_station*=array_flag_new_station[i];
			}
			if(check_new_station==1){
				flag_new_station=0;
				for(i=0;i<num_of_active_clients;i++)
					array_flag_new_station[i]=0;
			}
		}
	}
}

void OpenWelcomeSocket(int TCP_port) {
	struct sockaddr_in server_addr={0};
	int optval = 1;

//create welcome socket
	welcome_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (welcome_socket < 0)
		perror("error opening server socket");
//configure server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(TCP_port);
	memset(server_addr.sin_zero,'\0',sizeof(server_addr.sin_zero));

//SOL_SOCKET,optval- was used as default values	; SO_REUSEADDR- define multipule binds  for one socket
	if (setsockopt(welcome_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &optval,
			sizeof(optval)) < 0)
		perror("setsockopt error");

	if (bind(welcome_socket, (struct sockaddr *) &server_addr,sizeof(server_addr)) < 0)
		perror("bind error");
	if (listen(welcome_socket, 5) == 0)
		printf("listening\n");
	else
		perror("listening error");
	return;
}

void * Connect(void * st) {
	int client_index = atoi(st), status = 0, check = 0;
	socklen_t addr_size;
	struct sockaddr_in client_addr;
	struct sockaddr_storage client_storage;
	addr_size = sizeof(client_storage);
	client_sockets[client_index] = accept(welcome_socket,(struct sockaddr *) &client_addr,&addr_size);
	if (client_sockets[client_index] < 0)
		perror("error in accept\n");
	array_of_clints_ip[num_of_active_clients].sin_addr.s_addr=client_addr.sin_addr.s_addr;
	check = Hello(client_sockets[client_index]);

	if (check == -3)//in case of 'q' from manager
		pthread_exit(0);
		
	if (check < 0)
		perror("error hello message\n");
		
	if (SendMessage(client_sockets[client_index], 0, 0, NULL) < 0)//send 0 to client - welcome
		perror("error in sending welcome message\n");
	num_of_active_clients++;
	client_index++;
	while(1) {
		if (flag_new_station == 1 && array_flag_new_station[client_index - 1] == 0) {
			if (SendMessage(client_sockets[client_index - 1], 4, 0, NULL)< 0) //4-new station
				perror("error sending new station message\n");
			else
				array_flag_new_station[client_index - 1] = 1;
		}
		status = GetMessage(client_sockets[client_index - 1]);
		if (status == -2) {	//closing one client socket	- we couldn't read from the client
			puts("the client has closed his connection\n");
			num_of_active_clients--;
			pthread_exit(0);
		}
		if (status == -3) {	//in case of 'q' from manager
			pthread_exit(0);
		}
		if (status < 0)
			perror("error getting message\n");
	}
}

int Hello(int client_socket) {
	int fd = 0, select_flag, read_flag;
	unsigned char buffer[3] = {0};
	struct timeval timeout;
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(client_socket, &readfds);
	FD_SET(fd, &readfds);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	select_flag = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
	if (select_flag == -1)
		perror("error in select function\n");
	else if (select_flag == 0) {
		if (SendMessage(client_socket, 3, 0,"timeout has passed") < 0)
			perror("error sending the invalid command\n");
		perror("select read error\n");
	} 
	else {
		if (FD_ISSET(fd, &readfds)) {
			read_flag = read(fd, buffer, sizeof(buffer));
			if (read_flag < 0) {
				perror("error reading\n");
				return -1;
			}
			if (buffer[0] =='q') {
				Close_and_Free();
				return -3;
			} else if (buffer[0] =='p') {
				Print();
				Hello(client_socket);
				return 0;
			}
		}
		read_flag = read(client_socket, buffer, sizeof(buffer));
		if (read_flag == -1) {
			perror("error read function\n");
			return -1;
		}
		if (buffer[0] != 0 || buffer[1] != 0 || buffer[2] != 0) {//there is a problem in the hello message recieved
			if (SendMessage(client_socket, 3, 0, "wrong Hello message- we needed to get 3 bytes of 0") < 0)
				perror("error sending the invalid command\n");
			perror("wrong hello message\n");
			return -1;
		}
	}
	return 0;
}

int SendMessage(int client_socket, int command_type, int num_of_this_station, char* message) {
	unsigned char send_buff[FIXED_SIZE] = {0};
	station curr_station = array_of_stations[num_of_this_station];
	int multicast_addr_this_station =htonl(inet_addr(curr_station.multigroup_addr));
	int i = 0, size_message = 0;

	switch (command_type) {
		case 0: {	//command typed is 0 send welcome message
			send_buff[0] = 0;	//reply type 0-welcome
			*((uint16_t*) (send_buff + 1)) = htons(num_of_stations);
			*((uint32_t*) (send_buff + 3)) = multicast_addr_this_station;
			*((uint16_t*) (send_buff + 7)) = htons(curr_station.UDP_port);
			if (send(client_socket, send_buff, 9, 0)<0) {//9-buffer size
				perror("error in sending welcome message\n");
				return -1;
			}
			puts("welcome was sent\n");

			return 0;
		}
		case 1: {	//command typed is 1 send announce message
			send_buff[0] = 1;	//reply type 1
			size_message = curr_station.song_name_size;
			*((uint8_t*) (send_buff + 1)) = size_message;
			for (i = 2; i < size_message+2; i++)
				*((char*) (send_buff + i)) = curr_station.song_name[i - 2];
			if (send(client_socket, send_buff, size_message + 2, 0) < 0) {
				perror("error in sending announce message\n");
				return -1;
			}
			puts("announce was sent\n");

			return 0;
		}
		case 2: {	//command typed is 2 send permitSong message
			send_buff[0] = 2;	//replay type 2
			send_buff[1] = permit;
			if (send(client_socket, send_buff, 2, 0) < 0) {
				perror("error in sending permit message\n");
				return -1;
			}
			puts("permit was sent\n");
			return 0;
		}
		case 3: {	//command typed is 3 send invalid command message
			send_buff[0] = 3;	//replay type 3
			size_message = sizeof(message);
			*((uint8_t*) (send_buff + 1)) = size_message;
			for (i = 2; i < size_message; i++)
				*((char*) (send_buff + i)) = message[i - 2];
			if (send(client_socket, send_buff, size_message + 2, 0) < 0) {
				perror("error in sending invalid command message\n");
				return -1;
			}
			printf("%s\n", message);
			puts("invalid command was sent\n");
			return 0;
		}
		case 4: {	//command typed is 4 send newStations message
			puts("we send new station message\n");
			send_buff[0] = 4;
			*((uint16_t*) (send_buff + 1)) = htons(num_of_stations);
			if (send(client_socket, send_buff, 3, 0)<0) {
				perror("error in sending new station message\n");
				return -1;
			}
			return 0;
		}

	}
	return 0;
}

void *PlaySong(void *st) {	//sends the song to the multicast station
	struct sockaddr_in server_address;
	int UDP_server_socket, station_index = (int)st,TTL = 10;
	char buff[BUFFER_SIZE] = {0};
	station curr_station = array_of_stations[station_index];
	char* curr_multi_address = curr_station.multigroup_addr;

	if (curr_station.fp == NULL) {
		perror("file error\n");
		pthread_exit(0);
	}
	UDP_server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (setsockopt(UDP_server_socket, IPPROTO_IP, IP_MULTICAST_TTL,(char*) &TTL, sizeof(TTL)) < 0)
		perror("set socket opt error\n");
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(curr_multi_address)+htonl(curr_station.index);
	server_address.sin_port=htons(curr_station.UDP_port);
	while(1) {
			if (fread(buff, 1, BUFFER_SIZE, curr_station.fp) < 0)
				perror("read file error\n");
			if (feof(curr_station.fp))	//the end of file
				rewind(curr_station.fp);	//open the file again
			if (ferror(curr_station.fp) != 0)
				fputs("error reading the file", stderr);
			sendto(UDP_server_socket, buff, sizeof(buff), 0,(struct sockaddr*) &server_address, sizeof(server_address));//send to multicast group
			usleep(62500);
	}
}

int UploadSong(int client_socket, int song_size, char* song_name) {
	int read_flag = 0, write_flag = 0, total_byte_sent = 0, select_flag = 0;
	FILE *fp;
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(client_socket, &readfds);
	unsigned char recv_buff[FIXED_SIZE] = { 0 };
	permit = 0;	//change the flag for allowing upload song
	puts("starting upload\n");
	select_flag = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);//open select connection for receiving messages
	if (select_flag == -1) {
		perror("select error\n");
		return EXIT_FAILURE;
	} else {
		fp = fopen(song_name, "wb");
		if (fp == NULL) {
			perror("File error\n");
			return EXIT_FAILURE;
		}
		while (total_byte_sent < song_size) {	//receiving the song
			read_flag = read(client_socket, recv_buff, sizeof(recv_buff));
			if (read_flag == 0) {	//we spoted getting the song
				if (SendMessage(client_socket, 3, 0,"we stoped getting the song") < 0)
					perror("the invalid message sent is wrong");
			}
			if (read_flag == -1) {
				perror("error in read function");
				return -1;
			}
			total_byte_sent += read_flag;
			write_flag = fwrite(recv_buff, 1, sizeof(recv_buff), fp);
			if (write_flag == -1) {
				perror("error in write function");
				return -1;
			}
		}
	}
	fclose(fp);
	return 0;
}

int Close_and_Free() {	//free and close at the end of the program
	puts("good bye");
	int i = 0;
	for (i = 0; NUM_OF_CLIENTS; i++)
		close(client_sockets[i]);
	close(welcome_socket);
	for (i = 0; i < num_of_stations; i++) {
		pthread_exit(array_of_theards[i]);
		free(array_of_theards[i]);
	}
	for (i = 0; i < num_of_active_clients; i++)
		free(array_of_clients[i]);
	free(array_of_theards);
	free(array_of_stations);

	return 0;
}

int Print() {
	int i=0;
	struct sockaddr_in address;
	printf("we have %d active stations\n",num_of_stations);
	address=multicast;
	for(i=0;i<num_of_stations;i++){
		address.sin_addr.s_addr=multicast.sin_addr.s_addr+htonl(i);
		printf("station no.%d: %s \n",i,inet_ntoa(address.sin_addr));
		printf("The song: %s is playing\n",array_of_stations[i].song_name);
	}
	printf("We have %d clients\n",num_of_active_clients);
	for(i=0;i<num_of_active_clients;i++){
		printf("client %d with ip address: %s \n",i+1,inet_ntoa(array_of_clints_ip[i].sin_addr));//print each client with his ip address
	}
	return 1;
}

int GetMessage(int client_socket){//get message from the client or radio manager
	int select_flag=-1,song_name_size=0,exist=0,exist_count=0,fd=0,read_flag=0,i=0,j=0;
	uint16_t num_of_this_station=0,temp=0;
	uint32_t song_size=0;
	unsigned char recv_buff[FIXED_SIZE]={0};
	char song_name[200]={0};
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(client_socket,&readfds);
	FD_SET(fd,&readfds);
	puts("Hi, please type q to quit or p to print the information\n");
	select_flag=select(FD_SETSIZE,&readfds,NULL,NULL,NULL);
	if(select_flag==-1)
		perror("Error in select function\n");
	else if(select_flag==0)
		perror("select reading error\n");
	else{
		if(FD_ISSET(fd,&readfds)){
			read_flag=read(fd,recv_buff,sizeof(recv_buff));
			if(read_flag<0){
				perror("error read function\n");
				return -1;
			}
			if(recv_buff[0]=='q'){
				Close_and_Free();
				return -3;
			}
			else if(recv_buff[0]=='p'){
				Print();
				GetMessage(client_socket);//Infinity loop
				return 0;
			}
		}
		read_flag=read(client_socket,recv_buff,sizeof(recv_buff));
		if(read_flag<0){
			perror("error in read function\n");
			return -1;
		}
		if(read_flag==0)
			return -2;
		switch(recv_buff[0]){
			case 0:{
				if(SendMessage(client_socket,3,0,"We already established connection")<0){
					perror("invalid message has sent\n");
					break;
				}
				break;
			}
			case 1:{	//command type=1 ,ask song
				temp=0x00FF & recv_buff[2];
				num_of_this_station=((uint16_t)recv_buff[1]<<8)|temp;
				if(num_of_this_station>0 || num_of_this_station<num_of_stations){//correct station
					if(SendMessage(client_socket,1,num_of_this_station,NULL)<0){
						perror("error in sending the announce message\n");
						break;
					}
				}
				else{
					if(SendMessage(client_socket,3,0,"wrong station number - in ask song")<0){
						perror("error in sending error in announce message\n");
						break;
					}
					perror("error in ask song message\n");
					return -1;
				}
				break;
			}
			case 2:{	//command type=2 ,up song massage
				song_size=(recv_buff[1]<<24)|(recv_buff[2]<<16)|(recv_buff[3]<<8)|(recv_buff[4]);
				if(song_size>10*Mib){
					if(SendMessage(client_socket,3,0,"the song size is above limit")<0){
						perror("error sending in invalid command\n");
						break;
					}
					perror("the song size is above limit\n");
				}
				song_name_size=recv_buff[5];
				if(song_name_size>200){
					if(SendMessage(client_socket,3,0,"the song name is too long")<0){
						perror("error sending in invalid command\n");
						break;
					}
					perror("the song name is too long\n");
				}
				for(i=0;i<song_name_size;i++)
					song_name[i]=recv_buff[i+6];//6-stars after the song name size
				for(i=0;i<num_of_stations;i++){	//check if the song exist
					if(array_of_stations[i].song_name_size==song_name_size){
						for(j=0;j<song_name_size;j++){
							if(array_of_stations[i].song_name[j]==song_name[j])
								exist_count++;
						}
						if(exist_count==song_name_size){
							exist=1;
							break;
						}
					}
				}
				if(exist==1)
					permit=0;//this song already exist
				if(permit==1){//upload approved
					if(SendMessage(client_socket,2,0,NULL)<0){
						perror("error sending permit message\n");
						break;
					}
					if(UploadSong(client_socket,song_size,song_name)){
						perror("error receiving the song\n");
						break;
					}
					array_of_stations=(station*)realloc(array_of_stations,(num_of_stations+1)*sizeof(station));//Allocate space for the songs
					array_of_stations[num_of_stations].song_name_size=song_name_size;
					array_of_stations[num_of_stations].multigroup_addr=array_of_stations[num_of_stations-1].multigroup_addr;
					array_of_stations[num_of_stations].UDP_port=array_of_stations[num_of_stations-1].UDP_port;
					array_of_stations[num_of_stations].index=num_of_stations;
					strcpy(array_of_stations[num_of_stations].song_name,song_name);
					array_of_stations[num_of_stations].fp=fopen(song_name,"rb");
					if(array_of_stations[num_of_stations].fp==NULL){
						perror("file error\n");
						return EXIT_FAILURE;
					}
					array_of_theards=(pthread_t*)realloc(array_of_theards,(num_of_stations+1)*sizeof(pthread_t));
					array_of_theards[num_of_stations]=(pthread_t*)realloc(array_of_theards[num_of_stations],sizeof(array_of_theards));
					pthread_create((pthread_t*)array_of_theards[num_of_stations],NULL,PlaySong,num_of_stations);
					permit=1;//we finished sending song-free
					flag_new_station=1;
					num_of_stations++;
					puts("upload completed\n");
				}
				else{//permit==0 ,uploading not approved
					if(SendMessage(client_socket,2,0,NULL)<0){
						perror("error sending permit\n");
						break;
					}
				}
				break;
			}
		}
	}
	return 0;
}



