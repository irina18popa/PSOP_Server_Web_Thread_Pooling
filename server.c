    #include <arpa/inet.h>
    #include <errno.h>
    #include <stdio.h>
    #include <string.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <fcntl.h>
    #include <sys/types.h>

    #include <sys/stat.h>

    //#define PORT 8080
    #define BUFFER_SIZE 1024
    #define THREAD_NR 100
    #define QUEUE 256

    pthread_t new_tids[THREAD_NR] = {0};


    typedef struct {
        int task_id;
    } Task;

    typedef struct {
        pthread_t thread_id;
    
    } WorkerThread;

    typedef struct {
        WorkerThread workers[2];
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int task_count;
        Task tasks[100]; // Assuming a fixed-size task queue for simplicity
    } ThreadPool;



    typedef struct 
    {
        int running_fd;
        int server_sock_fd, server_sock_len;
        struct sockaddr_in server_sock_addr;
        int client_sock_len;
        struct sockaddr_in client_sock_addr;
        char* buff;
        char* resp;
        char method[BUFFER_SIZE], url[BUFFER_SIZE], version[BUFFER_SIZE];
    }parameters;

    int nr_conexiuni = 0;
    parameters* conexiuni[QUEUE];
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;

    typedef struct {
        char filename[512];
        off_t offset;              /* for support Range */
        size_t end;
    } http_request;

    const char *get_mime_type(const char *file_ext) {
        if (strcmp(file_ext, "html") == 0 || strcmp(file_ext, "htm") == 0) {
            return "text/html";
        } else if (strcmp(file_ext, "txt") == 0) {
            return "text/plain";
        } else if (strcmp(file_ext, "jpg") == 0 || strcmp(file_ext, "jpeg") == 0) {
            return "image/jpeg";
        } else if (strcmp(file_ext, "png") == 0) {
            return "image/png";
        } else {
            return "application/octet-stream";
        }
    }

    void creareRaspuns(const char *file_name, 
                            const char *file_ext, 
                            char *response, 
                            size_t *response_len) {
        // build HTTP header
        const char *mime_type = get_mime_type(file_ext);
        char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
        snprintf(header, BUFFER_SIZE,
                "HTTP/1.1 200 OK\r\n"
                "Server: webserver-c\r\n"
                "Content-Type: %s\r\n"
                "\r\n",
                mime_type);

        // if file not exist, response is 404 Not Found
        int file_fd = open(file_name, O_RDONLY);
        if (file_fd == -1) {

            snprintf(response, BUFFER_SIZE,
                    "HTTP/1.1 404 Not Found\r\n"
                    "Server: webserver-c\r\n"
                    "Content-Type: text/plain\r\n"
                    "\r\n"
                    "404 Not Found\n");
            *response_len = strlen(response);
            return;
        }


        // get file size for Content-Length
        struct stat file_stat;
        fstat(file_fd, &file_stat);
    // off_t file_size = file_stat.st_size;

        // copy header to response buffer
        *response_len = 0;
        memcpy(response, header, strlen(header));
        *response_len += strlen(header);

        // copy file to response buffer
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, response + *response_len, BUFFER_SIZE - *response_len)) > 0) 
        {
            *response_len += bytes_read;
        }
        free(header);
        close(file_fd);
    }

    char *get_file_extension( char *file_name) {
        char *dot = strrchr(file_name, '.');
        if (!dot || dot == file_name) {
            return "";
        }
        return dot + 1;
    }

    void creareRaspunsDelete(const char *file_name, const char *file_ext,  char *response,  size_t *response_len)
        {
            const char *mime_type = get_mime_type(file_ext);
            //char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
            int delete_status = remove(file_name);
            if (delete_status == 0) 
            {
                snprintf(response, BUFFER_SIZE,
                    "HTTP/1.1 200 OK\r\n"
                    "Server: webserver-c\r\n"
                    "Content-Type: %s\r\n"
                    "\r\n"
                    "Resource deleted successfully\n",
                    mime_type);
            }
            else
            {
                snprintf(response, BUFFER_SIZE,
                        "HTTP/1.1 404 Not Found\r\n"
                        "Server: webserver-c\r\n"
                        "Content-Type: text/plain\r\n"
                        "\r\n"
                        "404 Not Found\n");
                *response_len = strlen(response);
            }

            return ;
        }


    char *url_decode(const char *src) {
        size_t src_len = strlen(src);
        char *decoded = malloc(src_len + 1);
        size_t decoded_len = 0;

        for (size_t i = 0; i < src_len; i++) {
            if (src[i] == '%' && i + 2 < src_len) {
                int hex_val;
                sscanf(src + i + 1, "%2x", &hex_val);
                decoded[decoded_len++] = hex_val;
                i += 2;
            } else {
                decoded[decoded_len++] = src[i];
            }
        }

        decoded[decoded_len] = '\0';
        return decoded;
    }

    void add_connection(parameters* conex)
    {
        pthread_mutex_lock(&queue_mutex);
        conexiuni[nr_conexiuni] = conex;
        nr_conexiuni++;
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&queue_cond);
    }

    void accept_connections(parameters* args)
    {
        args->running_fd = accept(args->server_sock_fd, (struct sockaddr *)&args->server_sock_addr,
                                (socklen_t *)&args->server_sock_len);

        // Accept incoming connections
            if (args->running_fd < 0) {
                perror("webserver (accept)");
                //continue;
                exit(1);
            }
            printf("connection accepted\n");
    }

    void get_socket_name(int newsockfd, parameters* param)
    {
        int sockn = getsockname(newsockfd, (struct sockaddr *)&param->client_sock_addr,
                                (socklen_t *)&param->client_sock_len);
            if (sockn < 0) {
                perror("eroare server (getsockname)");
                exit(1);
            }
    }

    void read_from_sock(int sock_fd, parameters* args)
    {
        if(recv(sock_fd, args->buff, BUFFER_SIZE, 0) < 0)
        {
            perror("Eroare server (recv)");
            exit(1);
        }
        printf("\nMesajul clientului este \n %s \n", args->buff);
    }

    void execute_routine(parameters* args)
    {
        printf("\nA inceput executia thread-ului cu nr %ld\n\n", pthread_self());
        
        //char method[BUFFER_SIZE], url[BUFFER_SIZE], version[BUFFER_SIZE];
        sscanf(args->buff, "%s %s %s", args->method, args->url, args->version);
        
        printf("Acesta este raspunsul: \n[%s:%u] %s %s %s\n", inet_ntoa(args->client_sock_addr.sin_addr),
                ntohs(args->client_sock_addr.sin_port), args->method, args->version, args->url);

        //  printf("Acesta este raspunsul: \n[%s:%u] %s %s %s\n", inet_ntoa(args->client_sock_addr.sin_addr),
        //         ntohs(args->client_sock_addr.sin_port), args->method, "version", args->url);
        
        char *file_name_start = strchr(args->url, '/');
        if (file_name_start != NULL) {
        file_name_start++;  // Ne deplasăm dincolo de caracterul '/'
        }
    
        char *url_decoded_file_name = url_decode(file_name_start);
        
        
        char *file_ext = get_file_extension(url_decoded_file_name);

        
        char response[1024]; //= (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
        size_t response_len;

        if (strcmp(args->method, "GET") == 0) 
        {
            creareRaspuns(url_decoded_file_name, file_ext, response, &response_len);
        }
        else if (strcmp(args->method, "DELETE") == 0) {
            creareRaspunsDelete(url_decoded_file_name, file_ext, response, &response_len);
        }

        
        send(args->running_fd, response, response_len, 0);

        
        args->resp = strdup(response);
        free(url_decoded_file_name);
        
        int valwrite = send(args->running_fd, args->resp, strlen(args->resp),0);
        if (valwrite < 0) {
            perror("webserver (write)");
            exit(1);
        }
    }


    void* thr_routine(void* param)
    {
        parameters* args=(parameters*)param;

        //printf("\n%d\n", args->running_fd);
        //printf("\n%d\n", args->running_fd);
        get_socket_name(args->running_fd,args);
        read_from_sock(args->running_fd, args);

        // Parsam request-ul
        execute_routine(args);

        close(args->running_fd);
        free(args);
        return NULL;
    }



    void* thr_pool_routine()
    {
        while (1)
        {
            parameters* my_param;
            pthread_mutex_lock(&queue_mutex);
            if(nr_conexiuni == 0)
            {
                pthread_cond_wait(&queue_cond, &queue_mutex);
            }
            //*args = conexiuni[0];
            my_param = conexiuni[0];
            for(int i=0;i<nr_conexiuni;i++)
            {
                conexiuni[i] = conexiuni[i+1];
            }
            nr_conexiuni--;

            pthread_mutex_unlock(&queue_mutex);
            // Parsam request-ul
            //sleep(5);
            get_socket_name(my_param->running_fd, my_param);
            read_from_sock(my_param->running_fd, my_param);
            execute_routine(my_param);

            close(my_param->running_fd);
            free(my_param);
        }

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
        if(bytes < 0)
        {
            perror("Problema la file-ul de config\n");
        }

        char *token = strtok(buff," \n");
        token = strtok(NULL," \n");
        port = atoi(token);


        char buffer[BUFFER_SIZE];
        // char resp[] = "HTTP/1.0 200 OK\r\n"
        //               "Server: webserver-c\r\n"
        //               "Content-type: text/html\r\n\r\n"
        //               "<html>hello, we're Irina & Sumi</html>\r\n";

    
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
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(0); 
        client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

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
                //parameters param;
                pthread_t tid[THREAD_NR];
                int i=0;
                
                while (1)
                {
                    int running_fd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addrlen);
                    
                    parameters* param = (parameters*)malloc(sizeof(parameters));
                    param->running_fd = running_fd;
                    param->buff = buffer;
                    param->client_sock_addr = client_addr;
                    param->client_sock_len = client_addrlen;
                    param->server_sock_addr = host_addr;
                    param->server_sock_fd = sockfd;
                    param->server_sock_len = host_addrlen;

                // accept_connections(&param);
                    pthread_create(&tid[i], NULL, thr_routine, param);

                // free(param); // Eliberare memoria pentru structura de parametri dupa ce thread-ul a terminat executia
                    i++;
                }
            }
            else if (c == '2')
            {
                // parameters param;
                pthread_t th_pool[THREAD_NR];
                pthread_mutex_init(&queue_mutex, NULL);
                pthread_cond_init(&queue_cond, NULL);

                for (int i = 0; i < THREAD_NR; i++) {
                    if (pthread_create(&th_pool[i], NULL, &thr_pool_routine, NULL) != 0) 
                        {
                            perror("Eroare la crearea de thread-uri\n");
                        }
                }

                while (1)
                {
                    parameters* param_din = (parameters*)malloc(sizeof(parameters));
                    param_din->running_fd = 0;
                    param_din->buff=buffer;
                    param_din->client_sock_addr=client_addr;
                    param_din->client_sock_len=client_addrlen;
                    //param_din->resp=resp;
                    param_din->server_sock_addr=host_addr;
                    param_din->server_sock_fd=sockfd;
                    param_din->server_sock_len=host_addrlen;
                    
                    accept_connections(param_din);
                    // get_socket_name(param.running_fd, &param);
                    // read_from_sock(param.running_fd, &param);
                    add_connection(param_din);
                }
            }
            
        pthread_mutex_destroy(&queue_mutex);
        pthread_cond_destroy(&queue_cond);
        return 0;
    }