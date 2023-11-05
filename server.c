#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 80

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Cream socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0); //SOCK_STREAM folosim TCP
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(-1);
    }

    // Configuram adresa serverului
    server_addr.sin_family = AF_INET; //Ipv4
    server_addr.sin_addr.s_addr = INADDR_ANY; //serverul accepta conexiuni de pe orice interfata
    server_addr.sin_port = htons(PORT); //portul 80

    
    close(server_socket);

    return 0;
}