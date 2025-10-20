#include <cstdio>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include "utils.h"
#include "client.h"
#include "topic_match.h"

void tcp_connection(int tcp_sfd, std::unordered_map<std::string, struct client> &clients_by_id,
                    std::unordered_map<int, std::string> &sockfd_to_id,
                    std::unordered_map<std::string, struct client> &disconnect_clients,
                    pollfd fds[], int &nfds)
{
    struct sockaddr_in client{};
    socklen_t client_len = sizeof(client);
    int client_socket = accept(tcp_sfd, (struct sockaddr *) &client, &client_len);
    int opt = 1;
    // Disable Nagle's algorithm for new socket
    int r = setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(int));
    DIE(r == -1, "Could not set socket options.");

    char client_id[ID_LEN];
    memset(client_id, 0, ID_LEN);

    r = recv(client_socket, client_id, ID_LEN, 0);
    DIE(r < 0, "Could not receive client ID.");

    std::string cl_id(client_id);

    // Client is already connected
    if (clients_by_id.find(cl_id) != clients_by_id.end()) {
        printf("Client %s already connected.\n", cl_id.c_str());
        r = close(client_socket);
        DIE(r == -1, "Could not close socket");
    } else {
        // New connection
        // Add client socket to poll
        fds[nfds].fd = client_socket;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;

        // Old client. Restore subscriptions
        if (disconnect_clients.find(cl_id) 
                != disconnect_clients.end()) {
            clients_by_id[cl_id] = disconnect_clients[cl_id];
            sockfd_to_id[client_socket] = cl_id;
        } else {
                // New client
                struct client new_client;
                new_client.id = cl_id;
                new_client.fd = client_socket;
            
                // Add client to maps
                clients_by_id[cl_id] = new_client;
                sockfd_to_id[client_socket] = cl_id;
        }
        printf("New client %s connected from %s:%hu\n",
             cl_id.c_str(), inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    }
}

void udp_message(std::unordered_map<std::string, struct client> &clients_by_id,
                std::unordered_map<int, std::string> &sockfd_to_id, pollfd fds[],
                int udp_sfd, int nfds)
{
    struct sockaddr_in udp_addr;
    socklen_t udp_len = sizeof(struct sockaddr_in);

    struct notification notif;
    memset(&notif, 0, sizeof(notification));

    char buf[1551];
    memset(buf, 0, sizeof(buf));

    // Receive message from UDP connection
    int r = recvfrom(udp_sfd, buf, sizeof(buf), 0, 
                (struct sockaddr *) &udp_addr, &udp_len);
    DIE(r < 0, "Unable to recv UDP packet.");

    struct udp_packet *received = (struct udp_packet*) buf;

    // Construct message for clients
    memset(notif.ip, 0, sizeof(notif.ip));
    strcpy(notif.ip, inet_ntoa(udp_addr.sin_addr));
    notif.port = ntohs(udp_addr.sin_port);

    memset(notif.packet.topic, 0, 50);
    strcpy(notif.packet.topic, received->topic);
    notif.packet.type = received->type;

    memset(notif.packet.payload, 0, sizeof(notif.packet.payload));
    memcpy(notif.packet.payload, received->payload, sizeof(received->payload));

    std::string topic(received->topic);
    // Iterate through connected clients and send notification if it matches subscription
    for (int i = 3; i < nfds; ++i) {
        struct client *cli = &clients_by_id[sockfd_to_id[fds[i].fd]];
        if (match_client_subscription(cli, topic)) {
            r = send(cli->fd, &notif, sizeof(notif), 0);
            DIE(r < 0, "Failed to send notification to client.");
        }
    }
}


int main(int argc, char *argv[])
{   
    int udp_sfd, tcp_sfd;
    struct sockaddr_in server;
    int opt = 1, nfds = 0;
    struct pollfd fds[MAX_CLIENTS];
    uint16_t port;

    // Stores clients by their ID
    std::unordered_map<std::string, struct client> clients_by_id;
    // Stores client's IDs by their socket
    std::unordered_map<int, std::string>sockfd_to_id;
    // Remembers disconnected clients (used at reconnecting)
    std::unordered_map<std::string, struct client> disconnect_clients;
    
    DIE(argc != 2, "Usage: ./server <PORT>");

    // Disable buffering
    int r = setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(r == -1, "Could not disable buffering.");

    r = sscanf(argv[1], "%hu" ,&port);
    DIE(r != 1, "Invalid port");

    // Open TCP socket
    tcp_sfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sfd < 0, "TCP socket failed");

    // Open UDP socket
    udp_sfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sfd < 0, "UPD socket failed");

    // Disable Nagle's algorithm
    r = setsockopt(tcp_sfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    DIE(r == -1, "Could not disable Nagle's algorithm.");

    memset(&server, 0, sizeof(server));
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    r = bind(tcp_sfd, (struct sockaddr *) &server, sizeof(server));
    DIE(r < 0 , "TCP bind failed");
    
    // Listen for new connections
    r = listen(tcp_sfd, MAX_CLIENTS);
    DIE(r < 0, "Could not listen for clients.");

    r = bind(udp_sfd,(struct sockaddr *) &server, sizeof(server));
    DIE(r < 0 , "UDP bind failed");

    fds[nfds].fd = STDIN_FILENO;
    fds[nfds++].events = POLLIN;

    fds[nfds].fd = tcp_sfd;
    fds[nfds++].events = POLLIN;

    fds[nfds].fd = udp_sfd;
    fds[nfds++].events = POLLIN;

    while (true) {

        // Wait for updates from the open sockets
        r = poll(fds, nfds, -1);
        DIE(r < 0, "Poll failed.");

        if (fds[0].revents & POLLIN) {
            // Update form STDIN
            char buf[2000];
            fgets(buf, 2000, stdin);
            char copy[2000];
            memset(copy, 0, sizeof(copy));
            memcpy(copy, buf, sizeof(buf));
            int count = 0;
            for (char *p = strtok(copy, " \t\n"); p; p = strtok(NULL, " \t\n"))
                count++;

            if (strncmp(buf, "exit", 4) == 0) {
                if (count != 1) {
                    printf("Usage: <exit>\n");
                } else {
                    // close server + disconnect clients
                    for (auto& client_pair : clients_by_id) {
                        close(client_pair.second.fd);
                    }
                    r = close(tcp_sfd);
                    DIE(r == -1, "Could not close socket");
                    r = close(udp_sfd);
                    DIE(r == -1, "Could not close socket");
                    return 0;
                }
                
            }
            printf("Unknown command.\n");
        }
        if (fds[1].revents & POLLIN) {
            // Update from tcp_sfd (new connection)
            tcp_connection(tcp_sfd, clients_by_id, sockfd_to_id,
                             disconnect_clients, fds, nfds);
        
        }
        if (fds[2].revents & POLLIN) {
            // UDP update
            udp_message(clients_by_id, sockfd_to_id, fds, udp_sfd, nfds);
        }

        // Message from connected clients
        for (int i = 3; i < nfds; ++i) {
            if (fds[i].revents & POLLIN) {
    
                struct sub_unsub message;
                struct client *cli = &clients_by_id[sockfd_to_id[fds[i].fd]];

                int r = recv(fds[i].fd, &message, sizeof(message), 0);
                DIE(r < 0, "Cannot receive un/subscription.");

                // Disconnect client
                if (message.type == 0) {
                    std::string cid = sockfd_to_id[fds[i].fd];

                    struct client old_client;
                    old_client.fd = cli->fd;
                    old_client.id = cli->id;
                    old_client.topics = cli->topics;
                    disconnect_clients[cid] = old_client;

                    clients_by_id.erase(cid);
                    sockfd_to_id.erase(fds[i].fd);
                    r = close(fds[i].fd);
                    DIE(r == -1, "Could not close socket");

                    for (int j = i; j < nfds - 1; ++j) {
                        fds[j] = fds[j + 1];
                    }
                    nfds--;
                    i--;
                    printf("Client %s disconnected.\n", cid.c_str());
                    
                    continue;
                }

                // Subscribe to topic
                if (message.type == 1) {
                    std::string topic(message.topic);
                    cli->topics.insert(topic);
                }

                // Unsubscribe from topic
                if (message.type == 2) {
                    std::string topic(message.topic);
                    cli->topics.erase(topic);
                }
            }
        }
    }
    return 0;
}
