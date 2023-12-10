#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/time.h>


void* read_request(void* arg)
{
    int client_fd = *((int*)arg);
    char* buff = (char*)malloc(255*sizeof(char));
    buff= memset(buff, 0, 255);
    ssize_t bytes = recv(client_fd, buff, sizeof(buff), 0);
    if(bytes <0 )
    {
        perror("recv fail");
        struct timeval timeout;
        timeout.tv_sec = 10;  // Setează timeout-ul la 10 secunde
        timeout.tv_usec = 0;

        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("Error setting socket timeout");
        }
    }
    if(bytes > 0)
    {
        if (bytes == 0) {
            printf("Client closed the connection\n");
        }
        if(strstr(buff,"GET"))
        {
            char s[3]="OK";
            send(client_fd,s,3,0); 
        }
    } 
}



void mod1(struct sockaddr_in client_addr, int* client_fd)
{
    

        pthread_t thread_id ;
        pthread_create (&thread_id, NULL, read_request, client_fd);
    
}



int main()
{
    int server_socket; 
    int port1 = 8080;
    int fd = open("config.txt", O_RDONLY); 
     if(fd == -1)
     {
        perror("Eroare deschidere fisier");
        exit(1);
     }

    char buff[100];
    int port;

    size_t bytes;
    bytes = read(fd,buff,100);

    char *token = strtok(buff," \n");
    token = strtok(NULL," \n");
    port = atoi(token);
    token = strtok(NULL," \n");
   

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Eroare la crearea socket-ului");
        exit(1);
    }
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port1);

    if(bind(server_socket,  (struct sockaddr*) &server_address, sizeof(server_address)) == -1) // leaga serverul de port
    {
        perror("bind fail");
        exit(1);
    }

    if(listen(server_socket, 10) == -1) // ia maxim 10 conexiuni
    {
        perror("listen fail");
    }

    printf("Serverul asculta pe portul %d...\n", port);

    // int connect(server_socket,  server_address, sizeof(server_address));
    // {
    //     perror("Eroare la conectare");
    //     close(server_socket);
    //     exit(-1);
    // }

    printf("Alegeti modul de rulare:\n1) Thread - 1\n2) Pool de thread - 2\n");
    char c = getchar();
    while(1)
    {
        int client_fd; //= malloc(sizeof(int));
        struct sockaddr_in client_addr;
        socklen_t client_addr_l = sizeof(client_addr);
        if((client_fd = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_l) == -1))
        {
            perror("accept fail");
            exit(-1);
        }  
         
         if(c == '1')
        {
        //     pthread_t thread_id ;
        // pthread_create (&thread_id, NULL, read_request, (void*) client_fd);
        char* buff = (char*)malloc(255*sizeof(char));
        buff= memset(buff, 0, 255);
            ssize_t bytes = recv(client_fd, buff, sizeof(buff), 0);
            if(bytes > 0)
        {
            if (bytes == 0) {
                printf("Client closed the connection\n");
            }
            if(strstr(buff,"GET"))
            {
                char s[3]="OK";
                send(client_fd,s,3,0); 
            }
        }
        }

    
    }



   close(server_socket) ;
   return 0;
}

