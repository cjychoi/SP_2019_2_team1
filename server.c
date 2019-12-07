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
#define MAX_ROOM 256			// 최대 개설 가능한 방의 갯수
#define ROOM_ID_DEFAULT		-1	// 방의 초기 ID 값(방은 리스트로 구현되고 ID를 가진다.)

#define TRUE	1			// boolean 참(예)
#define FALSE	0			// boolean 거짓(아니오)

void *handle_serv(void *); 
void *handle_clnt(void *);		

pthread_mutex_t mutx;		// 상호배제를 위한 전역변수

typedef struct
{
	int roomId;				
	int socket;				
	char name[BUFSIZ];
}Client;

int client_size = 0;				// 접속중인 사용자 수(client_arr 배열의 size)
Client client_arr[MAX_CLNT];		// 접속중인 사용자 구조체들의 배열

typedef struct	
{
	int id;					// 방의 번호
	char name[BUFSIZ];	// 방의 이름
}Room;

int room_size = 0;				// room_arr 배열의 size
Room room_arr[MAX_ROOM];		// Room의 배열(현재 개설된 방의 배열)

int issuedId = 0;			// 발급된 ID

Client *addClient(int, char *);
void removeClient(int);
void sendMessageUser(char *, int);
void sendMessageRoom(char *, int);
int isInARoom(int);
int getIndexSpace(char *);
int getSelectedWaitingRoomMenu(char *);
void getSelectedRoomMenu(char *, char *);
Room *addRoom(char *);
void removeRoom(int);
int isExistRoom(int);
void enterRoom(Client *, int);
void createRoom(Client *);
void listRoom(Client *);
void listMember(Client *, int);
int getRoomId(int);
void printWaitingRoomMenu(Client *);
void printRoomMenu(Client *);
void printHowToUse(Client *);
void serveWaitingRoomMenu(int, Client *, char *);
void exitRoom(Client *);
void serveRoomMenu(char *, Client *, char *);
int isEmptyRoom(int);
void error_handling(char *);


int main(int argc, char *argv[])	
{
	int serv_sock, clnt_sock;		
	struct sockaddr_in serv_addr, clnt_addr;	
	int caddr_size;				
	char nick[BUFSIZ] = { 0 };
	pthread_t t_id;					
	pthread_t serv_id;
	void *thread_return; 

	if (argc != 2) {	
		printf("Usage : %s <port>\n", argv[0]);	
		exit(1);	
	}

	// 서버 소켓의 주소 초기화
	pthread_mutex_init(&mutx, NULL);			// 커널에서 Mutex 쓰기 위해 얻어온다.
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);	// TCP용 서버 소켓 생성

	memset(&serv_addr, 0, sizeof(serv_addr));		// 서버 주소 구조체 초기화
	serv_addr.sin_family = AF_INET;				// 인터넷 통신
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// 현재 IP를 이용하고
	serv_addr.sin_port = htons(atoi(argv[1]));		// 포트는 사용자가 지정한 포트 사용

													// 서버 소켓에 주소를 할당한다.
	if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind");

	// 서버 소켓을 서버용으로 설정한다.
	if (listen(serv_sock, 5) == -1)
		error_handling("listen");

	while (1)	// 무한루프 돌면서
	{
		caddr_size = sizeof(clnt_addr);	// 클라이언트 구조체의 크기를 얻고
		memset(nick, 0, sizeof(BUFSIZ));
		// 클라이언트의 접속을 받아 들이기 위해 Block 된다.(멈춘다)
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &caddr_size);

		//클라이언트에게 닉네임 받는다
		read(clnt_sock, nick, sizeof(nick));

		// 클라이언트와 접속이 되면 클라이언트 소켓을 배열에 추가하고 그 주소를 얻는다.
		Client *client = addClient(clnt_sock, nick);


		pthread_create(&t_id, NULL, handle_clnt, (void*)client); // 쓰레드 시작	
		pthread_create(&serv_id, NULL, handle_serv, (void*)client); // 

		printf("%s is connected \n", client->name);


		pthread_join(serv_id, thread_return); 
		pthread_join(t_id, thread_return);						
		
	}
	close(serv_sock);	
	return 0;
}

void *handle_serv(void *arg)
{
	Client *client = (Client *)arg;
	int serv_sock = client->socket;
	char srcv_msg[BUFSIZ]="";
	int i = 0;
	
	while(1)
	{
		fgets(srcv_msg, BUFSIZ, stdin);
		if(!strcmp(srcv_msg, "q\n") || !strcmp(srcv_msg, "Q\n"))
		{	
			for(i = 0; i < client_size; i++)
			{
				sendMessageUser(srcv_msg, client_arr[i].socket);
				
				close(client_arr[i].socket);
			}
				exit(0);
		}
		
	}
	return NULL;
}

// 클라이언트를 배열에 추가 - 소켓을 주면 클라이언트 구조체변수를 생성해 준다.
Client *addClient(int socket, char *nick)
{
	pthread_mutex_lock(&mutx);		
	Client *client = &(client_arr[client_size++]);	// 미리 할당된 공간 획득
	client->roomId = ROOM_ID_DEFAULT;					// 아무방에도 들어있지 않음
	client->socket = socket;						// 인자로 받은 소켓 저장
	strcpy(client->name, nick);
	pthread_mutex_unlock(&mutx);	
	return client;	// 클라이언트 구조체 변수 반환
}

// 클라이언트를 배열에서 제거 - 소켓을 주면 클라이언트를 배열에서 삭제한다.
void removeClient(int socket)
{
	pthread_mutex_lock(&mutx);		// 임계영역 시작
	int i = 0;
	for (i = 0; i<client_size; i++)   // 접속이 끊긴 클라이언트를 삭제한다.
	{
		if (socket == client_arr[i].socket)	// 끊긴 클라이언트를 찾았으면
		{
			while (i++<client_size - 1)	// 찾은 소켓뒤로 모든 소켓에 대해
				client_arr[i] = client_arr[i + 1]; // 한칸씩 앞으로 당김
			break;	// for문 탈출
		}
	}
	client_size--;	// 접속중인 클라이언트 수 1 감소
	pthread_mutex_unlock(&mutx);	// 임계영역 끝
}

// 모두에게 메시지를 보내는게 아니라, 특정 사용자에게만 메시지를 보낸다.
void sendMessageUser(char *msg, int socket)   // send to a members 
{	
	int length = write(socket, msg, BUFSIZ);
}

// 특정 방 안에 있는 모든 사람에게 메시지 보내기
void sendMessageRoom(char *msg, int roomId)   // send to the same room members 
{
	int i;
	
	pthread_mutex_lock(&mutx);		// 임계 영역 진입
	for (i = 0; i<client_size; i++)		// 모든 사용자들 중에서
	{
		if (client_arr[i].roomId == roomId)	// 특정 방의 사람들에게
			sendMessageUser(msg, client_arr[i].socket); // 각각 메시지전송
	}
	pthread_mutex_unlock(&mutx);	// 임계 영역 끝
}

// 특정 사용자가 방에 들어가 있습니까?
int isInARoom(int socket)	
{
	int i = 0;
	for (i = 0; i<client_size; i++)	// 클라이언트 배열에서 뒤져서
	{
		if (client_arr[i].socket == socket	// 특정 사용자가
			&& client_arr[i].roomId != ROOM_ID_DEFAULT)	// room id를 갖고있으면
			return TRUE;	// 방에 들어가 있는 것이다.
	}
	return FALSE;	// 아니면 방에 들어가 있지 않다.
}

// 특정 문자열에서 space 문자가 있는 곳의 index 번호를 구해준다.
int getIndexSpace(char *msg)
{
	int indexSpace = 0;
	int length = strlen(msg);
	int i = 0;
	for (i = 0; i<length; i++)
	{
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
int getSelectedWaintingRoomMenu(char *msg)
{
	if (msg == NULL) return -1;

	int indexSpace = getIndexSpace(msg); // 사용자메시지에서 공백문자는 구분자로 활용된다.
	if (indexSpace<0)
		return 0;

	char firstByte = msg[indexSpace + 1];	// 공백문자 이후에 처음 나타나는 문자 얻기

	return firstByte - '0';	// 사용자가 선택한 메뉴에서 48을 뺀다. atoi() 써도됨
}

// "방의 메뉴" - 채팅하다가 나가고 싶을 땐 나가기 명령이 필요하다.
void getSelectedRoomMenu(char *menu, char *msg)
{
	if (msg == NULL) return;	// 예외 처리 

	int indexSpace = getIndexSpace(msg);	// 공백문자 위치 얻기
	if (indexSpace<0) return;	// 없으면 잘못된 패킷

	char *firstByte = &msg[indexSpace + 1];	// 공백이후의 문자열 복사
	strcpy(menu, firstByte);	

								// all menus have 4 byte length. remove \n
	menu[4] = 0;	// 4바이트 에서 NULL 문자 넣어 문자열 끊기
	return;
}

// 방 생성하는 함수
Room *addRoom(char *name) // 방의 이름을 지정할 수 있다.
{
	pthread_mutex_lock(&mutx);		// 임계 영역 시작
	Room *room = &(room_arr[room_size++]);	// 패턴은 클라이언트와 동일
	room->id = issuedId++;			// 방의 ID 발급 - 고유해야 함
	strcpy(room->name, name);		// 방의 이름 복사
	pthread_mutex_unlock(&mutx);	// 임계 영역 끝
	return room;	// 생성된 방 구조체 변수의 주소 반환
}

// 방 제거하기
void removeRoom(int roomId)	// 방의 번호를 주면 배열에서 찾아 삭제한다.
{
	int i = 0;

	pthread_mutex_lock(&mutx);	// 임계 영역 진입
	for (i = 0; i<room_size; i++)	// 모든 방에 대해서
	{
		if (room_arr[i].id == roomId)	// 만약에 방을 찾았으면
		{
			while (i++<room_size - 1)	// 그 이후의 모든 방을
				room_arr[i] = room_arr[i + 1]; // 앞으로 한칸씩 당긴다.
			break;
		}
	}
	room_size--;	// 개설된 방의 갯수를 하나 줄인다.
	pthread_mutex_unlock(&mutx);	// 임계 영역 끝
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
	char her[BUFSIZ] = "---Commands in ChatRoom---\n exit : Exit Room \n list : User in this room \n help : This message \n";
	char buf[BUFSIZ] = "";
	if (!isExistRoom(roomId))	// 방이 존재하지 않으면
	{
		sprintf(buf, "[server] : invalidroomId\n");	// 못들어 간다. 에러 메시지 작성
	}
	else	// 방을 찾았으면
	{
		client->roomId = roomId;	// 클라이언트는 방의 ID를 가지게 된다.
		sprintf(buf, "[server] : **%s Join the Room**\n", client->name); // 확인 메시지 작성
		sendMessageUser(her, client->socket);
	}

	// 결과 메시지를 클라이언트에게 소켓으로 돌려준다.
	sendMessageRoom(buf, client->roomId);
	
}


void createRoom(Client *client)	
{
	int i;
	char name[BUFSIZ];
	char originRoomname[BUFSIZ];
	char cmpRoomname[BUFSIZ];

	char buf[BUFSIZ] = "";	
	sprintf(buf, "[server] : Input The Room Name:\n");	

	sendMessageUser(buf, client->socket);	

	if (read(client->socket, buf, BUFSIZ)>0)	
	{
		for (i = 0; i < room_size; i++)     
		{
			sscanf(room_arr[i].name, "%s %s", name, originRoomname);
			sscanf(buf, "%s %s", name, cmpRoomname);
			if (strcmp(originRoomname, cmpRoomname) == 0) {    
				enterRoom(client, room_arr[i].id);
				return;
			}
		}

		Room *room = addRoom(buf);			
		enterRoom(client, room->id);		
	}
}

void listRoom(Client *client)
{	
	char buf[BUFSIZ] = "";	
	int i = 0;					

	sprintf(buf, "[server] : List Room:\n");	
	sendMessageUser(buf, client->socket);	

											
	for (i = 0; i<room_size; i++)	
	{
		Room *room = &(room_arr[i]);	
										
		sprintf(buf, "RoomName : %s \n", room->name);
		sendMessageUser(buf, client->socket);
	}

	
	sprintf(buf, "Total %d rooms\n", room_size);
	sendMessageUser(buf, client->socket);
}


void listMember(Client *client, int roomId)
{
	char buf[BUFSIZ];		
	int i;					
	int sizeMember = 0;			
								
	sprintf(buf, "[server] : List Member In This Room\n");
	sendMessageUser(buf, client->socket); 

	for (i = 0; i<client_size; i++)	
	{
		if (client_arr[i].roomId == roomId)
		{
			sprintf(buf, "Name : %s\n", client_arr[i].name);
			sendMessageUser(buf, client->socket);
			sizeMember++;	
		}
	}

	
	sprintf(buf, "Total: %d Members\n", sizeMember);
	sendMessageUser(buf, client->socket);
}



int getRoomId(int socket)      
{
	int i, roomId = -1;        
	char buf[BUFSIZ] = "";   
	char Roomname[BUFSIZ] = "";
	char originRoomname[BUFSIZ] = "";
	
	sprintf(buf, "[system] : Input Room Name:\n");  
	sendMessageUser(buf, socket);        

	if (read(socket, buf, sizeof(buf))>0)      
	{
		char name[BUFSIZ] = "";
		sscanf(buf, "%s %s", name, Roomname);  
												
	}

	for (i = 0; i < room_size; i++)     
	{
		sscanf(room_arr[i].name, "%s %s", buf, originRoomname);
		if (strcmp(originRoomname, Roomname) == 0)   
			return room_arr[i].id;   
	}

	return roomId;   
}


void printWaitingRoomMenu(Client *client)
{
	char buf[BUFSIZ] = "";
	sprintf(buf, "[system] : Waiting Room Menu:\n");	
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


void printRoomMenu(Client *client)
{
	char buf[BUFSIZ] = "";
	sprintf(buf, "[system] : Room Menu:\n");		
	sendMessageUser(buf, client->socket);

	sprintf(buf, "exit) Exit Room\n");			
	sendMessageUser(buf, client->socket);

	sprintf(buf, "list) List Room\n");			
	sendMessageUser(buf, client->socket);

	sprintf(buf, "help) This message\n");		
	sendMessageUser(buf, client->socket);
}

void printHowToUse( Client *client)
{
	char buf[BUFSIZ]="";
	sprintf(buf ,"---Menu---\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "1.CreateRoom : Create chat room and enter the room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "2.ListRoom : Show all created chat rooms\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "3.EnterRoom : Enter the chat room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "4.InfoRoom : Show all users in certain chat room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "---Commands in ChatRoom---\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "1. exit : Exit room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "2. list : Users in the chat room\n");
	sendMessageUser(buf, client->socket);
	
	sprintf(buf, "3. help : Manual of Commands in chat room\n");
	sendMessageUser(buf, client->socket);

}


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
		//printWaitingRoomMenu(client);
		printHowToUse(client);
		break;

	default:
		sendMessageUser(msg, client->socket);
		//printWaitingRoomMenu(client);		// 잘못 입력했으면, 메뉴 다시 표시
		break;
	}
}

// 현재 방을 빠져 나가기
void exitRoom(Client *client)	
{
	int roomId = client->roomId;			
	client->roomId = ROOM_ID_DEFAULT;		

	char buf[BUFSIZ]="";					
	sprintf(buf,"[server] exited room id:%d\n", roomId); 
	sendMessageUser(buf, client->socket);	
	printWaitingRoomMenu(client);

	if (isEmptyRoom(roomId))			
	{
		removeRoom(roomId);		
	}
}

// 방에서 메뉴를 선택했을 때 제공할 서비스(서버가 해야 할 일)
void serveRoomMenu(char *menu, Client *client, char *msg)
{
	char buf[BUFSIZ] = "";
	printf("Server Menu : %s\n", menu);	

	if (strcmp(menu, "exit") == 0) {			
		sprintf(buf, "%s out\n", client->name);
		sendMessageRoom(buf, client->roomId);
		exitRoom(client);
	}
	else if (strcmp(menu, "list") == 0) {			
		listMember(client, client->roomId);
	}
	else if (strcmp(menu, "help") == 0) {				
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


void *handle_clnt(void *arg)	
{
	Client *client = (Client *)arg;	
	int str_len = 0, i;
	char msg[BUFSIZ];			
								
	printWaitingRoomMenu(client);	

	int clnt_sock = client->socket;	
	int roomId = client->roomId;	

									
	while ((str_len = read(clnt_sock, msg, BUFSIZ)) != 0)
	{
		printf("Read User(%d):%s\n", clnt_sock, msg); 
														 
		if (isInARoom(clnt_sock))	
		{
			char menu[BUFSIZ] = "";
			getSelectedRoomMenu(menu, msg);		
			serveRoomMenu(menu, client, msg);	
		}

		else
		{

			int selectedMenu = getSelectedWaintingRoomMenu(msg);

			printf("User(%d) selected menu(%d)\n", client->socket, selectedMenu);

			
			serveWaitingRoomMenu(selectedMenu, client, msg);
		}
	}

	
	removeClient(clnt_sock);	

	close(clnt_sock);			
	return NULL;				
}

void error_handling(char * msg)
{
	perror(msg);
	exit(1);				// 프로그램 종료
}