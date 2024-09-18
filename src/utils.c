#include "../include/utils.h"
#include "../include/server.h"
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <stdint.h>

//Declare a global variable to hold the file descriptor for the server socket
int master_fd = 0;
//Declare a global variable to hold the mutex lock for the server socket
pthread_mutex_t server_socket_mutex = PTHREAD_MUTEX_INITIALIZER;
//Declare a gloabl socket address struct to hold the address of the server
struct sockaddr_in server_address;
/*
################################################
##############Server Functions##################
################################################
*/

/**********************************************
 * init
   - port is the number of the port you want the server to be
     started on
   - initializes the connection acception/handling system
   - if init encounters any errors, it will call exit().
************************************************/
void init(int port) {
    int socket_fd;
    struct sockaddr_in local_server_address;

    //create a socket and save the file descriptor to sd (declared above)
    socket_fd = socket(PF_INET, SOCK_STREAM, 0); // See 4/11 Lec slides for arg options
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Change the socket options to be reusable using setsockopt(). 
    int enable = 1; // See "Avoiding Port Collision" 4/11 slide
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the socket to the provided port.
    local_server_address.sin_family = AF_INET;                 // IPv4
    local_server_address.sin_addr.s_addr = htonl(INADDR_ANY);  // Accept connections on any available interface
    local_server_address.sin_port = htons((short)port);               // Server port
    //printf("stuff: %d, %d\n", socket_fd, sizeof(local_server_address));

    if (bind(socket_fd, (struct sockaddr *)&local_server_address, sizeof(local_server_address)) == -1) {
        perror("bind");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Mark the socket as a pasive socket. (ie: a socket that will be used to receive connections)
    if (listen(socket_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
   
   
   
    // We save the file descriptor to a global variable so that we can use it in accept_connection().
    // Save the file descriptor to the global variable master_fd
    master_fd = socket_fd;
    printf("UTILS.O: Server Started on Port %d\n",port);
    fflush(stdout);

}


/**********************************************
 * accept_connection - takes no parameters
   - returns a file descriptor for further request processing.
   - if the return value is negative, the thread calling
     accept_connection must should ignore request.
***********************************************/
int accept_connection(void) {

    //create a sockaddr_in struct to hold the address of the new connection
    struct sockaddr_in new_connection;
    socklen_t addr_len = sizeof(new_connection); // Length of the struct

    // Aquire the mutex lock
    pthread_mutex_lock(&server_socket_mutex);

    // Accept a new connection on the passive socket and save the fd to newsock
    int new_sock = accept(master_fd, (struct sockaddr *)&new_connection, &addr_len);
    if (new_sock == -1) {
        perror("accept");
    }

    // Release the mutex lock
    pthread_mutex_unlock(&server_socket_mutex);

    // Return the file descriptor for the new client connection
    return new_sock;
}


/**********************************************
 * send_file_to_client
   - socket is the file descriptor for the socket
   - buffer is the file data you want to send
   - size is the size of the file you want to send
   - returns 0 on success, -1 on failure 
************************************************/
int send_file_to_client(int socket, char * buffer, int size) 
{
    //create a packet_t to hold the packet data
    packet_t packet_data;
    packet_data.size = htonl(size);

    //send the file size packet
    if (send(socket, &packet_data.size, sizeof(int), 0) == -1) {
        perror("send file size");
        return -1;
    }

    //send the file data
    int total_sent = 0;
    total_sent = send(socket, buffer, size, 0);
    int x = size - total_sent;
    if(total_sent == -1){
        perror("send");
        return -1;
    }
    else if(x <= 0){
        return 0;
    }
    else{
        return -1;
    }
}


/**********************************************
 * get_request_server
   - fd is the file descriptor for the socket
   - filelength is a pointer to a size_t variable that will be set to the length of the file
   - returns a pointer to the file data
************************************************/
char * get_request_server(int fd, size_t *filelength)
{
    //create a packet_t to hold the packet data
    packet_t data;
    memset(&data.size, 0, sizeof(data.size));
    //receive the response packet
    if (recv(fd, &data.size, sizeof(int), 0) == -1) {
        perror("receive response packet");
        exit(1);
    }

    //get the size of the image from the packet
    size_t s = ntohl(data.size);
    *filelength = s;
    //recieve the file data and save into a buffer variable.
    char* buffer = malloc(s + 1);
    memset(buffer, 0, s + 1);
    if(recv(fd, buffer, s, 0) == -1){
        perror("failed to receive file data");
        exit(1);
    }

    //return the buffer
    return buffer;
}


/*
################################################
##############Client Functions##################
################################################
*/

/**********************************************
 * setup_connection
   - port is the number of the port you want the client to connect to
   - initializes the connection to the server
   - if setup_connection encounters any errors, it will call exit().
************************************************/
int setup_connection(int port)
{
    //create a sockaddr_in struct to hold the address of the server   
    struct sockaddr_in serv_add;

    //create a socket and save the file descriptor to sockfd
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //assign IP, PORT to the sockaddr_in struct
    serv_add.sin_family = AF_INET;
    serv_add.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_add.sin_port = htons(port);

    //connect to the server
    socklen_t addr_len = sizeof(serv_add);
    int c = connect(sockfd, (struct sockaddr *) &serv_add, addr_len);
    if(c != 0){
        perror("server connection failed");
        exit(1);
    }
   
    //return the file descriptor for the socket
    printf("conneciton successful");
    return sockfd;
}


/**********************************************
 * send_file_to_server
   - socket is the file descriptor for the socket
   - file is the file pointer to the file you want to send
   - size is the size of the file you want to send
   - returns 0 on success, -1 on failure
************************************************/
int send_file_to_server(int socket, FILE *file, int size) 
{
    char buf[1032];
    //send the file size packet
    int s = htonl(size);
    if (send(socket, &s, sizeof(int), 0) == -1) {
        perror("send file size");
        return -1;
    }

    //send the file data
    int n;
    int total_sent = 0;
    //this while loop ensures everything is read from the file, no matter the size
    while(1){
        n = fread(buf, 1, 260, file);
        if(!n){
            break;
        }
        total_sent = send(socket, buf, n, 0);
        size -= total_sent;
        if(total_sent == -1){
            perror("send fail");
            return -1;
        }
    }
    if(size <= 0){
        return 0;
    }
    else{
        perror("failed to send full image");
        return -1;
    }

    //return 0 on success, -1 on failure
}

/**********************************************
 * receive_file_from_server
   - socket is the file descriptor for the socket
   - filename is the name of the file you want to save
   - returns 0 on success, -1 on failure
************************************************/
int receive_file_from_server(int socket, const char *filename) 
{
    //create a buffer to hold the file data
    char buffer[1024]; // picked an arbitrary size ig

    //open the file for writing binary data
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("open file");
        return -1;
    }
    
    //create a packet_t to hold the packet data
    packet_t data;
    //receive the response packet
    if(recv(socket, &data.size, sizeof(int), 0) == -1){
        perror("bad recv");
        return -1;
    }

    //get the size of the image from the packet
    int s = ntohl(data.size);
   

    //recieve the file data and write it to the file
    int x = recv(socket, buffer, s, 0);
    if(x == -1){
        perror("bad data receive");
        fclose(file);
        return -1;
    }
    if(fwrite(buffer, 1, s, file) < s){
        perror("failed to write to file");
        fclose(file);
        return -1;
    }
    //return 0 on success, -1 on failure
    fclose(file);
    return 0;
}

