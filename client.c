#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

int main(int argc, char *argv[])
{
    struct sockaddr_in servadd;
    struct hostent *hp;
    int sock_id;
    char message[BUFSIZ];
    int messlen;

    if(ac != 5)
    {
        fprintf(stderr, "./client name port_number IPaddress");
        exit(1);
    }

    //create client socket
    sock_id = socket(AF_INET, SOCK_STREAM, 0);

    if(sock_id == -1)
    {
        perror("socket");
        exit(1);
    }

    //initialize server sockaddr structure
    bzero(&servadd, sizeof(servadd));
    hp = gethostbyname(av[3]);
    
    if(hp == NULL);
    {
        perror(av[3]);
        exit(1);
    }

    //set values in server sockaddr structure
    bcopy(hp->h_addr, (struct sockeaddr *)&servadd.sin_addr, hp->h_length);
    servadd.sin_port = htons(atoi(av[2]));
    servadd.sin_family = AF_INET;

    //request connect to server socket
    if(connect(sock_id, (struct sockaddr *)&servadd, sizeof(servadd)) != 0)
    {
        perror("connect");
        exit(1);
    }

    //read
    messlen = read(sock_id, message, BUFSIZ);

    if(messlen == -1)
    {
        perror("read");
        exit(1);
    }

    //write
    if(write(1, message, messlen) != messlen)
    {
        perror("write");
        exit(1);
    }

    close(sock_id);

        
}