# SP_2019_2_team1
##### 2019-2 System Programming Team Project

### 1.	Project Topic
-	Multi Client-Server Chatting Program
-	Socket, thread, mutex 및 커널에서 제공하는 다양한 System Call을 사용하여 여러 사용자들이 사용할 수 있도록 만든 다중 채팅 프로그램


### 2.	Project Summary
-	Socket을 사용하여 Server와 Client 간의 통신이 이루어진다.
-	Server에서 Client에게 채팅을 위한 다양한 기능을 제공한다.
-	Client는 1명 이상의 다른 Client와 채팅 할 수 있다.
-	Server에서는 Client가 선택한 메뉴, 입력한 대화 등을 확인할 수 있다.





#### <실행방법>
**1. server**
``` sh
$ gcc server.c -lpthread -o server
$ ./server <port>
```

**2. client**
``` sh
$ gcc client.c -lpthread -o client
$ ./client <name> <port> <IP>
```


