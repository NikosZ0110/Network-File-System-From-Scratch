#include "globals.h"
#include "server.h"

Node* head = NULL;

fdNode* fdHead = NULL;

///////////Function to renew incarnation number/////////////

int renew_incarnation_number(const char *path) {
    char incnum_filename[] = INCARNATION_FILE;
    int length = strlen(path) + strlen(incnum_filename) + 1;
    char name[length];
    strcpy(name, path);
    strcat(name, incnum_filename);
    name[length - 1] = '\0'; // Ensure null terminator is added

    printf("Inc file name: %s\n", name);

    // Open file for incarnation number
    int inc_num_fd = open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (inc_num_fd == -1) {
        perror("Error opening/creating file");
        return -1; // Return -1 to indicate failure
    }

    // Read current incarnation number
    ssize_t bytesRead = read(inc_num_fd, &inc_num, sizeof(int));
    if (bytesRead == -1) {
        perror("Error reading file");
        close(inc_num_fd);
        return -1; // Return -1 to indicate failure
    }

    // Renew incarnation number
    if (bytesRead == 0) {
        inc_num = 0;
    } else {
        inc_num++;
    }

    // Write new incarnation number
    if (lseek(inc_num_fd, 0, SEEK_SET) == -1) {
        perror("Error seeking file");
        close(inc_num_fd);
        return -1; // Return -1 to indicate failure
    }
    if (write(inc_num_fd, &inc_num, sizeof(int)) == -1) {
        perror("Error writing file");
        close(inc_num_fd);
        return -1; // Return -1 to indicate failure
    }

    // Close incarnation number file
    printf("Incnum: %d\n", inc_num);
    close(inc_num_fd);

    return 0; // Return 0 to indicate success
}

//////////////////Client handling routine///////////////////////
void *handleClient(void *arg) {
    
    // Cast the argument to the ThreadArgs structure
    ThreadArgs *args = (ThreadArgs *)arg;

    // Extract the contents and client address
    char *contents = args->contents;
    struct sockaddr_in client_addr = args->client_addr;

    int client_inc_num;
    
    // Extract the first token as the client incarnation number (int)
    char *token = strtok(contents, " ");
    if (token == NULL) {
        printf("Invalid client request: Empty message\n");
        free(contents);
        free(args);
        pthread_exit(NULL);
    }

    client_inc_num = atoi(token); 

    if (client_inc_num != inc_num && client_inc_num != -1) {
        char incnum_str[33]; // Assuming the server's incarnation number can be represented in a string of length 32 + null terminator
        sprintf(incnum_str, "%d", inc_num);

        // Send the server's incarnation number back to the client
        if (sendto(sockfd, incnum_str, strlen(incnum_str), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            perror("Send failed");
            free(contents);
            free(args);
            pthread_exit(NULL);
        } 
    } else {
        // Extract the second token as the service ID (char)
        token = strtok(NULL, " ");
        if (token == NULL) {
            printf("Invalid client request: Missing svcID\n");
            free(contents);
            free(args);
            pthread_exit(NULL);
        }
        char svcId = token[0];  

        //Extract the third token as the reqnum (int)
        token = strtok(NULL, " ");
        if (token == NULL) {
            printf("Invalid client request: Missing request number\n");
            free(contents);
            free(args);
            pthread_exit(NULL);
        }
        int req_num = atoi(token); //Take reqnum as int

        int fd;
        int fd_version;
        int version;
        int size;
        int position;
        int file_size;
        int tmod;
        int cnt = 0;
        Node *foundNode = NULL;

        //Switch case for client request
        switch (svcId){
            case 'o':                
                token = strtok(NULL, " ");
                if (token == NULL) { 
                    printf("Invalid client request: Missing filename\n");
                    free(contents);
                    free(args);
                    pthread_exit(NULL);
                }
                char *fname =  strdup(token);    // Copy filename

                //Find the node
                
                //Check if the file is already open
                foundNode = findNodeByName(fname);
                if (foundNode != NULL){


                    fd = foundNode->fd;
                    //The file is already open - take current version from the list
                    version = findVersion(fd);
                }
                else{
                    //First make the proper path
                    int length = strlen(path) + strlen(fname) + 1;
                    char name[length];
                    strcpy(name, path);
                    strcat(name, fname);
                    name[length - 1] = '\0'; // Ensure null terminator is added
                    
                    //Fd open with check for failure
                    do {
                        cnt++;
                        fd = open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                    } while (fd == -1 && cnt <= 20);

                    if (fd == -1) {
                        //Concacate strings
                        char messageToCliento[(3 * 32) + 2 + 1];     //3 integers + 2 spaces + null terminator
                        sprintf(messageToCliento, "%d %d %d", inc_num, req_num, fd);

                         //Send data back to client|| incnum reqnum fd ||
                        if (sendto(sockfd, messageToCliento, strlen(messageToCliento), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                            perror("Send failed");
                            free(contents);
                            free(fname);
                            pthread_exit(NULL);
                        }
                    }

                    //Make new node and enqueue it to the list
                    //Find and return the node
                    foundNode = enqueueNode(fname, fd);
                    

                    //Check if the fd is in the fd list 
                    version = findVersion(fd);
                    if (version != -1){     //It is -> fd was closed -> increment version , take new version
                        version = renewVersion(fd);
                    } else{  //Fd is not in the list -> Add it, take version (0)
                        enqueueFdNode(fd);
                        version = 0;
                    }

                }

                //Synchronization
                sem_wait(&(foundNode->mutex));
                if ((foundNode->w_critical + foundNode->w_waiting) > 0){
                    foundNode->r_waiting++;
                    sem_post(&(foundNode->mutex));
                    sem_wait(&(foundNode->readers_q));
                    if (foundNode->r_waiting > 0){
                        foundNode->r_waiting--;
                        foundNode->r_critical++;
                        sem_post(&(foundNode->readers_q));
                    }
                    else{
                        sem_post(&(foundNode->mutex));
                    }
                }
                else{
                    foundNode->r_critical++;
                    sem_post(&(foundNode->mutex));
                }

                //Find the file size 
                file_size = foundNode->fileSize;

                 //Synchronization
                sem_wait(&(foundNode->mutex));
                foundNode->r_critical--;
                if ((foundNode->r_critical == 0) && (foundNode->w_waiting > 0)){
                    foundNode->w_waiting--;
                    foundNode->w_critical++;
                    sem_post(&foundNode->writers_q);
                }
                sem_post(&(foundNode->mutex));


                //Concacate strings
                char messageToCliento[(5 * 32) + 4 + 1];     //4 integers + 3 spaces + null terminator
                sprintf(messageToCliento, "%d %d %d %d %d", inc_num, req_num, fd, version, file_size);

                //Send data back to client|| incnum reqnum fd file_size ||
                if (sendto(sockfd, messageToCliento, strlen(messageToCliento), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                    perror("Send failed");
                    free(contents);
                    free(fname);
                    free(args);
                    pthread_exit(NULL);
                }

                //Free allocated memory
                free(fname);
                fname = NULL;

                break;

            case 'r':

                for (int i = 0; i < 4; i++) {
                    token = strtok(NULL, " ");                   
                    if (token == NULL) {
                        printf("Invalid client request: Missing data\n");
                    }                    
                    switch (i) {
                        case 0:
                            fd = atoi(token);
                            break;
                        case 1:
                            fd_version = atoi(token);
                            break;
                        case 2:
                            position = atoi(token);
                            break;
                        case 3:                            
                            size = atoi(token);
                            break;
                    }
                }
                
                //Fist of all check validity of fd version
                version = findVersion(fd);

                //If client's version is wrong send back incnum
                if (fd_version != version){
                    int minus = -1;
                    char messageToCliento[(4 * 32) + 3 + 1];     //3 integers + 2 spaces + null terminator
                    sprintf(messageToCliento, "%d %d %d %d", inc_num, req_num, fd, minus);

                    // Send the server's incarnation number back to the client
                    if (sendto(sockfd, messageToCliento, strlen(messageToCliento), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                        perror("Send failed");
                        free(contents);
                        free(args);
                        pthread_exit(NULL);
                    } 
                } else {

                    foundNode = findNodeByFd(fd);

                    if (foundNode == NULL) {
                        int minus = -1;
                        char messageToCliento[(4 * 32) + 3 + 1];     //3 integers + 2 spaces + null terminator
                        sprintf(messageToCliento, "%d %d %d %d", inc_num, req_num, fd, minus);

                        // Send the server's incarnation number back to the client
                        if (sendto(sockfd, messageToCliento, strlen(messageToCliento), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                            perror("Send failed");
                            free(contents);
                            free(args);
                            pthread_exit(NULL);
                        } 
                    }

                    //Synchronization
                    sem_wait(&(foundNode->mutex));
                    if ((foundNode->w_critical + foundNode->w_waiting) > 0){
                        foundNode->r_waiting++;
                        sem_post(&(foundNode->mutex));
                        sem_wait(&(foundNode->readers_q));
                        if (foundNode->r_waiting > 0){
                            foundNode->r_waiting--;
                            foundNode->r_critical++;
                            sem_post(&(foundNode->readers_q));
                        }
                        else{
                            sem_post(&(foundNode->mutex));
                        }
                    }
                    else{
                        foundNode->r_critical++;
                        sem_post(&(foundNode->mutex));
                    }

                    //Save file info into buffer
                    int dupfd = dup(fd);
                    lseek(dupfd, position, SEEK_SET);
                    char* buff = (char*) malloc(size + 1);
                    int r_return = read(dupfd, buff, size);
                    buff[r_return]= '\0';
                    
                    //Find the file size
                    file_size = foundNode->fileSize;

                    //Find tmod
                    tmod = foundNode->tmod;

                    //Synchronization
                    sem_wait(&(foundNode->mutex));
                    foundNode->r_critical--;
                    if ((foundNode->r_critical == 0) && (foundNode->w_waiting > 0)){
                        foundNode->w_waiting--;
                        foundNode->w_critical++;
                        sem_post(&foundNode->writers_q);
                    }
                    sem_post(&(foundNode->mutex));


                    //Concatenate strings
                    int messageSize = (5 * 32) + size+1 + 5 + 1;     //4 integers + sizeof the data string + 4 spaces + null terminator
                    char* messageToClientr = (char*)malloc(messageSize);
                    sprintf(messageToClientr, "%d %d %d %d %d %s", inc_num, req_num, file_size, tmod, r_return, buff);
                    printf("im about to send: %s\n", messageToClientr);

                    //Send data back to client |||incnum reqnum data filesize tmod ||
                    if (sendto(sockfd, messageToClientr, strlen(messageToClientr), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                        perror("Send failed");
                        free(contents);
                        free(args);
                    
                        pthread_exit(NULL);
                    }

                    //Close duplicate fd
                    close(dupfd);

                    //Free allocated memory
                    free(buff);
                    buff = NULL;
                    free(messageToClientr);
                }

                break;

            case 'w':

                for (int i = 0; i < 3; i++) {
                    token = strtok(NULL, " ");                   
                    if (token == NULL) {
                        printf("Invalid client request: Missing data\n");
                    }                    
                    switch (i) {
                        case 0:
                            fd = atoi(token);
                            break;
                        case 1:
                            fd_version = atoi(token);
                            break;
                        case 2:
                            position = atoi(token);
                            break;
                    }
                }

                //Fist of all check validity of fd version
                version = findVersion(fd);
                if (fd_version != version){
                    int minus = -1;
                    char messageToCliento[(4 * 32) + 3 + 1];     //3 integers + 2 spaces + null terminator
                    sprintf(messageToCliento, "%d %d %d %d", inc_num, req_num, fd, minus);

                    // Send the server's incarnation number back to the client
                    if (sendto(sockfd, messageToCliento, strlen(messageToCliento), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                        perror("Send failed");
                        free(contents);
                        free(args);
                        pthread_exit(NULL);
                    } 
                } else {

                    foundNode = findNodeByFd(fd);

                    if (foundNode == NULL) {
                        int minus = -1;
                        char messageToCliento[(4 * 32) + 3 + 1];     //3 integers + 2 spaces + null terminator
                        sprintf(messageToCliento, "%d %d %d %d", inc_num, req_num, fd, minus);

                        // Send the server's incarnation number back to the client
                        if (sendto(sockfd, messageToCliento, strlen(messageToCliento), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                            perror("Send failed");
                            free(contents);
                            free(args);
                            pthread_exit(NULL);
                        } 
                    }
                
                    //Save the all the remaining data sent into a buffer
                    char* wbuff = (char*) calloc(50001, 1); 
                    if (wbuff == NULL) {
                        // Handle memory allocation failure
                        perror("Memory allocation failed");
                    }

                    //Concatenate the remaining tokens into wbuff - the null terminator is added by strcat
                    while ((token = strtok(NULL, " ")) != NULL) {
                        strcat(wbuff, token);
                        strcat(wbuff, " "); // Add space between tokens where needed
                    }
                    
                    //Synchronization
                    sem_wait(&(foundNode->mutex));
                    if ((foundNode->r_critical + foundNode->w_critical) > 0){
                        foundNode->w_waiting++;
                        sem_post(&(foundNode->mutex));
                        sem_wait(&(foundNode->writers_q));
                    }
                    else{
                        foundNode->w_critical++;
                        sem_post(&(foundNode->mutex));
                    }

                    //Write onto the file
                    lseek(fd, position, SEEK_SET);
                    int wreturn = write(fd, wbuff, (strlen(wbuff)-1));

                    //Find the file size
                    file_size = lseek(fd, 0, SEEK_END);
                    foundNode->fileSize = file_size;

                    //Find tmod
                    foundNode->tmod++;
                    tmod = foundNode->tmod;

                    //Synchronization
                    sem_wait(&(foundNode->mutex));
                    foundNode->w_critical--;
                    if (foundNode->r_waiting > 0){
                        foundNode->r_waiting--;
                        foundNode->r_critical++;
                        sem_post(&(foundNode->readers_q));
                    }
                    else{
                        if (foundNode->w_waiting > 0){
                            foundNode->w_waiting--;
                            foundNode->w_critical++;
                            sem_post(&(foundNode->writers_q));
                        }
                        sem_post(&(foundNode->mutex));
                    }

                    //Concacate strings
                    char messageToClient[(5 * 32) + 4 + 1];      //4 integers + sizeof the data string + 4 spaces + null terminator
                    sprintf(messageToClient, "%d %d %d %d %d", inc_num, req_num, file_size, tmod, wreturn);

                    //Send data back to client || incnum reqnum file_size tmod ||
                    if (sendto(sockfd, messageToClient, strlen(messageToClient), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                        perror("Send failed");
                        free(contents);
                        free(wbuff);
                        free(args);
                        wbuff = NULL;
                        pthread_exit(NULL);
                    }

                    

                    //Free allocated memory
                    free(wbuff);
                    wbuff = NULL;

                }

                break;
        }
    }

    // Free allocated memory for contents
    free(contents);
    contents = NULL;

    // Free allocated memory for args structure
    free(args);
    args = NULL;

    // Exit the thread
    pthread_exit(NULL);
}

//////////////////Fd closing handling routine/////////////////////
void *handleFdClosure(){
    struct timeval current_time;
    Node *curr = head;
    Node *tmp = NULL;

    while (1){
        sleep (THIRTY_MINUTES_IN_SECONDS);

        sem_wait(&sem_list);
        gettimeofday(&current_time, NULL);
        
            while (curr != NULL) {
                
                long time_diff = (current_time.tv_sec - curr->lastUsed.tv_sec) * 1000000 +
                         (current_time.tv_usec - curr->lastUsed.tv_usec);

                if (time_diff >= THIRTY_MINUTES_IN_MICROSECONDS) {
                    if (curr->w_waiting + curr->r_waiting + curr->w_critical + curr->r_critical == 0) {
                        tmp = curr;
                        curr = curr->next;
                        close(tmp->fd);
                        free(tmp->fileName);
                        free(tmp); 
                    } else {
                        curr = curr->next;
                    }
                } else {
                   curr = curr->next;
                }
            }
        
        sem_post(&sem_list);
    }
}

////////////////Function to create a new node/////////////////////
Node* createNode(char *fname, int fd) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    //Assign new nodes values
    newNode->fileName = strdup(fname); 
    newNode->fd = fd;
    newNode->tmod = 0;
    newNode->fileSize = 0;

    gettimeofday(&(newNode->lastUsed), NULL);

    //Synchronization counters-"queues"
    newNode->w_critical = 0;
    newNode->w_waiting = 0;
    newNode->r_waiting = 0;
    newNode->r_critical = 0;
    
    if (sem_init(&(newNode->mutex), 0, 1) != 0 ||
        sem_init(&(newNode->readers_q), 0, 0) != 0 ||
        sem_init(&(newNode->writers_q), 0, 0) != 0 ) {
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }
    
    newNode->next = NULL;

    return newNode;
}

//////////////Function to append a new node to the linked list/////////////////
Node* enqueueNode(char *fname, int fd) {
    Node* newNode = createNode(fname, fd);
    
    sem_wait(&sem_list);
        
    newNode->fileSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    newNode->next = head;
    head = newNode;

    sem_post(&sem_list);
    return newNode;
}

/////////Function to search a node by name///////////
Node* findNodeByName (char *name){
    Node* current = head;

    sem_wait(&sem_list);

    while (current != NULL) {
        if (strcmp(current->fileName, name) == 0) {
            gettimeofday(&(current->lastUsed), NULL);
            sem_post(&sem_list);

            return current; // Found the node with the specified fileName
        }
        current = current->next;
    }

    sem_post(&sem_list);
    return NULL; // Node with the specified fileName not found
}

/////////Function to search a node by fd///////////
Node* findNodeByFd (int fd){
    Node* current = head;

    sem_wait(&sem_list);

    while (current != NULL) {
        if (current->fd == fd) {
            gettimeofday(&(current->lastUsed), NULL);
            sem_post(&(sem_list));
            
            return current; // Found the node with the specified fileName
        }
        current = current->next;
    }

    sem_post(&sem_list);
    return NULL; // Node with the specified fileName not found
}

////////////////Function to create a new node for fd list/////////////////////
fdNode *createFdNode(int fd){
    fdNode* newNode = (fdNode*)malloc(sizeof(fdNode));
    if (newNode == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    newNode->fd = fd;
    newNode->version = 0;

    newNode->next = NULL;

    return newNode;
}

//////////////Function to append a new fd node to the fd linked list/////////////////
void enqueueFdNode(int fd) {
    fdNode* newNode = createFdNode(fd);
    
    sem_wait(&sem_fdlist);

    newNode->next = fdHead;
    fdHead = newNode;

    sem_post(&sem_fdlist);
}

////////////////////////Function to find the version of the fd////////////////////////
int findVersion (int fd){
    fdNode* current = fdHead;
    int version;

    sem_wait(&sem_fdlist);

    while (current != NULL){
        if (current->fd == fd){
            version = current->version;
            sem_post(&sem_fdlist);
            return (version);
        }
        current = current->next;
    }

    sem_post(&sem_fdlist);
    return -1; //Fd never found
}


//////////////Function to renew the version of the fd (increment by one(1))/////////////////
int renewVersion (int fd){
    fdNode* current = fdHead;
    int version;

    sem_wait(&sem_fdlist);

    while (current != NULL){
        if (current->fd == fd){
            current->version++;
            version = current->version;
            sem_post(&sem_fdlist);
            return (version);
        }
        current = current->next;
    }

    sem_post(&sem_fdlist);
    return -1; //Fd never found
}
