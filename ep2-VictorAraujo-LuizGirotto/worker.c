#include "worker.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <stdbool.h>


#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096


int
worker() {
    FILE *fd;
    char buffer[MAXLINE];
    char work_number[MAXLINE];
    char ip[INET_ADDRSTRLEN];


    bool work_left = true;
    bool work_done = true;

    work_args *arg = malloc( sizeof( work_args));
    if (arg == NULL) {
        fprintf( stderr, "ERROR: Could not allocate memory\n");
        exit( EXIT_FAILURE);
    }
    pthread_mutex_t *work_done_mutex = malloc( sizeof( pthread_mutex_t));
    if (work_done_mutex == NULL) {
        fprintf( stderr, "ERROR: Could not allocate memory\n");
        exit( EXIT_FAILURE);
    }
    pthread_t *thread = malloc ( sizeof( pthread_t));
    if (thread == NULL) {
        fprintf( stderr, "ERROR: Could not allocate memory\n");
        exit( EXIT_FAILURE);
    }

    if (pthread_mutex_init( work_done_mutex, NULL)) {
        fprintf( stderr, "ERROR: Could not initialize mutex\n");
        exit( EXIT_FAILURE);
    }

    arg->work_done_mutex = work_done_mutex;
    arg->work_done = &work_done;

    /*Só roubei do EP01*/
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    char recvline[MAXLINE + 1];
    ssize_t n;

    if ((listenfd = socket( AF_INET, SOCK_STREAM, 0) == -1)) {
        fprintf( stderr, "ERROR: could not create socket, %s\n", strerror( errno));
        exit( EXIT_FAILURE);
    }

    bzero( &servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(WORKER_PORT);

    if ((bind( listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)) {
        fprintf( stderr, "ERROR: Could not bind socket, %s\n", strerror( errno));
        exit( EXIT_FAILURE);
    }

    if (listen( listenfd, LISTENQ) == -1) {
        fprintf( stderr, "ERROR: Could not listen on port, %s\n", strerror( errno));
        exit( EXIT_FAILURE);
    }
    /*Fim do roubo*/

    while (work_left) {
        if ((connfd = accept( listenfd, (struct sockaddr *) NULL, NULL) == -1)) {
            fprintf(stderr, "ERROR: Could not accept connection, %s\n", strerror( errno));
            continue;
        }
        n = read( connfd, recvline, MAXLINE);
        recvline[n] = 0;
        if (!strncmp( recvline, "001\r\n", 5 * sizeof( char))) {
            printf("SOU O LIDER PORRA\n");
        }
        else if (!strncmp( recvline, "102\r\n", 5 * sizeof( char))) {
            pthread_mutex_lock( work_done_mutex);
            if (work_done) {
                write( connfd, "200\r\n", 5 * sizeof( char));
                read( connfd, buffer, MAXLINE);
                arg->work_number = atoi(buffer);

                strncpy( buffer, "splitIn", MAXLINE * sizeof( char));
                snprintf( work_number, MAXLINE * sizeof( char), "%d", arg->work_number);
                strncat( buffer, work_number, MAXLINE * sizeof( char));
                strncat( buffer, ".txt", MAXLINE * sizeof( char));

                if ((fd = fopen( buffer, "w")) == NULL) {
                    fprintf(stderr, "ERROR: Could not open file, %s\n", strerror( errno));
                    write( connfd, "211\r\n", 5 * sizeof( char));
                    exit( EXIT_FAILURE);
                }
                write( connfd, "200\r\n", 5 * sizeof( char));
                while ((read( connfd, buffer, MAXLINE)) > 0) {
                    fprintf( fd, "%s", buffer);
                }
                fclose( fd);
                if (pthread_create( thread, NULL, work, arg)) {
                    fprintf(stderr, "ERROR: Could not create thread\n");
                }
            }
            else {
                write( connfd, "210\r\n", 5 * sizeof( char));
            }
            pthread_mutex_unlock( work_done_mutex);
        }
        else if (!strncmp( recvline, "003\r\n", 5 * sizeof( char))) {
            getIP( ip, sizeof( ip));
            write( connfd, "203\r\n", 5 * sizeof( char));
            write( connfd, ip, sizeof( ip));
        }
        close( connfd);
    }

    pthread_mutex_destroy( work_done_mutex);
    exit( EXIT_SUCCESS);
}


void *
work(void *args) {
    work_args *arg = (work_args *) args;
    char work_number[1000];
    char in[1000];
    char out[1000];

    pthread_mutex_lock( arg->work_done_mutex);
    *(arg->work_done) = false;
    pthread_mutex_unlock( arg->work_done_mutex);

    strncpy( in, "splitIn", 1000 * sizeof( char));
    strncpy( out, "splitOut", 1000 * sizeof( char));
    snprintf( work_number, 10000 * sizeof( char), "%d", arg->work_number);
    strncat( in, work_number, 1000 * sizeof( char));
    strncat( out, work_number, 1000 * sizeof( char));
    strncat( in, ".txt", 1000 * sizeof( char));
    strncat( out, ".txt", 1000 * sizeof( char));

    orderFile( in, out);

    /*Mandar pro lider o trabalho*/
    int sockfd, n;
    char recvline[MAXLINE + 1];
    struct sockaddr_in servaddr;

    if ((sockfd = socket( AF_INET, SOCK_STREAM, 0)) < 0)

    bzero( &servaddr, sizeof( servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons( IMMORTAL_PORT);

    /* Get immortal ip from config */
    //inet_pton( AF_INET, argv[1], &servaddr.sin_addr);

    connect( sockfd, (struct sockaddr *) &servaddr, sizeof( servaddr));
    /* Send work to immortal */
    write( sockfd, "212\r\n", 5 * sizeof( char));
    n = read( sockfd, recvline, MAXLINE);
    recvline[n] = 0;
    /* Can receive either 000 or 011. Retry if 011 */
    while (!strncmp( recvline, "011\r\n", 5 * sizeof( char))) {
        printf("Esperando imortal estar disponivel...\n");
        n = read( sockfd, recvline, MAXLINE);
        recvline[n] = 0;
    }
    FILE *fd;
    char *big_buffer;
    struct stat *file_mdata = malloc( sizeof( struct stat));
    fd = fopen( out, "r");
    stat( out, file_mdata);
    big_buffer = malloc( file_mdata->st_size);
    fread( big_buffer, 1, file_mdata->st_size, fd);
    write( sockfd, big_buffer, strlen( big_buffer));
    free( big_buffer);
    free( file_mdata);
    fclose( fd);

    close( sockfd);

    pthread_mutex_lock( arg->work_done_mutex);
    *(arg->work_done) = true;
    pthread_mutex_unlock( arg->work_done_mutex);
    return NULL;
}

int main() {
    printf( "Hello world\n");
    return 0;
}