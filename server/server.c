#include "globals.h"
#include "server.h"

char *path;          //Given by user as the root directory 
int inc_num;

int sockfd;

sem_t sem_list;      //Semaphore to add new nodes to list/ read info from the list/ change list's info
sem_t sem_fdlist;    //Semapore to add new nodes/change node contents/read node contents

int main (int argc, char *argv[]) {
    
    //Buffer to store client sent data
    char buffer[MAX_BUFFER_SIZE];
    char ip[INET_ADDRSTRLEN];

    //Client and server addresses
    struct sockaddr_in server_addr, client_addr; 

    socklen_t client_addr_len, addr_len;
    addr_len = sizeof(server_addr);

    //Check if directory is given
    if (argc != 2){
        printf("Requires Directory\n");
        return(0);
    }
    else
    {
        path = argv[1];     
    }

    printf("Path: %s\n", path);

    //Initiliaze list semaphore
    if (sem_init(&sem_list, 0, 1) != 0){
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }

    //Initialize fd list semaphore
    if (sem_init(&sem_fdlist, 0, 1) != 0){
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }

    //Create new thread for fd use checking 
    pthread_t fd_tid;
    if (pthread_create(&fd_tid, NULL, handleFdClosure, NULL) != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    //Renew incarnation number
    if (renew_incarnation_number(path) == -1) {
        printf("Failed to renew incarnation number.\n");
        return EXIT_FAILURE;
    }

    client_addr_len = sizeof(client_addr);
    
    

    //Create a UDP Socket 
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    } 
    
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));   
    
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(PORT); 
    server_addr.sin_family = AF_INET;  
   
    // bind server address to socket descriptor 
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }  


    // Get the socket name to retrieve the port number
    if (getsockname(sockfd, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        perror("getsockname failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Print the port number
    printf("Listening on Port: %d\n", ntohs(server_addr.sin_port));

    // Get the list of network interfaces
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Iterate through the list of interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        
        if (ifa->ifa_addr->sa_family == AF_INET) { // Check it is IPv4
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
            printf("Listening on IP: %s\n", ip);
        }
    }

    freeifaddrs(ifaddr);




    //Receive requests
    while (1) {
        
        memset(buffer, 0, MAX_BUFFER_SIZE);

        int bytes_received = recvfrom(sockfd, (char *)buffer, MAX_BUFFER_SIZE, 0,
                            (struct sockaddr *)&client_addr, &client_addr_len);

        if (bytes_received < 0) {
            perror("Receive failed");
            exit(EXIT_FAILURE);
        }

        buffer[bytes_received] = '\0'; // Null-terminate the received data

        printf("Received message '%s' from client with IP: %s and port: %d\n",
               buffer, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
       
        // Create a new thread arguments structure
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (args == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }

        args->contents = strdup(buffer); // Copy buffer contents
        args->client_addr = client_addr; // Copy client address

        // Create a new thread to handle the client request
        pthread_t tid;
        if (pthread_create(&tid, NULL, handleClient, (void *)args) != 0) {
            perror("Thread creation failed");
            free(args); // Free allocated memory on failure
            exit(EXIT_FAILURE);
        }

    }

    return 0;
}

