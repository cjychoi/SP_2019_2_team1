#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#define HOSTLEN 256
#define MAX 1000

#define TRUE 1
#define FALSE 0

//room structure
typedef struct{
    int id; //room number
    char name[BUFSIZ]; //room name
}Room;

//client structure
typedef struct{
    int socket; //client socket
    char name[BUFSIZ]; //client name
    Room *room; //room in which client entered
}Client;

int client_size = 0; //number of clients
int room_size = 0; //number of rooms
int room_id = 0;

Client clientArr[MAX];
Room roomArr[MAX];

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

Client *addClient(int, char *); //add client in array
void deleteClient(int); //delete client from array
Room *addRoom(char *); //add room in array
void deleteRoom(int); //delete room from array
void sendMessageRoom(char *, int); //send message to every clients in room
void sendMessageUser(char *, int); //send message to specific client in room
void printWaitingManual(); //print Manual when client executes program

int main(int argc, char *argv[])
{
    struct socketaddr_in saddr;
    struct hostent *hp;
    char hostname[HOSTLEN];
    char *ip;
    int sock_id, sock_fd, portnum;
    FILE *sock_fp;

    if(argc != 3)
    {
        fprintf(stderr, "%s port_number IPaddress", argv[0]);
        exit(1);
    }

    portnum = atoi(argv[1]);


    //create server socket
    sock_id = socket(PF_INET, SOCK_STREAM, 0);

    if(sock_id == -1)
    {
        perror("socket");
        exit(1);
    }

    //initialize server socketaddr_in structure
    bzero((void *)&saddr, sizeof(saddr));

    //set values in server socketaddr_in structure
    gethostname(hostname, HOSTLEN);
    hp = gethostbyname(hostname);

    bcopy((void *)hp->h_addr, (void *)&saddr.sin_addr, hp->h_length);
    saddr.sin_port = htons(portnum);
    saddr.sin_family = AF_INET;

    //bind
    if(bind(sock_id, (struct sockaddr *)&saddr, sizeof(saddr))!= 0)
    {
        perror("bind");
        exit(1);
    }
        
    //listen
    if(listen(sock_id, 1) != 0)
    {
        perror("listen");
        exit(1);
    }
        

    while(1){
        //accept client request
        sock_fd = accept(sock_id, NULL, NULL);

        if(sock_fd == -1)
        {
            perror("accept");
            exit(1);
        }
            
        //read client socket
        sock_fp = fdopen(sock_fd, "r");
        if(sock_fp = NULL)
        {
            perror("fdopen");
            exit(1);
        }
        fclose(sock_fp);
    }  
}

void *handle_serv(void *arg)
{
    Client client = (Client *)arg;
    int serv_sock = client->socket;
    char srcv_msg[BUFSIZ]="";
    int i=0;

    while(1){
        fgets(srcv_msg, BUFSIZ, stdin);
        if(!strcmp(srcv_msg, "q\n") || !strcmp(srcv_msg, "Q\n"))
        {
            for(i=0; i<client_size; i++){
                sendMessageUser(srcv_msg, clientArr[i].socket);
                close(clientArr[i].socket);
            }

            exit(0);
        }
    }

    return NULL;
}

Client *addClient(int socket, char *name)
{
    pthread_mutex_lock(&lock);
    Client *client=clientArr[client_size++]
    client->room->id = -1;
    client->room->name = "";
    strcpy(client->name, name);
    client->socket = socket;
    pthread_mutex_unlock(&lock);
    return client;
}

void deleteClient(int socket)
{
    pthread_mutex_lock(&lock);
    int i=0;
    for(i=0; i<client_size; i++){
        if(socket == clientArr[i].socket)
        {
            while(i<client_size-1){
                clientArr[i]=clientArr[i+1];
                i++;
            }
            break;
        }
    }

    client_size--;
    pthread_mutex_unlock(&lock);
}

Room *addRoom(char *name)
{
    pthread_mutex_lock(&lock);
    Room *room = &(roomArr[room_size++]);
    room->id = room_id++;
    strcpy(room->name, name);
    pthread_mutex_unlock(&lock);
    return room;
}

void deleteRoom(int roomID)
{
    int i=0;
    pthread_mutex_lock(&lock);
    for(i=0; i<room_size; i++){
        if(roomArr[i].id == roomID)
        {
            while(i<room_size-1){
                roomArr[i] = roomArr[i+1];
                i++;
            }
            break;
        }
    }
    room_size--;
    pthread_mutex_unlock(&lock);
}

void sendMessageRoom(char *msg, int roomID)
{
    int i=0;
    pthread_mutex_lock(&lock);
    for(i=0; i<client_size; i++){
        if(clientArr[i].room->id == roomID)
            sendMessageUser(msg, clientArr[i].socket);
    }
    pthread_mutex_unlock(&lock);
} 
void sendMessageUser(char *msg, int socket)
{
    if(write(socket, msg, BUFSIZ)==-1)
    {
        perror("user write");
        exit(1);
    }
}

int existRoom(int roomID)
{
    int i=0;
    for(i=0; i<room_size; i++){
        if(roomArr[i].id == roomID)
            return TRUE;
    }

    return FALSE;
}

int existClient(int socket)
{
    int i=0;
    for(i=0; i<client_size; i++){
        if(clientArr[i].socket == socket && clientArr[i].room->id != -1)
        {
            return TRUE;
        }
    }
    return FALSE;
}

int getSelectedWaitingMenu(char *msg)
{
    if(msg == NULL)
        return -1;

    if (strcmp(msg, "\n")==0)
        return 0;

    return msg - '0';
}

void createRoom(Client *client)
{
    int i;
    char name[BUFSIZ];
    char originRoomName[BUFSIZ];
    char cmpRoomName[BUFSIZ];
    char buf[BUFSIZ]="";
    
    sprintf(buf, "[server] : Input the Room Name:\n");
    sendMessageUser(buf, client->socket);

    if(read(client->socket, buf, BUFSIZ)>0)
    {
        for(i=0; i<room_size; i++){
            sscanf(roomArr[i].name, "%s %s", name, originRoomName);
            sscanf(buf, "%s %s", name, cmpRoomName);
            if(strcmp(originRoomName, cmpRoomName)==0)
            {
                enterRoom(client, arrRoom[i].id);
                return;
            }
        }
    }

    Room *room = addRoom(buf);
    enterRoom(client, room->id);
}

void listRoom(Client *client)
{
    char buf[BUF_SIZE] = "";
	int i = 0;

	sprintf(buf, "[server] : List Room:\n");
	sendMessageUser(buf, client->socket);	

											
	for (i = 0; i<room_size; i++)	
	{
		Room * room = &(roomArr[i]);	
										
		sprintf(buf, "RoomName : %s \n", room->name);
		sendMessageUser(buf, client->socket);
	}

	
	sprintf(buf, "Total %d rooms\n", sizeRoom);
	send
    
    MessageUser(buf, client->socket);
}

void listMember(Client * client, int roomId) 
{
	char buf[BUF_SIZE] = "";		
	int i = 0;					
	int sizeMember = 0;			

								
	sprintf(buf, "[server] : List Member In This Room\n");
	sendMessageUser(buf, client->socket); 

	for (i = 0; i<sizeClient; i++)	
	{
		if (clientArr[i].room->id == roomId)	
		{
			sprintf(buf, "Name : %s\n", clientArr[i].name);
			sendMessageUser(buf, client->socket);
			sizeMember++;	
		}
	}

	sprintf(buf, "Total: %d Members\n", sizeMember);
	sendMessageUser(buf, client->socket);
}

void enterRoom(Client *client, int roomID)
{
    char manual[BUFSIZ] = "---Commands in ChatRoom---\nexit : Exit Room\nlist : List Users in this Room\nhelp : Print This Manual\n";
    char buf[BUFSIZ] = "";

    if(!existRoom(roomID))
    {
        sprintf(buf, "[server] : Invalid Room ID\n");
    }

    else
    {
        client->room->id = roomID;
        sprintf(buf, "[server] : %s Join the Room\n", clientArr->name);
        sendMessageUser(manual, client->socket);
    }

    sendMessageRoom(buf, client->room->id);
}

