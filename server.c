#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>

#define PORT 80
#define BUFFER_SIZE 1024
#define THREAD_NR 4
#define QUEUE 256

typedef struct 
{
    int server_sock_fd, server_sock_len;
    struct sockaddr_in server_sock_addr;
    int client_sock_len;
    struct sockaddr_in client_sock_addr;
    char* buff;
    char* resp;
}parameters;

int nr_conexiuni = 0;
parameters conexiuni[QUEUE];
pthread_mutex_t queue_mutex;
pthread_cond_t queue_cond;

void add_connection(parameters conex)
{
    pthread_mutex_lock(&queue_mutex);
    conexiuni[nr_conexiuni] = conex;
    nr_conexiuni++;
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&queue_cond);
}

void accept_connections(int* newsockfd)
{
    if (newsockfd < 0) {
            perror("webserver (accept)");
            exit(1);
        }
        printf("connection accepted\n");
}

int get_socket_name(int* newsockfd, parameters* param)
{
    int sockn = getsockname(newsockfd, (struct sockaddr *)&param->client_sock_addr,
                            (socklen_t *)&param->client_sock_len);
        if (sockn < 0) {
            perror("webserver (getsockname)");
            exit(1);
        }
    return sockn;
}



void* thr_routine(void* param)
{
    pthread_mutex_lock(&queue_mutex);
    while (nr_conexiuni == 0)
    {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    
    
    parameters* args=(parameters*)param;

    int newsockfd = accept(args->server_sock_fd, (struct sockaddr *)&args->server_sock_addr,
                            (socklen_t *)&args->server_sock_len);


    printf("\nA inceput executia thread-ului cu nr %d\n\n", pthread_self());

    // Accepta conexiuni (curenta + viitoare)
        accept_connections(&newsockfd);

        // EXtragem adresa clientului
        int sockn = get_socket_name(&newsockfd, args);

        // Citim cererea din socket
        if(recv(newsockfd, args->buff, BUFFER_SIZE,0) < 0)
        {
            perror("webserver (recv)");
            exit(1);
        }
        printf("\nMesajul clientului este \n %s \n", args->buff);



        // Read the request
        char method[BUFFER_SIZE], url[BUFFER_SIZE], version[BUFFER_SIZE];
        sscanf(args->buff, "%s %s %s", method, url, version);
        printf("Acesta este raspunsul: \n[%s:%u] %s %s %s\n", inet_ntoa(args->client_sock_addr.sin_addr),
               ntohs(args->client_sock_addr.sin_port), method, version, url);

        // Write to the socket
        int valwrite = send(newsockfd, args->resp, strlen(args->resp),0);
        if (valwrite < 0) {
            perror("webserver (write)");
            exit(1);
        }
        pthread_mutex_unlock(&queue_mutex);

        close(newsockfd);
    
        return NULL;
}

int main() {

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


    char buffer[BUFFER_SIZE];
    char resp[] = "HTTP/1.0 200 OK\r\n"
                  "Server: webserver-c\r\n"
                  "Content-type: text/html\r\n\r\n"
                  "<html>hello, we're Irina & Sumi</html>\r\n";

    // Cream un socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) 
    {
        perror("webserver (socket)");
        return 1;
    }
    printf("socket created successfully\n");
   

    // Cream adresa pe care s o atribuim socket-ului
    struct sockaddr_in host_addr;
    int host_addrlen = sizeof(host_addr);
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(port);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Adresa clientului
    struct sockaddr_in client_addr;
    int client_addrlen = sizeof(client_addr);

    // Atribuim adresa serverului 
    if (bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0) {
        perror("webserver (bind)");
        return 1;
    }
    printf("socket successfully bound to address %d\n", host_addr.sin_addr.s_addr);

    // Serverul asculta...
    if (listen(sockfd, SOMAXCONN) != 0) {
        perror("webserver (listen)");
        return 1;
    }
    printf("Serverul asculta noi conexiuni...\n");

    printf("Alegeti modul de rulare:\n1) New Thread - 1\n2) Pool de thread - 2\n");
        
        char c = getchar(); 

        if(c == '1')
        {
            parameters param;

            while (1)
            {
                
                pthread_t new_tid;

                param.buff=buffer;
                param.client_sock_addr=client_addr;
                param.client_sock_len=client_addrlen;
                param.resp=resp;
                param.server_sock_addr=host_addr;
                param.server_sock_fd=sockfd;
                param.server_sock_len=host_addrlen;

                pthread_create(&new_tid, NULL, thr_routine, &param);

                pthread_join(new_tid, NULL);
            }
        }
        else if (c == '2')
        {
            parameters param;
            pthread_t th_pool[THREAD_NR];
            pthread_mutex_init(&queue_mutex, NULL);
            pthread_cond_init(&queue_cond, NULL);

            param.buff=buffer;
            param.client_sock_addr=client_addr;
            param.client_sock_len=client_addrlen;
            param.resp=resp;
            param.server_sock_addr=host_addr;
            param.server_sock_fd=sockfd;
            param.server_sock_len=host_addrlen;

            for (int i = 0; i < THREAD_NR; i++) {
                if (pthread_create(&th_pool[i], NULL, &thr_routine, &param) != 0) 
                {
                    perror("Eroare la crearea de thread-uri\n");
                }
            }

            while (1)
            {
                add_connection(param);
            }

            for (int i = 0; i < THREAD_NR; i++) {
                if (pthread_join(&th_pool[i], NULL) != 0); 
                {
                    perror("Eroare la asteptarea thread-urilor\n");
                }
            }
        }
        
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&queue_cond);
    return 0;
}