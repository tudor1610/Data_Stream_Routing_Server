#include <cmath>
#include <cstdio>
#include <iterator>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/tcp.h>
#include "client.h"

void handle_server_update(int socketfd)
{
    // Disable buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    struct notification *notif;
    char buff[1570];
    memset(buff, 0, sizeof(buff));

    int r = recv(socketfd, buff, sizeof(buff), 0);
    DIE(r < 0,  "Could  not read server message.");

    notif = (struct notification *)buff;

    // Server is closing
    if (r == 0) {
        exit(0);
    }

    // Print received message
    if (notif->packet.type == INT) {
        
        uint32_t value = ntohl(*((uint32_t *)(notif->packet.payload + 1)));
        if (notif->packet.payload[0] == 1)
            value = (int) (-1 * value);

        printf("%s:%hu - %s - INT - %d\n",
                notif->ip, notif->port, notif->packet.topic, value);

    } else if (notif->packet.type == SHORT_REAL) {

        double value = ntohs(*(uint16_t *)notif->packet.payload);
        value /= 100;
        printf("%s:%hu - %s - SHORT_REAL - %.2lf\n",
                    notif->ip, notif->port, notif->packet.topic, value);

    } else if (notif->packet.type == FLOAT) {

        double value = ntohl(*(uint32_t*)(notif->packet.payload + 1));
        uint8_t power = (uint8_t) notif->packet.payload[5];
        value /= pow(10, power);
        if (notif->packet.payload[0] == 1) {
            value *= -1;
        }
        printf("%s:%hu - %s - FLOAT - %lf\n",
                notif->ip, notif->port, notif->packet.topic, value);

    } else if (notif->packet.type == STRING) {
        printf("%s:%hu - %s - STRING - %s\n",
                notif->ip, notif->port, notif->packet.topic, notif->packet.payload);
    }
}

int main(int argc, char *argv[])
{
    uint16_t port;
    int socketfd;
    struct sockaddr_in server_addr;
    char client_id[11];
    char server_ip[17];
    struct pollfd fds[2];
    int opt = 1;

    DIE(argc != 4, "Usage ./subscriber <CLIENT_ID> <IP_SERVER> <PORT_SERVER>.");
    DIE(strlen(argv[1]) > 10, " Invalid ID.");
    
    strncpy(client_id, argv[1], sizeof(client_id) - 1);
    client_id[sizeof(client_id) - 1] = '\0';
    strncpy(server_ip, argv[2], sizeof(server_ip) - 1);
    server_ip[sizeof(server_ip) - 1] = '\0';
    port = atoi(argv[3]);

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = socketfd;
    fds[1].events = POLLIN;

    // Disable Nagle's algorithm
    int r = setsockopt(socketfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    DIE(r == -1, "Could not disable Nagle's algorithm.");
    // Disable buffering
    r = setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(r == -1, "Could not disable buffering");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(server_ip, &server_addr.sin_addr);

    r = connect(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    DIE(r < 0, "Could not connect.");

    r = send(socketfd, client_id, 10, 0);
    DIE(r < 0, "Could not send message.");

    while (true) {

        // Wait for updates
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            // STDIN update
            // input_stdin(socketfd);
            char buffer[1601]; 
            memset(&buffer, 0, 1601);

            fgets(buffer, 1600, stdin);
            char copy[1601];
            memset(copy, 0, sizeof(copy));
            memcpy(copy, buffer, sizeof(buffer));
            int count = 0;
            for (char *p = strtok(copy, " \t\n"); p; p = strtok(NULL, " \t\n"))
                count++;
            

            // Disconnect
            if (strncmp(buffer, "exit", 4) == 0) {

                if (count != 1) {
                    printf("Usage: <exit>\n");
                } else {
                    struct sub_unsub message;
                    message.type = 0;
    
                    int r = send(socketfd, &message, sizeof(struct sub_unsub), 0);
                    DIE(r < 0, "Error sending subscription.");
    
                    break;
                }
            } else if (strncmp(buffer, "subscribe", 9) == 0) {
                 // Subscribe to topic
                if (count != 2) {
                    printf("Usage: subscribe <topic>\n");
                } else {
                    // Construct message
                    struct sub_unsub message;
                    char *topic = strchr(buffer, ' ') + 1;
                    topic[strlen(topic) - 1] = '\0'; 
                    message.type = 1;
                    strcpy(message.topic, topic);

                    r = send(socketfd, &message, sizeof(struct sub_unsub), 0);
                    DIE(r < 0, "Error sending subscription.");

                    // std::cout << "Subscribed to topic " << topic << "\n";
                    printf("Subscribed to topic %s\n", topic);
                    continue;
                }
                
            }else if (strncmp (buffer, "unsubscribe", 11) == 0) {

                if (count != 2) {
                    printf("Usage: unsubscribe <topic>\n");
                } else {
                    // COnstruct message
                    struct sub_unsub message;
                    char *topic = strchr(buffer, ' ') + 1;
                    topic[strlen(topic) - 1] = '\0'; 
                    message.type = 2;
                    strcpy(message.topic, topic);

                    r = send(socketfd, &message, sizeof(struct sub_unsub), 0);
                    DIE(r < 0, "Error sending unsubscription.");

                    printf("Unsubscribed from topic %s\n", topic);
                    continue;
                }
                
            } else {
                printf("Unknown command.\n");
            }


        } else if ((fds[1].revents & POLLIN) != 0) {
            // Update from server
            handle_server_update(socketfd);
        }
    }

    r = close(socketfd);
    DIE(r == -1, "Could not close socket");
    return 0;
}
