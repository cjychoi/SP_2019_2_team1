#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#define MAX_CLNT 256			// 최대 동시 접속자 수
#define MAX_ROOM 256			// 최대 개설 가능한 방의 개수
#define ROOM_ID_DEFAULT	-1	// 방의 초기 ID 값(방은 리스트로 구현되고 ID를 가진다.)

#define TRUE	1			// boolean 참(예)
#define FALSE	0			// boolean 거짓(아니오)

void *handle_serv(void *); //server 동작 제어
void *handle_clnt(void *); //client 동작 제어

pthread_mutex_t lock;		// mutex lock을 위한 변수

typedef struct { //client 정보를 담을 구조체
	int roomId;	//client가 있는 방
	int socket;	//client 소켓
	char name[BUFSIZ]; //client 이름
}Client;

int client_size = 0;				// 접속중인 사용자 수(client_arr 배열의 size)
Client client_arr[MAX_CLNT];		// 접속중인 사용자 구조체들의 배열

typedef struct {
	int id;					// 방의 번호
	char name[BUFSIZ];	// 방의 이름
}Room;

int room_size = 0;				// room_arr 배열의 size
Room room_arr[MAX_ROOM];		// Room의 배열(현재 개설된 방의 배열)

int issuedId = 0;			// 발급된 ID

// 함수 목록
Client *addClient(int, char *); // client_arr에 client 추가
void removeClient(int); // client_arr에서 client 삭제(client disconnected)
void sendMessageUser(char *, int); // 해당 사용자에게 message 보냄
void sendMessageRoom(char *, int); // 방의 모든 사용자에게 message 보냄
int isInARoom(int); // 해당 client가 방에 있는지 체크
int getIndexSpace(char *); // 문자열에서 공백 확인
int getSelectedWaitingRoomMenu(char *); // 대기화면에서 client가 고른 menu 받아옴
void getSelectedRoomMenu(char *, char *); // 채팅방에서 client가 고른 menu 받아옴
Room *addRoom(char *); // room_arr에 room(채팅방) 추가
void removeRoom(int); // room_arr에서 room(채팅방) 삭제
int isExistRoom(int); // 특정 room(채팅방)이 존재하는지 확인
void enterRoom(Client *, int); // client가 해당 채팅방에 입장
void createRoom(Client *); // room(채팅방) 생성
void listRoom(Client *); // client에게 채팅방 목록 제공
void listMember(Client *, int); // 해당 채팅방에 있는 모든 client 나열
int getRoomId(int); // 채팅방의 id를 얻어옴
void printWaitingRoomMenu(Client *); // 대기화면에서 사용할 수 있는 메뉴 보여줌
void printRoomMenu(Client *); // 채팅방에서 사용할 수 있는 메뉴 보여줌
void printHowToUse(Client *); // 사용방법 보여줌
void serveWaitingRoomMenu(int, Client *, char *); // 대기화면에서 client가 메뉴 선택했을 때 기능 제공
void exitRoom(Client *); // client가 채팅방에서 나감
void serveRoomMenu(char *, Client *, char *); // 채팅방에서 client가 메뉴 입력했을 때 기능 제공
int isEmptyRoom(int); // 빈 방인지 확인
void error_handling(char *); // error 처리

int main(int argc, char *argv[])	
{
	int serv_sock, clnt_sock; // server socket, client socket
	struct sockaddr_in serv_addr, clnt_addr; // server&client address structure
	int caddr_size;	// clnt_addr 구조체의 사이즈를 저장할 변수
	char nick[BUFSIZ]; // client가 채팅방에서 사용할 닉네임을 저장할 char 배열
	pthread_t t_id;	// client 처리할 thread 변수
	pthread_t serv_id; //server 처리할 thread 변수

	//사용법 알려줌
	if (argc != 2) {	
		printf("Usage: %s <port>\n", argv[0]);	
		exit(1);	
	}

	pthread_mutex_init(&lock, NULL); //initialize mutex variable
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);	// create socket(TCP)

	memset(&serv_addr, 0, sizeof(serv_addr));		// initialize serv_addr structure
	serv_addr.sin_family = AF_INET;				// internet version
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// set IP
	serv_addr.sin_port = htons(atoi(argv[1]));		// set port number

	// binding
	if (bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind");

	// listen
	if (listen(serv_sock, 5) == -1)
		error_handling("listen");

	printf("Press 'q' or 'Q' to terminate the server\n");

	while (1) {
		caddr_size = sizeof(clnt_addr);	// get the size of clnt_addr structure
		memset(nick, 0, sizeof(BUFSIZ)); // initialize nick array
		
		// accept
		clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &caddr_size);
		
		//get chatting name from client
		read(clnt_sock, nick, sizeof(nick));
		
		// add client socket to array
		Client *client = addClient(clnt_sock, nick);
		
		pthread_create(&t_id, NULL, handle_clnt, (void *)client); // start thread to handle client	
		pthread_create(&serv_id, NULL, handle_serv, (void *)client); // start thread to handle server

		printf("%s is connected \n", client->name); // print connected client's name

		pthread_detach(serv_id); // thread 소멸
		pthread_detach(t_id);	// thread 소멸
		
	}
	close(serv_sock); //close server socket	
	return 0;
}

// provide functions to handle server
void *handle_serv(void *arg)
{
	char srcv_msg[BUFSIZ]="";
	int i = 0;
	
	while(1)
	{
		fgets(srcv_msg, BUFSIZ, stdin); // get message from server
		if(!strcmp(srcv_msg, "q\n") || !strcmp(srcv_msg, "Q\n")) // if 'q' or 'Q'
		{	
			for(i = 0; i < client_size; i++) {
				sendMessageUser(srcv_msg, client_arr[i].socket); // send message to all clients
				close(client_arr[i].socket); // close all client sockets
			}
				exit(0);
		}
	}
	return NULL;
}

// add clients to client_arr - returns the Client structure variable
Client *addClient(int socket, char *nick)
{
	pthread_mutex_lock(&lock);	// client_arr가 전역변수이기 때문에 mutex lock을 건다	
	Client *client = &(client_arr[client_size++]);	// 미리 할당된 공간 획득
	client->roomId = ROOM_ID_DEFAULT; // 아무방에도 들어있지 않음
	client->socket = socket; // 인자로 받은 소켓 저장
	strcpy(client->name, nick); // store the chat name
	pthread_mutex_unlock(&lock);
	return client;	// returns structure variable
}

// remove client from client_arr
void removeClient(int socket)
{
	pthread_mutex_lock(&lock);	// mutex area begins
	int i = 0;
	for (i = 0; i<client_size; i++)   // remove disconnected client from client_arr
	{
		if (socket == client_arr[i].socket)	// find disconnected client
		{
			while (i++ < client_size - 1)	
				client_arr[i] = client_arr[i + 1]; 
			break;
		}
	}
	client_size--;	// 접속중인 클라이언트 수 1 감소
	pthread_mutex_unlock(&lock);	// mutex area ends
}

// send message to specific client
void sendMessageUser(char *msg, int socket) 
{	
	int length = write(socket, msg, BUFSIZ);
}

// send message to all users in specific room
void sendMessageRoom(char *msg, int roomId)  
{
	int i;
	pthread_mutex_lock(&lock);		// mutex area starts
	for (i = 0; i<client_size; i++)		
	{
		if (client_arr[i].roomId == roomId)	// find users(clients) in specific room
			sendMessageUser(msg, client_arr[i].socket); // send message
	}
	pthread_mutex_unlock(&lock);	// mutex area ends
}

// find whether specific client is in room
int isInARoom(int socket)	
{
	int i = 0;
	for (i = 0; i<client_size; i++)	// in client_arr
	{
		if (client_arr[i].socket == socket	// if specific client
			&& client_arr[i].roomId != ROOM_ID_DEFAULT)	// has roomID
			return TRUE;
	}
	return FALSE;
}

// 특정 문자열에서 space 문자가 있는 곳의 index 번호를 구해준다.
int getIndexSpace(char *msg)
{
	int indexSpace = 0;
	int length = strlen(msg);
	int i = 0;
	for (i = 0; i<length; i++){
		if (msg[i] == ' ')	// 공백 문자를 찾아서
		{
			indexSpace = i;
			break;
		}
	}

	if (indexSpace + 1 >= length)
	{
		return -1;
	}

	return indexSpace;	// 공백문자의 위치 반환
}

// "대기실의 메뉴"에서 선택한 메뉴를 얻어온다.(사용자 메뉴는 1자리 숫자이다.)
int getSelectedWaitingRoomMenu(char *msg)
{
	if (msg == NULL) return -1;

	int indexSpace = getIndexSpace(msg); // 사용자메시지에서 공백문자는 구분자로 활용된다.
	if (indexSpace<0)
		return 0;

	char firstByte = msg[indexSpace + 1];	// 공백문자 이후에 처음 나타나는 문자 얻기

	return firstByte - '0';	// 사용자가 선택한 메뉴를 int형으로 return.
}

// "방의 메뉴" - 채팅하다가 나가고 싶을 땐 나가기 명령이 필요하다.
void getSelectedRoomMenu(char *menu, char *msg)
{
	if (msg == NULL) return;	// 예외 처리 

	int indexSpace = getIndexSpace(msg);	// 공백문자 위치 얻기
	if (indexSpace < 0) 
		return;	// 없으면 잘못된 message

	char *firstByte = &msg[indexSpace + 1];	// 공백이후의 문자열 복사
	strcpy(menu, firstByte);	

	menu[4] = 0;	// 4바이트 에서 NULL 문자 넣어 문자열 끊기
	return;
}

// 방 생성하는 함수
Room *addRoom(char *name) // 방의 이름을 지정할 수 있다.
{
	pthread_mutex_lock(&lock);		// 임계 영역 시작
	Room *room = &(room_arr[room_size++]);	// 패턴은 클라이언트와 동일
	room->id = issuedId++;			// 방의 ID 발급 - 고유해야 함
	strcpy(room->name, name);		// 방의 이름 복사
	pthread_mutex_unlock(&lock);	// 임계 영역 끝
	return room;	// 생성된 방 구조체 변수의 주소 반환
}

// 방 제거하기
void removeRoom(int roomId)	// 방의 번호를 주면 배열에서 찾아 삭제한다.
{
	int i = 0;

	pthread_mutex_lock(&lock);	// 임계 영역 진입
	for (i = 0; i<room_size; i++)	// 모든 방에 대해서
	{
		if (room_arr[i].id == roomId)	// 만약에 방을 찾았으면
		{
			while (i++ < room_size - 1)	// 그 이후의 모든 방을
				room_arr[i] = room_arr[i + 1]; // 앞으로 한칸씩 당긴다.
			break;
		}
	}
	room_size--;	// 개설된 방의 갯수를 하나 줄인다.
	pthread_mutex_unlock(&lock);	// 임계 영역 끝
}

// 특정 방이 존재 합니까?
int isExistRoom(int roomId)	// 방의 번호를 주고, 만들어진 방인지 확인한다.
{
	int i = 0;
	for (i = 0; i<room_size; i++)		// 모든 방에 대해서
	{
		if (room_arr[i].id == roomId)	// 만약에 특정 방을 찾았으면
			return TRUE;	// 참을 반환한다.
	}
	return FALSE;	// 다 뒤졌는데 못찾으면 거짓을 반환한다.
}

// 사용자가 특정 방에 들어가기
void enterRoom(Client *client, int roomId) // 클라이언트가 roomID의 방에 들어간다.
{
	char her[BUFSIZ] = "---Commands in ChatRoom---\n exit: Exit Room\n list: User in this room\n help: This message\n";
	char buf[BUFSIZ] = "";
	if (!isExistRoom(roomId))	// 방이 존재하지 않으면
	{
		sprintf(buf, "[server]: invalidroomId\n");	// 못들어 간다. 에러 메시지 작성
	}
	else	// 방을 찾았으면
	{
		client->roomId = roomId;	// 클라이언트는 방의 ID를 가지게 된다.
		sprintf(buf, "[server]: **%s Join the Room**\n", client->name); // 확인 메시지 작성
		sendMessageUser(her, client->socket);
	}

	// 결과 메시지를 클라이언트에게 소켓으로 돌려준다.
	sendMessageRoom(buf, client->roomId);
	
}

// client creates the room
void createRoom(Client *client)	
{
	int i;
	char name[BUFSIZ]; // room name
	char originRoomname[BUFSIZ]; //
	char cmpRoomname[BUFSIZ];

	char buf[BUFSIZ] = "";	
	sprintf(buf, "[server]: Input The Room Name:\n");	

	sendMessageUser(buf, client->socket);	

	if (read(client->socket, buf, BUFSIZ)>0)	
	{
		for (i = 0; i < room_size; i++)     
		{
			sscanf(room_arr[i].name, "%s %s", name, originRoomname); // split chat name, room name
			sscanf(buf, "%s %s", name, cmpRoomname); // split chat name, room name
			
			if (strcmp(originRoomname, cmpRoomname) == 0) {  // 기존 방과 같은 이름이 있을 경우
				enterRoom(client, room_arr[i].id); // 기존방에 enter
				return;
			}
		}

		Room *room = addRoom(buf);	// [chat_name] + room name		
		enterRoom(client, room->id);		
	}
}

// list all rooms
void listRoom(Client *client)
{	
	char buf[BUFSIZ] = "";	
	int i = 0;					
	char name[BUFSIZ] = "";
	char rname[BUFSIZ] = "";

	sprintf(buf, "[server]: List Room:\n");	
	sendMessageUser(buf, client->socket);	

											
	for (i = 0; i < room_size; i++)	
	{
		Room *room = &(room_arr[i]); // get pointer of room_arr[i]
		sscanf(room_arr[i].name, "%s %s", name, rname); // split chat name, room name								
		sprintf(buf, "RoomName: %s \n", rname);
		sendMessageUser(rname, client->socket);
	}

	sprintf(buf, "Total %d rooms\n", room_size);
	sendMessageUser(buf, client->socket);
}

// list all members in specific room
void listMember(Client *client, int roomId)
{
	char buf[BUFSIZ];		
	int i;					
	int sizeMember = 0;	// count members in room		
								
	sprintf(buf, "[server]: List Member In This Room\n");
	sendMessageUser(buf, client->socket); 

	for (i = 0; i<client_size; i++)	// search client_arr
	{
		if (client_arr[i].roomId == roomId) // if client is in room
		{
			sprintf(buf, "Name: %s\n", client_arr[i].name); // print client name
			sendMessageUser(buf, client->socket);
			sizeMember++; // increase count
		}
	}

	sprintf(buf, "Total: %d Members\n", sizeMember);
	sendMessageUser(buf, client->socket);
}


// search room
int getRoomId(int socket)      
{
	int i, roomId = -1;        
	char buf[BUFSIZ] = "";   
	char Roomname[BUFSIZ] = "";
	char originRoomname[BUFSIZ] = "";
	
	sprintf(buf, "[system]: Input Room Name:\n");  
	sendMessageUser(buf, socket);        

	if (read(socket, buf, sizeof(buf))>0)      
	{
		char name[BUFSIZ] = "";
		sscanf(buf, "%s %s", name, Roomname); // get room name from client
												
	}

	for (i = 0; i < room_size; i++) // search room_arr
	{
		sscanf(room_arr[i].name, "%s %s", buf, originRoomname);
		if (strcmp(originRoomname, Roomname) == 0)  // if same room exists
			return room_arr[i].id;  // return id
	}

	return roomId;   
}

// print menu in waiting room
void printWaitingRoomMenu(Client *client)
{
	char buf[BUFSIZ] = "";
	sprintf(buf, "[system]: Waiting Room Menu:\n");	
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "1) Create Room\n");				
	sendMessageUser(buf, client->socket);

	sprintf(buf, "2) List Room\n");					
	sendMessageUser(buf, client->socket);

	sprintf(buf, "3) Enter Room\n");					
	sendMessageUser(buf, client->socket);

	sprintf(buf, "4) Info Room\n");
	sendMessageUser(buf, client->socket);

	sprintf(buf, "5) How To Use\n");
	sendMessageUser(buf, client->socket);

	sprintf(buf, "q Or Q) Quit\n");						
	sendMessageUser(buf, client->socket);
}

// print menu in chatting room
void printRoomMenu(Client *client)
{
	char buf[BUFSIZ] = "";
	sprintf(buf, "[system]: Room Menu:\n");		
	sendMessageUser(buf, client->socket);

	sprintf(buf, "exit) Exit Room\n");			
	sendMessageUser(buf, client->socket);

	sprintf(buf, "list) List Room\n");			
	sendMessageUser(buf, client->socket);

	sprintf(buf, "help) This message\n");		
	sendMessageUser(buf, client->socket);
}

// print manual
void printHowToUse(Client *client)
{
	char buf[BUFSIZ]="";
	sprintf(buf ,"---Menu---\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "1.CreateRoom: Create chat room and enter the room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "2.ListRoom: Show all created chat rooms\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "3.EnterRoom: Enter the chat room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "4.InfoRoom: Show all users in certain chat room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "---Commands in ChatRoom---\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "1. exit: Exit room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "2. list: Users in the chat room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "3. help: Manual of Commands in chat room\n");
	sendMessageUser(buf, client->socket);

}

// serve functions in waiting room
void serveWaitingRoomMenu(int menu, Client *client, char *msg) 
{
	int roomId = ROOM_ID_DEFAULT;
	switch (menu)	
	{
	case 1:
		createRoom(client);					
		break;
	case 2:
		listRoom(client);					
		break;
	case 3:
		roomId = getRoomId(client->socket);	
		enterRoom(client, roomId);
		break;
	case 4:
		roomId = getRoomId(client->socket);
		listMember(client, roomId);
		break;
	case 5:
		printHowToUse(client);
		break;

	case 65:
		printf("%s is disconnected \n", client->name);
		removeClient(client->socket);
		break;

	default:
		sendMessageUser(msg, client->socket);
		break;
	}
}

// 현재 방을 빠져 나가기
void exitRoom(Client *client)	
{
	int roomId = client->roomId; // get room id	
	client->roomId = ROOM_ID_DEFAULT; // client의 room id -1로 초기화		

	char buf[BUFSIZ]="";					
	sprintf(buf,"[server] exited room id:%d\n", roomId); // client가 room을 빠져나갔다는 message 전송
	sendMessageUser(buf, client->socket);	
	printWaitingRoomMenu(client);

	if (isEmptyRoom(roomId)) // 방이 비었으면 삭제			
	{
		removeRoom(roomId);		
	}
}

// 방에서 메뉴를 선택했을 때 제공할 서비스(서버가 해야 할 일)
void serveRoomMenu(char *menu, Client *client, char *msg)
{
	char buf[BUFSIZ] = "";
	printf("Server Menu: %s\n", menu);	

	if (strcmp(menu, "exit") == 0) { // exit
		sprintf(buf, "%s out\n", client->name);
		sendMessageRoom(buf, client->roomId);
		exitRoom(client);
	}
	else if (strcmp(menu, "list") == 0) { // list members in room	
		listMember(client, client->roomId);
	}
	else if (strcmp(menu, "help") == 0) { // print manual		
		printRoomMenu(client);
	}
	else    // send normal chat message
		sendMessageRoom(msg, client->roomId);	
}

// 특정 방에 사람이 있습니까?
int isEmptyRoom(int roomId)
{
	int i = 0;
	for (i = 0; i<client_size; i++)		
	{
		if (client_arr[i].roomId == roomId)	
			return FALSE;			
	}
	return TRUE;					
}

// functions to handle client
void *handle_clnt(void *arg)	
{
	Client *client = (Client *)arg;	
	int str_len = 0, i;
	char msg[BUFSIZ];			
								
	printWaitingRoomMenu(client); // print waiting room menu

	int clnt_sock = client->socket;	// get client socket
	int roomId = client->roomId;	// get roomID of client is in

									
	while ((str_len = read(clnt_sock, msg, BUFSIZ)) != 0)
	{
		printf("Read User(%d): %s\n", clnt_sock, msg); 
														 
		if (isInARoom(clnt_sock)) // client가 방에 있으면
		{
			char menu[BUFSIZ] = "";
			getSelectedRoomMenu(menu, msg);		
			serveRoomMenu(menu, client, msg);	
		}

		else // waiting room에 있으면
		{
			int selectedMenu = getSelectedWaitingRoomMenu(msg);
			printf("User(%d) selected menu(%d)\n", clnt_sock, selectedMenu);
			serveWaitingRoomMenu(selectedMenu, client, msg);
		}
	}

	printf("%s is disconnected \n", client->name);
	removeClient(clnt_sock);	

	close(clnt_sock);			
	return NULL;				
}

void error_handling(char *msg)
{
	perror(msg);
	exit(1);				// 프로그램 종료
}