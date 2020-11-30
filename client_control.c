/*
 ============================================================================
 Name        : Client.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

struct Welcome{
	uint8_t replytype;
	uint16_t numStations;
	uint32_t multiGroup;
	uint16_t portnum;
}Welcome;


int OpenTcpSocket(char * server_ip,int server_port);
void Hello(short UDPportnum,struct timeval timeout,struct Welcome *welcome);
void *ListenToStation();
void AskSong(short station_number);
int RequestUpSong(int song_size, int song_name_size,char* song_name,struct timeval timeout);
void UpSong(struct timeval timeout);
void CleanBuffer(char * buffer);
int IsDigit(char * buffer);
void IllegalCommand();
void PrintIP();

int TCPsocket;
struct Welcome welcome;
struct in_addr ip_multicast;
uint8_t Connection_ON;
uint8_t change_station=0;
unsigned int current_station=0;

int main(int argc,char * argv[]) {

	int client_fd=0;
	struct timeval timeout;
	char buffer[BUFFER_SIZE]={0};
	char* server_ip=argv[1];
	int server_port=atoi(argv[2]);
	short UDP_port_num=0;
	int replytype,station_num,digit,select_flag,read_flag;
	pthread_t UDP_play_thread;
	pthread_attr_t attr;
	fd_set read_set;
	FD_ZERO(&read_set);

	/*initiate welcome struct*/
	welcome.replytype=5;
	welcome.numStations=0;
	welcome.portnum=0;

	/*initiate timeout*/
	timeout.tv_sec=0;
	timeout.tv_usec=300000;


	/*starting TCP connection*/
	TCPsocket=OpenTcpSocket(server_ip,server_port);
	if(TCPsocket==-1)
		return EXIT_FAILURE;

	/*sending hello message , waiting for welcome*/
	Hello(UDP_port_num,timeout,&welcome);
	/*start receiving UDP packets from the thread and play the song*/
	pthread_attr_init(&attr);
	pthread_create(&UDP_play_thread,&attr,ListenToStation,NULL);

	FD_SET(TCPsocket,&read_set);
	FD_SET(client_fd,&read_set);

	while(1){
		printf("Please enter a station number to change the station, or 's' to send a song, or 'q' to quit.\n");
		/*waiting for input from the user or command from the server*/
		select_flag=select(FD_SETSIZE,&read_set,NULL,NULL,NULL);
		if(select_flag==-1)
			perror("error in select function");
	/*	else if(select_flag==0)
			printf("timeout passed without getting 'new station'\n"); 	no timeout defined*/
		else{
			CleanBuffer(buffer);
			if(FD_ISSET(client_fd,&read_set)){/*if the input is from a client*/
				client_fd=STDIN_FILENO;//stream receive data from the client
				read_flag=read(client_fd,buffer,sizeof(buffer));
				if(read_flag<0)
					perror("error in read function");
				else{//check for the type of command from the client
					digit=IsDigit(buffer);
					if(digit){//for station number
						station_num=atoi(buffer);
						if(station_num<(int)welcome.numStations){
							printf("You chose to listen to station number: %d",station_num);
							AskSong(station_num);
							current_station=station_num;
							change_station=1;
						}else
							printf("Please retry by entering a valid station number or 'q' to quit");
					}else{//the command is not digit
						if(buffer[0]=='s' && buffer[1]==10){//10 for line end
							printf("You chose to upload song to the server");
							UpSong(timeout);
						}else if(buffer[0]=='q' && buffer[1]==10){//10 for line end
							printf("You chose to quit the radio , closing the connection");
							Connection_ON=0;
							close(TCPsocket);
							break;
						}else
							printf("Invalid command ,try again");
					}
				}
			}//INPUT
		}if(FD_ISSET(TCPsocket,&read_set)){/*if the input is from the TCP socket*/
			printf("Reading from the socket\n");
			read_flag=read(TCPsocket,buffer,sizeof(buffer));
			if(read_flag<0)
				perror("error in read function");
			else if(read_flag==0){
				printf("the connection was closed by the server");
				Connection_ON=0;
				close(TCPsocket);
				break;
			}else{
				replytype=buffer[0];
				if(replytype==4){//New station message
					station_num=buffer[1];
					station_num=station_num<<8;
					station_num+=buffer[2];
					welcome.numStations=station_num;
					printf("number of the new station is: %d",welcome.numStations);
				}else if(replytype==3){
					IllegalCommand();
					break;
				}
			}
		}
	}
}

int OpenTcpSocket(char * server_ip,int server_port){
	//create TCP socket
	struct sockaddr_in server_addr;
	int clinet_socket;

	clinet_socket=socket(AF_INET,SOCK_STREAM,0);
	if(clinet_socket<0)
		perror("EROOR creating socket");
	//configure server address
	server_addr.sin_family= AF_INET;
	server_addr.sin_addr.s_addr=inet_addr(server_ip);
	server_addr.sin_port=htons(server_port);
	memset(server_addr.sin_zero,'\0',sizeof(server_addr.sin_zero));

	//connect to server
	if(connect(clinet_socket,(struct sockaddr*) &server_addr,sizeof(server_addr))<0){
		perror("connecting error");
		return -1;
	}
	return clinet_socket;
}

void Hello(short UDPportnum,struct timeval timeout,struct Welcome *welcome){
	unsigned char bufeer_send[3],buffer_recv[9];
	int return_value,read_succ,replytype;
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(TCPsocket, &readfds);


	bufeer_send[0]=0;
	bufeer_send[1]=0;
	bufeer_send[2]=0;
	send(TCPsocket,bufeer_send,3,0);

	while(1){
		return_value=select(FD_SETSIZE,&readfds,NULL,NULL,&timeout);
		if(return_value==-1){
			perror("error in select function");
			break;
		}
		else if(return_value==0){
			perror("error in reading from select; no data received within 0.3 sec.");
			break;
		}
		else{
			read_succ=read(TCPsocket,buffer_recv,sizeof(buffer_recv));
			if(read_succ==-1){
				perror("error in read function");
				break;
			}
			else if(read_succ==0){
				perror("error in read function-no data");
				break;
			}
			else{	/*the information was received successfully*/
				replytype=buffer_recv[0];
				if(replytype==0){
					welcome->replytype =replytype;
					welcome->numStations=(buffer_recv[1]<<8)+buffer_recv[2];
					welcome->multiGroup=(buffer_recv[3]<<24)+(buffer_recv[4]<<16)+(buffer_recv[5]<<8)+buffer_recv[6];
					welcome->portnum=(buffer_recv[7]<<8)+buffer_recv[8];
					ip_multicast.s_addr=welcome->multiGroup;

					//printing the information received from the welcome packet
					printf("Welcome to T&H radio station!!\n");
					printf("The number of stations is: %d \n",welcome->numStations);
					PrintIP();
					printf("\nPort number is: %d\n",welcome->portnum);
					Connection_ON=1;
					break;
				}else if(replytype==3){
					IllegalCommand();
				}
			}
		}

	}

}

void *ListenToStation(){
	int UDP_socket,return_value,write_return_value;
	socklen_t addr_len;
	struct sockaddr_in server_addr;
	struct ip_mreq mreq;
	FILE *fp;
	char *buff[BUFFER_SIZE]={0};

	//create UDP socket
	UDP_socket=socket(AF_INET,SOCK_DGRAM,0);
	if(UDP_socket<0)
		perror("EROOR creating socket");
	//configure server address
	server_addr.sin_family= AF_INET;
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	server_addr.sin_port=htons(welcome.portnum);
	memset(server_addr.sin_zero,'\0',sizeof(server_addr.sin_zero));//memset padding zeros as definition

	// Bind the address struct to the socket
	bind(UDP_socket,(struct sockaddr*)&server_addr,sizeof(server_addr));

	mreq.imr_multiaddr.s_addr=ip_multicast.s_addr;
	mreq.imr_interface.s_addr=htonl(INADDR_ANY);

	setsockopt(UDP_socket, IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));
	addr_len = sizeof server_addr;

	fp = popen("play -t mp3 -> /dev/null 2>&1", "w");	//open the mp3 file and play
	if (fp == NULL)
		printf("error opening file with popen!!!!!!\n");
	while(Connection_ON){
		if(change_station==1){//leave current multicast group
			setsockopt(UDP_socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));

			/*Connect to the new multicast group*/
			mreq.imr_multiaddr.s_addr= ip_multicast.s_addr;
			mreq.imr_multiaddr.s_addr+= htonl(current_station);
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);
			setsockopt(UDP_socket, IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));

			change_station=0;
		}
		return_value = recvfrom(UDP_socket,buff,BUFFER_SIZE,0,(struct sockaddr*)&server_addr,&addr_len);
		if(return_value>0){
			write_return_value=fwrite (buff , sizeof(char), BUFFER_SIZE, fp);
			if(write_return_value<0)
				perror("ERROR while playing\n");
		}
		else if(return_value<0)
			perror("Error receiving data with UDP");
		else{
			puts("Connection was closed by the server.\n");
		}
	}
	setsockopt(UDP_socket, IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));
	puts("Close UDP connection.\n");
	pthread_exit(0);


}

void AskSong(short station_number){

	char buffer[BUFFER_SIZE];
	int sent_flag,recv_flag,i,replytype;
	uint8_t song_name_size;
	fd_set readfds;
	struct timeval timeout;
	FD_ZERO(&readfds);
	FD_SET(TCPsocket,&readfds);

	//set timer for 0.3 milliseconds
	timeout.tv_sec=0;
	timeout.tv_usec=300000;

	buffer[0]=1;
	buffer[2]=station_number;
	buffer[1]=station_number>>8;
	sent_flag=send(TCPsocket,buffer,3,0);
	if(sent_flag<0)
		perror("error sending request");
	sent_flag=select(FD_SETSIZE,&readfds,NULL,NULL,&timeout);
	if(sent_flag==-1)
		perror("error in select function");
	else if(sent_flag==0)
		printf("time out");
	else{
		recv_flag = read(TCPsocket,buffer,sizeof(buffer));
		if(recv_flag==-1)
			perror("error reading request");
		else if(recv_flag==0)
			perror("read has no data");
		else{
			replytype=buffer[0];
			if(replytype==1){//reply type ==1 - announce
				song_name_size=buffer[1];
				printf("announce\n The name of the station is:");
				for(i=0;i<song_name_size;i++)
					printf("%c",buffer[i+2]);
				printf("\n");
			}
			else if(replytype==3){//reply type!=1
				printf("announce function in server had an error");
				IllegalCommand();
			}
		}
	}
}

int RequestUpSong(int song_size, int song_name_size,char* song_name,struct timeval timeout){
	unsigned char buffer[6+song_name_size];
	unsigned int i,select_flag,read_flag,replytype;
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(TCPsocket,&readfds);

	buffer[0]=2;//type request
	buffer[1]=(song_size>>24)& 0xFF;//song size
	buffer[2]=(song_size>>16)& 0xFF;
	buffer[3]=(song_size>>8)& 0xFF;
	buffer[4]=(song_size)& 0xFF;
	buffer[5]=song_name_size;
	for(i=0;i<song_name_size;i++)
		buffer[i+6]=song_name[i];

	write(TCPsocket,buffer,6+song_name_size);//sent to thread
	select_flag=select(FD_SETSIZE,&readfds,NULL,NULL,&timeout);
	if(select_flag == -1)
		perror("error in select function\n");
	else if(select_flag == 0)
		printf("timeout passed without getting 'premit'\n");
	else{
		read_flag=read(TCPsocket,buffer,sizeof(buffer));
		if(read_flag== -1)
			perror("error in read() function\n");
		else if(read_flag==0)
			printf("read has no data\n");
		else{
			replytype=buffer[0];
			if(replytype==2){//replytype=2-premit song
				if(buffer[1]==0){
					printf("premit denied, Try again later\n");
					return 0;
				}else if(buffer[1]==1){
					printf("You received permission to upload the song\n");
					return 1;
				}else{
					printf("error in the  premit message, Try again\n");
					return 0;
				}
			}else if(replytype==3){
				IllegalCommand();
				return 0;
			}else
			{
				printf("error receiving premit, Try again\n");
				return 0;
			}
		}
	}
	return 0;
}

void UpSong(struct timeval timeout){

	char song_name[200],buffer[BUFFER_SIZE],new_station;
	FILE *fp;
	int read_flag,send_flag,select_flag,replytype;
	struct stat st;
	struct timeval timeout2;
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(TCPsocket,&readfds);

	//set timer for 2 second
	timeout2.tv_sec=2;
	timeout2.tv_usec=0;

	printf("Please enter a valid song name to transmit, in the form songName.mp3 .");
	printf(" If you want to quit enter anything else.\nPlease enter less than 200 characters and then press enter.\n");
	scanf("%s",song_name);
	fp=fopen(song_name,"r");
	if(fp==NULL){
		printf("The song file you chose does not exist or can not be opened.\n");
		return;
	}
	stat(song_name,&st);//store information about the file
	if(RequestUpSong(st.st_size,strlen(song_name),song_name,timeout)==0){
		return;
	}
	printf("Trying to transmit the song...");

	while(!(feof(fp))){//while we didnt reached the end of the file
		read_flag=fread(buffer,1,sizeof(buffer),fp);
		if(ferror(fp)!=0)
			fputs("Error read the file",stderr);
		if(read_flag<=0)
			perror("error return value <= 0");
		else{//send the file
			send_flag=send(TCPsocket,buffer,read_flag,0);
			usleep(8000);
			if(send_flag<0)
				perror("error sending the song");
			if(send_flag==0)
				printf("send function returned zero");
		}
	}
	printf("Transmission completed \nWaiting for newStation message\n ");

	select_flag=select(FD_SETSIZE,&readfds,NULL,NULL,&timeout2);
	if(select_flag==-1)
		perror("error in select function");
	else if(select_flag==0)
		printf("timeout passed without getting 'new station'\n");
	else{
		CleanBuffer(buffer);
		read_flag=read(TCPsocket,buffer,sizeof(buffer));
		if(read_flag== -1)
			perror("error in read() function\n");
		else if(read_flag==0)
			printf("read has no data\n");
		else{
			replytype=buffer[0];
			if(replytype==3)
				IllegalCommand();
			else if (replytype==4)
			{
				new_station=buffer[1];
				new_station=new_station<<8;
				new_station+=buffer[2];
				welcome.numStations=new_station;
				printf("number of the new station is: %d",welcome.numStations);
				change_station=1;
				current_station=new_station-1;
			}
			else
				printf("error receiving New Station, Try again\n");
		}
	}

}

void CleanBuffer(char * buffer){
	int i;
	for(i=0;i<sizeof(buffer);i++)
		buffer[i]=0;
}

int IsDigit(char * buffer){//check that all buffer data is digits
	int i, len=strlen(buffer);
	for(i=0;i<len-1;i++)
		if(!(isdigit(buffer[i])))
			return 0;
	return 1;
}

void IllegalCommand(){
	printf("you have typed illegal command, your'e connection has closed ");
	Connection_ON=0;
	close(TCPsocket);
}

void PrintIP(){
	struct in_addr ip;
	ip.s_addr=welcome.multiGroup;
	printf("Multicast group address is: %s/n",inet_ntoa(ip));
}

