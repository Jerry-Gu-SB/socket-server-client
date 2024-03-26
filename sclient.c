#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include "macro.h"
/*--------------------------------------------------------------------------------*/
int NUM_OF_CONNECTIONS_ALLOWED = 10;

int
main(const int argc, const char **argv) {
    const char *pserver = NULL;
    int port = -1;
    int i;

    /* argument processing */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && (i + 1) < argc) {
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-s") == 0 && (i + 1) < argc) {
            pserver = argv[i + 1];
            i++;
        }
    }

    /* check arguments */
    if (port < 0 || pserver == NULL) {
        printf("usage: %s -p port -s server-ip\n", argv[0]);
        exit(-1);
    }
    if (port < 1024 || port > 65535) {
        printf("port number should be between 1024 ~ 65535.\n");
        exit(-1);
    }


    signal(SIGPIPE, SIG_IGN);
    struct addrinfo hints;
    struct addrinfo *res;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if ((status = getaddrinfo(pserver, port_str, &hints, &res)) < 0) {
        fprintf(stderr, "Error ocurred in getaddrinfo: %s\n", gai_strerror(status));
        exit(-1);
    }
    // // get the addr info
    // int status;
    // struct addrinfo hints;
    // struct addrinfo *res;  // will point to the results
    //
    // memset(&hints, 0, sizeof hints); // make sure the struct is empty
    // hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    // hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    //
    // char port_argument[6];
    // snprintf (port_argument, sizeof(port_argument), "%d",port);

    // printf("connecting to IP %s on port %s\n", pserver, port_argument);  // TEST CODE
    // if ((status = getaddrinfo("localhost", port_argument, &hints, &res)) != 0) {
    //     fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    //     exit(-1);
    // }


    // create the socket
    const int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    char *std_in_file_content = (char *) calloc(MAX_CONT, sizeof(char));

    int read_char;
    int buffer_index = 0;
    while ((read_char = fgetc(stdin)) != EOF) {
        std_in_file_content[buffer_index++] = read_char;
        if (buffer_index >= MAX_CONT - 1) {
            // check if buffer is full
            printf("Input file is too big!");
            exit(-1);
        }
    }
    std_in_file_content[buffer_index] = '\0'; // null terminate

    // build response to server
    char *message_to_server = (char *) calloc(MAX_HDR + MAX_CONT, sizeof(char));

    if (snprintf(message_to_server, MAX_HDR + MAX_CONT,
                 "POST message SIMPLE/1.0\r\n"
                 "Host: %s\r\n"
                 "Content-length: %d\r\n\r\n"
                 "%s\r\n", pserver, buffer_index, std_in_file_content) < 0) {
        fprintf(stderr, "ERROR: snprintf message to send to server failed");
    }


    const int yes = 1; // ripped from beej
    //char yes='1'; // Solaris people use this
    // lose the pesky "Address already in use" error message
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // connect
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        fprintf(stderr, "ERROR: connect failed\n");
        exit((-1));
    }
    printf("CONNECTION ESTABLISHED.......\n");

    // send the message
    const int sent_bytes = send(sockfd, message_to_server, strlen(message_to_server), 0);
    if (sent_bytes <= 0) {
        fprintf(stderr, "ERROR: sending failed");
        exit((-1));
    }
    printf("MESSAGE SENT! sent %d bytes\n", sent_bytes);


    char *header_and_content_from_server = (char *) calloc(MAX_CONT + MAX_HDR, sizeof(char));

    if (recv(sockfd, header_and_content_from_server, MAX_HDR + MAX_CONT, 0) < 0) {
        fprintf(stderr, "ERROR: receiving failed with errno %d: %s\n", errno, strerror(errno));
        exit(-1);
    }


    // printf("header_and_content_from_server: %s\n",header_and_content_from_server);
    const char *message_content_from_server = strstr(header_and_content_from_server, "\r\n\r\n") + 4; // +4 to chop off the double new line

    // printf("message content from server: %s\n", message_content_from_server);

    if (message_content_from_server == NULL) {
        fprintf(stderr, "ERROR: no content in message received from server\n");
        exit(-1);
    }
    char *message_content_copy = strdup(message_content_from_server);
    if (message_content_copy == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        exit(-1);
    }

    char *line = strtok(header_and_content_from_server, "\r\n");
    char *content_length_string = (char *) calloc(MAX_HDR, sizeof(char));
    int content_length_received;

    int iteration = 0;
    int post_flag = FALSE, content_length_flag = FALSE;
    int header_is_bad_request = TRUE;
    int header_parsed_flag = FALSE;
    while (line != NULL) {
        printf("current line: %s, iteration %d\n", line, iteration);
        switch (iteration) {
            case 0:
                if (strlen(line) == 0) {
                    fprintf(stderr, "ERROR: empty header!");
                    exit(-1);
                }

                const char *strstr_200 = strstr(line, "200");
                const char *strstr_400 = strstr(line, "400");
                const char *strstr_OK = strstr(line, "OK");
                const char *strstr_Bad_Request = strstr(line, "Bad Request");
            // printf("second iteration second word: %s\n", post_line);

                if (!((strstr_200 != NULL && strstr_OK) ^ (strstr_400 != NULL && strstr_Bad_Request))) {
                    fprintf(stderr, "ERROR: Header formatted incorrectly. exiting....\n");
                    header_is_bad_request = TRUE;
                    break;
                }
                if (strstr_200 > strstr_OK || strstr_400 > strstr_Bad_Request) {
                    fprintf(stderr, "ERROR: Header elements ordered incorrectly. Exiting...");
                    header_is_bad_request = TRUE;
                    break;
                }
            // we assume that the header is a bad request unless we have 200 OK to tell us otherwise
                if (strstr_200 != NULL && strstr_OK != NULL && (strstr_200 < strstr_OK)) {
                    header_is_bad_request = FALSE;
                }

                printf("LINE 1 PASS!----------\n");
                post_flag = TRUE;
                break;

            case 1: // looking for content
                printf("header is bad request: %d\n", header_is_bad_request);
                if (header_is_bad_request) break;
                if (strlen(line) == 0) {
                    fprintf(stderr, "ERROR: missing second header line!");
                    header_is_bad_request = TRUE;
                }
                for (int j = 0; j < 15; j++) {
                    // verify content-length tag
                    const char *content_length_string_lower = "content-length:";
                    if (tolower(line[j]) != content_length_string_lower[j]) {
                        fprintf(stderr, "ERROR: \"content-length:\" string not found!\n");
                        header_is_bad_request = TRUE;
                    }
                }
                int cur_index = 0;
                for (int j = 15; j < strlen(line); j++) {
                    // grab the content length
                    if (!isspace(line[j])) {
                        if (!isdigit(line[j])) {
                            fprintf(stderr, "Content length must be a base 10 number!\n");
                            header_is_bad_request = TRUE;
                        }
                        content_length_string[cur_index] = line[j];
                        cur_index++;
                    }
                }
                content_length_received = atoi(content_length_string);
                if (content_length_received == 0 && content_length_string[0] != '0') {
                    fprintf(stderr, "ERROR: content-length received not a number\n");
                    exit(-1);
                }
                printf("LINE 2 PASS!--------- content length: %d\n", content_length_received);
                content_length_flag = TRUE;

                header_parsed_flag = TRUE;
                printf("HEADER PARSED! ------\n");
                break;

            default:
                fprintf(stderr, "Oops! You weren't supposed to find me :] exiting...\n");
                exit(-1);
        }
        if (header_parsed_flag)
            break;  // ensures strtok doesn't consume the \r\n needed to strstr the content
        line = strtok(NULL, "\r\n");
        iteration++;
    } // parse header

    // check if we have all pieces of the header
    if (!(post_flag && content_length_flag)) {
        fprintf(stderr, "ERROR: missing header parts! Writing entire response header.....\n");
        header_is_bad_request = TRUE;
    }

    if (header_is_bad_request) {
        // just write the header
        printf("Header is a bad request! Printing just the content\n");
        write(1, header_and_content_from_server, strlen(header_and_content_from_server));
    } else {
        // just write the messsage
        printf("Valid request. Here is your message: \n");
        write(1, message_content_copy, strlen(message_content_copy));
    }

    // send the packet

    // wait and receive a response

    // write to stdout based on what you get

    freeaddrinfo(res); // free the linked-list

    free(content_length_string);
    free(std_in_file_content);
    free(message_content_copy);
    close(sockfd);
}


//     POST message SIMPLE/1.0
//     Host: [s-server-domain-name]
//     Content-length: [byte-count]
//     (empty line)
//     [message-content]

// so long as "Host" and "Content-length:" exist then it's well formed. no order enforced.
// each line must end with '\r' then '\n'
// s-server is the domain name (www.nytimes.com)
// can be "localhost" or address like "143.247.3.1"
// no need to validate this so long as there is a string there

// byte count is the number of bytes of input message
// content length ends with '\r\n\r\n'

// request header lengths are case insensitive (Host: == hOsT:
// there can be any number of whitespace (checked by isspace())before and after a value like servername and byte content
// there can be any number of whitespaces around,
// “message”, “SIMPLE/1.0” (but no whitespaces before “POST”, “Host:” and “Content-length”)

// bind the server


// if (listen(sockfd, NUM_OF_CONNECTIONS_ALLOWED) < 0) {
//     // fprintf(stderr, "ERROR: listening failed\n");
//     exit(-1);
// }
// printf("LISTENING ON PORT %d\n", port);
//
// // accept
// if (accept(sockfd, (struct sockaddr *) &their_addr, &addr_size) < 0) {
//     fprintf(stderr, "ERROR: accepting failed\n");
//     exit(-1);
// }
// printf("CONNECTION ACCEPTED------------------\n");

// parsing the strings splitted

// start iterations at 0. for 0th iteration, check for post
// for 1,2 iterations, check for host/content
// for 3rd iteration, check for empty line


// receive the message and then print to std out depending on status

// generate the header string
// then send that jit

// parse the received string


// then generate this format. note that you'll have to read the message before to get this information

// read in string to 1 buffer
//  server, and then the FILE with the message. also you're parsing
//  the REPONSE from the client. so the parsing needs to be in the
//  server file LOL. finish the parsing here for now.

// SERVER fork logic: while(cur = listen()..) basically keep taking
// stuff from the queue if it's there.
// fork()
//      if parent, just do nothing and go to the next iteration
//          can just do a if(!parent) continue
//          if pid < 0 then it's an error i believe.
//      if child, do the parsing and sending work
