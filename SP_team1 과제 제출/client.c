#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

void *send_msg(void *); // send message
void *recv_msg(void *); // receive message

char name[BUFSIZ]="[DEFAULT]"; // client name
char msg[BUFSIZ]; // messages to send

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr; // server address structure
    int sock; // client socket				
	pthread_t snd_thread, rcv_thread; // send, receive message thread
	void *thread_return; // thread_join시 return값 저장할 변수	
    
    if(argc != 4) // 사용법 알려줌
    {
        fprintf(stderr, "%s <name> <port_number> <IPaddress>", argv[0]);
        exit(1);
    }

    strcpy(name, argv[1]); // get client name
    
    //create client socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock == -1)
    {
        perror("socket");
        exit(1);
    }

    //set values in server sockaddr structure
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_port = htons(atoi(argv[2]));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(argv[3]);

    //request connect to server socket
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("connect");
        exit(1);
    }

    // send client name to server
    write(sock, name, BUFSIZ);

    //start thread
    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);
    close(sock);
    return 0;
}

void *send_msg(void *arg)  // send message to server 
{
	int sock=*((int*)arg);
	char name_msg[BUFSIZ];

	while(1) 
	{
		fgets(msg, BUFSIZ, stdin);	// get message from terminal
		if(!strcmp(msg,"q\n") || !strcmp(msg,"Q\n")) // if 'q' or 'Q'
		{
			close(sock); //terminate and close socket
			exit(0);
		}
		
		sprintf(name_msg,"[%s] %s", name, msg);	
		write(sock, name_msg, BUFSIZ); // send message to server
	}
	return NULL;
}

void *recv_msg(void *arg)  // receive message from server
{
	int sock=*((int*)arg);		
	char name_msg[BUFSIZ];	
	int str_len;				

	while(1)
	{
		str_len=read(sock, name_msg, BUFSIZ); // get message from server
		if(str_len==-1){ 
			perror("client read");
            exit(1);
		}
		else if(!strcmp(name_msg, "q\n")||!strcmp(name_msg, "Q\n")){ // if 'q' or 'Q'
			close(sock); // terminate and close socket
			exit(0);
		}
		
		fputs(name_msg, stdout); // print message to client
	}
	return NULL;
}