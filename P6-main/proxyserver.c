#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "proxyserver.h"
#include "safequeue.h"

/*
 * Global configuration variables.
 * Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int num_listener;
int *listener_ports;
int num_workers;
char *fileserver_ipaddr;
int fileserver_port;
int max_queue_size;

void send_error_response(int client_fd, status_code_t err_code, char *err_msg) {
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    http_send_string(client_fd, buf);
    return;
}

/*
 * forward the client request to the fileserver and
 * forward the fileserver response to the client
 */
void serve_request(int client_fd) {

    // create a fileserver socket
    int fileserver_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fileserver_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        exit(errno);
    }

    // create the full fileserver address
    struct sockaddr_in fileserver_address;
    fileserver_address.sin_addr.s_addr = inet_addr(fileserver_ipaddr);
    fileserver_address.sin_family = AF_INET;
    fileserver_address.sin_port = htons(fileserver_port);

    // connect to the fileserver
    int connection_status = connect(fileserver_fd, (struct sockaddr *)&fileserver_address,
                                    sizeof(fileserver_address));
    if (connection_status < 0) {
        // failed to connect to the fileserver
        printf("Failed to connect to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");
        return;
    }

    char *buffer = (char*)malloc(RESPONSE_BUFSIZE * sizeof(char));

    int bytes_read = read(client_fd, buffer, RESPONSE_BUFSIZE);
    int ret = http_send_data(fileserver_fd, buffer, bytes_read);

    if (ret < 0) {
        printf("Failed to send request to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");

    } else {
        // forward the fileserver response to the client
        while (1) {
            int bytes_read = recv(fileserver_fd, buffer, RESPONSE_BUFSIZE - 1, 0);
            if (bytes_read <= 0) // fileserver_fd has been closed, break
                break;
            ret = http_send_data(client_fd, buffer, bytes_read);
            if (ret < 0) { // write failed, client_fd has been closed
                break;
            }
        }
    }

    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);

    // Free resources and exit
    free(buffer);
}

// start the listener thread helper func
void start_listener(listener_info *d) {
    int *server_fd=d->server_fd;
    int listener_port=d->listener_port;

    // create a socket to listen
    *server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (*server_fd == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    // manipulate options for the socket
    int socket_option = 1;
    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }

    // create the full address of this proxyserver
    struct sockaddr_in proxy_address;
    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = INADDR_ANY;
    //not using proxy_port use listener _port directly
    proxy_address.sin_port = htons(listener_port); // listening port

    // bind the socket to the address and port number specified in
    if (bind(*server_fd, (struct sockaddr *)&proxy_address,
             sizeof(proxy_address)) == -1) {
        perror("Failed to bind on socket");
        exit(errno);
    }

    // starts waiting for the client to request a connection
    if (listen(*server_fd, 1024) == -1) {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", listener_port);

    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_fd;
    while (1) {
        safequeueItem_t *item;
        client_fd = accept(*server_fd,
                           (struct sockaddr *)&client_address,
                           (socklen_t *)&client_address_length);
        if (client_fd < 0) {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);

        // get the priortity
        parsed_http_request_t *request=parse_client_request(client_fd);

        if (strcmp(request->path, "/GetJob") == 0) {
            //check if is get job if not use nonblocking
            item = get_work_nonblocking();

            if (!item) {
                send_error_response(client_fd, QUEUE_EMPTY, "Empty queue");
                shutdown(client_fd, SHUT_WR);
                close(client_fd);
            } else {
                char *buf = malloc(strlen(item->path) + 2);
                sprintf(buf, "%s\n", item->path);

                http_start_response(client_fd, OK);
                http_send_header(client_fd, "Content-Type", "text/html");
                http_end_headers(client_fd);
                http_send_string(client_fd, buf);
                shutdown(client_fd, SHUT_WR);
                close(client_fd);
            }
        } else {
            int priority;
            
            if (sscanf(request->path, "/%d/", &priority) != 1) {
                send_error_response(client_fd, BAD_REQUEST, "fail to extract priority");
                shutdown(client_fd, SHUT_WR);
                close(client_fd);
            }

            // if get priority successful go add_work
            if (add_work(priority, request->path, request->delay, client_fd) < 0) {
                send_error_response(client_fd, QUEUE_FULL, "Queue is full");
                shutdown(client_fd, SHUT_WR);
                close(client_fd);
            }
        }
    }
}
//get worker started call get_work
void start_worker() {
    while(1) {
        safequeueItem_t *job = get_work();

        sleep(job->delay);

        serve_request(job->client_fd);

        // shutdown the connection to the client
        shutdown(job->client_fd, SHUT_WR);
        close(job->client_fd);

        free(job);
    }
}

int *server_fds;
//helper func
void start_listnerthreads(int *server_fds, pthread_t *listeners){
// start listnerthreads
    for (int i = 0; i < num_listener; i++) {
        listener_info *d = malloc(sizeof(listener_info));
        d->listener_port = listener_ports[i];
        d->server_fd = &server_fds[i];
        pthread_create(&listeners[i], 0, (void*)start_listener, d);
    }
}

/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */

void serve_forever(int *server_fds) {
    // make threads
    pthread_t *listeners = (pthread_t *)malloc(num_listener * sizeof(pthread_t));
    if (!listeners) {
        printf("Failed to allocate memory\n");
        exit(1);
    }

    pthread_t *workers = (pthread_t *)malloc(num_workers * sizeof(pthread_t));
    if (!workers) {
        printf("Failed to allocate memory\n");
        exit(1);
    }

    start_listenerthreads(server_fds,listeners);
    
    //start worker thread
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&workers[i], 0, (void*)start_worker, NULL);
    }

    // join listener threads
    for (int i = 0; i < num_listener; i++) {
        pthread_join(listeners[i], NULL);
    }
    //join worker thread
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }
    // shutdown the connection to the client
    for (int i = 0; i < num_listener; i++) {
        shutdown(server_fds[i], SHUT_RDWR);
        close(server_fds[i]);
    }

    free(listeners);
    free(workers);
}

/*
 * Default settings for in the global configuration variables
 */
void default_settings() {
    num_listener = 1;
    listener_ports = (int *)malloc(num_listener * sizeof(int));
    listener_ports[0] = 8000;

    num_workers = 1;

    fileserver_ipaddr = "127.0.0.1";
    fileserver_port = 3333;

    max_queue_size = 100;
}

void print_settings() {
    printf("\t---- Setting ----\n");
    printf("\t%d listeners [", num_listener);
    for (int i = 0; i < num_listener; i++)
        printf(" %d", listener_ports[i]);
    printf(" ]\n");
    printf("\t%d workers\n", num_listener);
    printf("\tfileserver ipaddr %s port %d\n", fileserver_ipaddr, fileserver_port);
    printf("\tmax queue size  %d\n", max_queue_size);
    printf("\t  ----\t----\t\n");
}

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    for (int i = 0; i < num_listener; i++) {
        if (close(server_fds[i]) < 0) perror("Failed to close server_fd (ignoring)\n");
    }
    free(listener_ports);
    dequeue();
    exit(0);
}

char *USAGE =
    "Usage: ./proxyserver [-l 1 8000] [-n 1] [-i 127.0.0.1 -p 3333] [-q 100]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);

    /* Default settings */
    default_settings();

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp("-l", argv[i]) == 0) {
            num_listener = atoi(argv[++i]);
            free(listener_ports);
            listener_ports = (int *)malloc(num_listener * sizeof(int));
            for (int j = 0; j < num_listener; j++) {
                listener_ports[j] = atoi(argv[++i]);
            }
        } else if (strcmp("-w", argv[i]) == 0) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp("-q", argv[i]) == 0) {
            max_queue_size = atoi(argv[++i]);
        } else if (strcmp("-i", argv[i]) == 0) {
            fileserver_ipaddr = argv[++i];
        } else if (strcmp("-p", argv[i]) == 0) {
            fileserver_port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }
    print_settings();

    create_queue(max_queue_size);

    server_fds = malloc(sizeof(int) * num_listener);
    serve_forever(server_fds);
    
    free(server_fds);

    return EXIT_SUCCESS;
}
