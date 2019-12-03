#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define HOSTLEN 256
#define MAX 1000

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
        fprintf(stderr, "./server port_number IPaddress");
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