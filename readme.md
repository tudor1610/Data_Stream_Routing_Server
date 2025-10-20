<small>Brandibur Tudor 322CA, 2024-2025</small>

# Client-Server application for TCP and UDP messages

## Introduction
The implementation provides a simple client-server messaging system.
It allows multiple TCP clients (subscribers) to connect to a central
server, where they can subscribe or unsubscribe from variuos topics.
Meanwhile, the UDP clinets send messages (updates) for certain topics.
The server ensures that the subscribers are notified depending on their
subscriptions.

## 1. Server
The server opens a TCP socket on which it listens for new subscribers
and an UDP one for messages from publishers. Using `poll()` the server
awaits for updates from the open sockets or from STDIN. Depending on
the socket the update comes form the server can:

- Read input from STDIN
- Manage a new TCP connection using the function `tcp_connection()`
- Handle a UDP message using the function `udp_message()`
- Receive updates from the allready connected clients

#### 1.1 STDIN
There is only one available command from STDIN and that is "exit".
This command closes the server and all of the connected clints.

#### 1.2 TCP connection
When the server receives an update on the `tcp_sfd` socket, it checks
wheather the client that wants to subscribe is connected or not and,
if it is a new client, it opens a new socket for said client. The
server than adds the client to the `fds` poll so it can know to listen
for updates from the client. It also stores the client into the
`clients_by_id` map so it can access it when needed.

#### 1.3 UDP messages
The UDP update comes in the form of a `struct udp_packet`:

        struct udp_packet {
            char topic[50];
            uint8_t type; 
            unsigned char payload[1500];
        };
    
First, the server receives the message and the constructs the
notification for the clients:

        struct notification {
            char ip[16];                // the ip of the UDP client
            uint16_t port;              // the port from which the udp_packet came
            struct udp_packet packet;   // the upd_packet reccceived
        };

Then, the server proceeds to search through each of the
clients and, if the upd_packet topic matches on of the
topics a client is subscribed to, send them the notification.
To check the clients subscription, the implementation uses
the function `match_client_subscription()` which returns **true**
if at least one subscrioption matches or **false** if not.
The function, calls for every subscription the function `split_and_match()`.
It takes two strings and splits them into two arrays of strings.
Then the two arrays are compared recursively, taking into consideration
the * and + rules.

#### 1.4 Client updates
The server listens for client messages of 3 types:
- exit
- subscribe 
- unsubscribe

It keeps track of every subscriber and it's subscription (works on 
reconnection as well).


## 2. Subscriber
The subscriber implementation is quite similar with the server one,
but with some modifications regarding functionalities.
After omening a TCP socket for server side comunication, it sends
a connect request to the server. Once the connection is processed,
the client sends its ID to the server so that the server can validate
its existance and uniqueness. Then, the subscriber starts listening
for incomming messages from two sources:

- STDIN
- Server

#### 2.1 STDIN
The client can handle 3 types of STDIN commnads.

        exit
        subscribe <TOPIC>
        unsubscribe <TOPIC>

EXIT - sends a disconnection message to the server and then closes
its sockets and disconnects.

SUBSCRIBE - sends the server a message with the topic it wants to
receive notifications from.

UNSUBSCRIBE - sends the server a message indicating that it no longer
wants to receive notifications from a certain topic.

#### 2.2 Server updates
The server updates are handled by the `handle_server_update()` 
function and can mean 2 things:

- The server is closing
- A new UDP notification is available

If the server closes, the client closes all of its sockets and 
disconnects. Otherwise, it handles the received message and
prints the output according to the message type (INT, SHORT_REAL,
FLOAT, STRING).
