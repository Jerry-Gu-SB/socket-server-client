#include <stdio.h>
#include <errno.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>

#include "macro.h"
/*--------------------------------------------------------------------------------*/
int CONNECTIONS_ALLOWED = 10;

int send_bad_request(const int sockfd_sock) {
    const char *bad_request_string = "SIMPLE/1.0 400 Bad Request\r\n\r\n";
    if (send(sockfd_sock, bad_request_string, strlen(bad_request_string), 0) == -1) {
        fprintf(stderr, "ERROR: send failed...\n");
        exit(-1);
    }
    return 1;
}

int
main(const int argc, const char **argv) {
    int i;
    int port = -1;

    /* argument parsing  */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && (i + 1) < argc) {
            port = atoi(argv[i + 1]);
            i++;
        }
    }
    if (port <= 0 || port > 65535) {
        printf("usage: %s -p port\n", argv[0]);
        exit(-1);
    }

    // implement your own code
    signal(SIGPIPE, SIG_IGN);


    int status;
    struct addrinfo hints;
    struct addrinfo *res; // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    char port_argument[6];
    snprintf(port_argument, sizeof(port_argument), "%d", port);

    if ((status = getaddrinfo(NULL, port_argument, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(-1);
    }

    const int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    const int yes = 1; // ripped from beej
    //char yes='1'; // Solaris people use this
    // lose the pesky "Address already in use" error message
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        exit(-1);
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        fprintf(stderr, "ERROR: bind failed.\n");
        exit(-1);
    }
    printf("CLIENT SOCKET BOUND!----------------\n");


    if (listen(sockfd, CONNECTIONS_ALLOWED) == -1) {
        fprintf(stderr, "ERROR: listen failed\n");
        exit((-1));
    }
    printf("LISTENING... getpid(): %d\n", getpid());

    int listening_file_descriptor;
    while ((listening_file_descriptor = accept(sockfd, NULL, NULL)) >= 0) {
        printf("LISTENING ON PORT %d\n", port);

        const int forked_pid = fork();
        if (forked_pid < 0) {
            fprintf(stderr, "ERROR: fork call failed! Exiting...\n");
            exit(-1);
        }
        if (forked_pid > 0) {
            continue;
        }
        printf("current pid: %d, current fd: %d\n", getpid(), listening_file_descriptor);

        // receive the message and then print to std out depending on status
        char *header_and_content_from_client = (char *) calloc(MAX_CONT + MAX_HDR, sizeof(char));

        if (recv(listening_file_descriptor, header_and_content_from_client, MAX_HDR + MAX_CONT, 0) == -1) {
            fprintf(stderr, "ERROR: receiving failed...\n");
            exit(-1);
        }

        // printf("content received: %s\n", header_and_content_from_client);
        // parsing the strings splitted

        // start iterations at 0. for 0th iteration, check for post
        // for 1,2 iterations, check for host/content
        // for 3rd iteration, check for empty line

        char *message_content_from_client = strstr(header_and_content_from_client, "\r\n\r\n");
        // printf("MESSAGE_CONTENT_FROM_CLIENT: %s\n", message_content_from_client);
        if (message_content_from_client == NULL) {
            fprintf(stderr, "ERROR: no content in message received from server");
            exit(-1);
        }
        char *message_content_copy = strdup(message_content_from_client);
        if (message_content_copy == NULL) {
            fprintf(stderr, "ERROR: Memory allocation failed\n");
            exit(-1);
        }

        char *line = strtok(header_and_content_from_client, "\r\n");
        char *s_server_domain_name = (char *) calloc(100, sizeof(char));
        char *content_length_string = (char *) calloc(10, sizeof(char));

        int content_length_received = -1;
        int iteration = 0;
        int host_first = FALSE;
        int post_flag = FALSE, host_flag = FALSE, content_length_flag = FALSE;
        int header_parsed_flag = FALSE;
        while (line != NULL) {
            // printf("current line: %s, iteration %d\n", line, iteration);
            switch (iteration) {
                case 0: // looking for POST
                    if (strlen(line) == 0) {
                        fprintf(stderr, "ERROR: empty header!");
                        send_bad_request(listening_file_descriptor);
                    }
                // printf("first iteration first word: %s\n", post_line);
                    if (strstr(line, "POST") == NULL) {
                        fprintf(stderr, "ERROR: \"POST\" not found!\n");
                        send_bad_request(listening_file_descriptor);
                    }
                // printf("second iteration second word: %s\n", post_line);
                    if (strstr(line, "message") == NULL) {
                        fprintf(stderr, "ERROR: \"message\" not found!\n");
                        send_bad_request(listening_file_descriptor);
                    }
                    if (strstr(line, "SIMPLE/1.0") == NULL) {
                        fprintf(stderr, "ERROR: \"SIMPLE/1.0\" not found!\n");
                        send_bad_request(listening_file_descriptor);
                    }
                    printf("LINE 1 PASS!----------\n");
                    post_flag = TRUE;
                    break;
                case 1: // looking for host or content
                    if (strlen(line) == 0) {
                        fprintf(stderr, "ERROR: missing second header line!");
                        send_bad_request(listening_file_descriptor);
                    }
                    if (tolower(line[0]) == 'h') {
                        host_first = TRUE; // host_first is set by default to FALSE
                    }
                    if (host_first) {
                        printf("HOST WAS FIRST\n");
                        for (int j = 0; j < 5; j++) {
                            // verify the host tag
                            const char *host_string_lower = "host:";
                            if (tolower(line[j]) != host_string_lower[j]) {
                                fprintf(stderr, "ERROR: \"host:\" string not found!\n");
                                send_bad_request(listening_file_descriptor);
                            }
                        }
                        int cur_index = 0;
                        for (int j = 5; j < strlen(line); j++) {
                            // grab the host value
                            if (!isspace(line[j])) {
                                s_server_domain_name[cur_index] = line[j];
                                cur_index++;
                            }
                        }
                    } else {
                        for (int j = 0; j < 15; j++) {
                            // verify content-length tag
                            const char *content_length_string_lower = "content-length:";
                            if (tolower(line[j]) != content_length_string_lower[j]) {
                                fprintf(stderr, "ERROR: \"content-length:\" string not found!\n");
                                send_bad_request(listening_file_descriptor);
                            }
                        }
                        int cur_index = 0;
                        for (int j = 15; j < strlen(line); j++) {
                            // grab the content length
                            if (!isspace(line[j])) {
                                if (!isdigit(line[j])) {
                                    fprintf(stderr, "Content length must be a base 10 number!\n");
                                    send_bad_request(listening_file_descriptor);
                                }
                                content_length_string[cur_index] = line[j];
                                cur_index++;
                            }
                        }
                        content_length_received = atoi(content_length_string);
                        if (content_length_received == -1) {
                            fprintf(stderr, "ERROR: atoi failed somehow idk. Exiting..");
                            send_bad_request(listening_file_descriptor);
                        }
                        if (content_length_received == 0 && content_length_string[0] != '0') {
                            fprintf(stderr, "ERROR: content-length received not a number\n");
                            send_bad_request(listening_file_descriptor);;
                        }
                    }
                    printf("LINE 2 PASS!---------");
                    if (host_first) {
                        host_flag = TRUE;
                        printf("host found: %s\n", s_server_domain_name);
                    } else {
                        content_length_flag = TRUE;
                        printf("content-length found: %d\n", content_length_received);
                    }
                    break;
                case 2:
                    if (strlen(line) == 0) {
                        fprintf(stderr, "ERROR: missing third header line!");
                        send_bad_request(listening_file_descriptor);;
                    }
                    if (!host_first) {
                        printf("HOST WAS FIRST\n");
                        for (int j = 0; j < 5; j++) {
                            // verify the host tag
                            const char *host_string_lower = "host:";
                            if (tolower(line[j]) != host_string_lower[j]) {
                                fprintf(stderr, "ERROR: \"host:\" string not found!\n");
                                send_bad_request(listening_file_descriptor);;
                            }
                        }
                        int cur_index = 0;
                        for (int j = 5; j < strlen(line); j++) {
                            // grab the host value
                            if (!isspace(line[j])) {
                                s_server_domain_name[cur_index] = line[j];
                                cur_index++;
                            }
                        }
                    } else {
                        for (int j = 0; j < 15; j++) {
                            // verify content-length tag
                            const char *content_length_string_lower = "content-length:";
                            if (tolower(line[j]) != content_length_string_lower[j]) {
                                fprintf(stderr, "ERROR: \"content-length:\" string not found!\n");
                                send_bad_request(listening_file_descriptor);
                            }
                        }

                        int cur_index = 0;
                        for (int j = 15; j < strlen(line); j++) {
                            // grab the content length
                            if (!isspace(line[j])) {
                                content_length_string[cur_index] = line[j];
                                cur_index++;
                            }
                        }
                        content_length_received = atoi(content_length_string);
                        if (content_length_received == 0 && content_length_string[0] != '0') {
                            fprintf(stderr, "ERROR: content-length received not a number\n");
                            send_bad_request(listening_file_descriptor);
                        }
                    }
                    printf("LINE 3 PASS!---------");
                    if (!host_first) {
                        host_flag = TRUE;
                        printf("host found: %s\n", s_server_domain_name);
                    } else {
                        content_length_flag = TRUE;
                        printf("content-length found: %d\n", content_length_received);
                    }
                    header_parsed_flag = TRUE;
                    printf("HEADER PARSED!\n");
                    break;
                default:
                    fprintf(stderr, "Oops! You weren't supposed to find me O__o\n");
                    exit(-1);
            }
            if (header_parsed_flag)
                break;
            line = strtok(NULL, "\r\n");
            iteration++;
        }
        // printf("message content from server after parsing: %s\n",message_content_copy);
        // check if we have all pieces of the header
        if (!(post_flag && host_flag && content_length_flag)) {
            fprintf(stderr, "ERROR: missing header parts!\n");
        }

        // build message to client
        char *message_to_client = (char *) calloc(MAX_HDR + MAX_CONT, sizeof(char));
        if (snprintf(message_to_client, MAX_HDR + MAX_CONT,
                     "POST 200 OK\r\n"
                     "Content-length: %d\r\n\r\n"
                     "%s\r\n", content_length_received, message_content_copy) < 0) {
            fprintf(stderr, "ERROR: snprintf message to send to server failed\n");
        }

        if (send(listening_file_descriptor, message_to_client, strlen(message_to_client), 0) == -1) {
            fprintf(stderr, "ERROR: send failed...\n");
            exit(-1);
        }
        printf("RESPONSE SENT TO CLIENT!---------------------\n");
        free(s_server_domain_name);
        free(content_length_string);
        free(message_to_client);
        free(header_and_content_from_client);
        free(message_content_copy);
        close(listening_file_descriptor);
    }
    close(sockfd);
    freeaddrinfo(res);
}
