#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CAPORT "25493" // TCP port with clientA
#define CBPORT "26493" // TCP port with clientB
#define UPORT "24493"  // UDP port
#define SAPORT "21493"
#define SBPORT "22493"
#define SCPORT "23493"
#define HOST "localhost"

void sigchld_handler(int s) {
    (void)s; // quiet unused variable warning
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

class TCPServer {
  private:
    static const int BACKLOG = 10;
    static const int MAX_DATASIZE = 256;
    int sockfd, clientSockfd;
    char buffer[MAX_DATASIZE];

  public:
    std::string port;
    TCPServer() { sockfd = clientSockfd = 0; }
    bool init(std::string port) {
        this->port = port;

        addrinfo hints, *servinfo, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        int rv, yes = 1;

        if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return false;
        }
        // loop through all the results and bind to the first we can
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1) {
                perror("server: socket");
                continue;
            }

            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                           sizeof(int)) == -1) {
                perror("setsockopt");
                return false;
            }

            if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("server: bind");
                continue;
            }

            break;
        }

        freeaddrinfo(servinfo); // all done with this structure

        if (p == NULL) {
            fprintf(stderr, "server: failed to bind\n");
            return false;
        }

        if (::listen(sockfd, BACKLOG) == -1) {
            perror("listen");
            return false;
        }

        return true;
    }

    ~TCPServer() {
        if (sockfd)
            close(sockfd);
        if (clientSockfd)
            close(clientSockfd);
    }

    bool listen() {
        // struct sockaddr_storage their_addr;
        // socklen_t sin_size = sizeof(their_addr);
        clientSockfd = accept(sockfd, 0, 0);
        if (clientSockfd == -1) {
            sleep(1);
            perror("accept");
            return false;
        }
        return true;
    }

    void closeClient() {
        if (clientSockfd) {
            close(clientSockfd);
            clientSockfd = 0;
        }
    }

    int send(std::string msg) {
        return ::send(clientSockfd, msg.c_str(), msg.size() + 1, 0);
    }

    std::string recv() {
        if ((::recv(clientSockfd, buffer, MAX_DATASIZE - 1, 0)) == -1) {
            perror("recv");
            exit(1);
        }
        return std::string(buffer);
    }
};

class UDPServer {
  private:
    static const int MAX_DATASIZE = 256;
    int sockfd;
    char buffer[MAX_DATASIZE];

  public:
    bool init(std::string port) {
        struct addrinfo hints, *servinfo, *p;
        int rv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET; // set to AF_INET to use IPv4
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return false;
        }

        // loop through all the results and bind to the first we can
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1) {
                perror("listener: socket");
                continue;
            }

            if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("listener: bind");
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "listener: failed to bind socket\n");
            return false;
        }

        freeaddrinfo(servinfo);

        return true;
    }

    std::string recv() {
        struct sockaddr_storage their_addr;
        socklen_t numbytes = sizeof(their_addr);
        if ((numbytes = recvfrom(sockfd, buffer, MAX_DATASIZE - 1, 0,
                                 (struct sockaddr *)&their_addr, &numbytes)) ==
            -1) {
            perror("recvfrom");
            exit(1);
        }
        return std::string(buffer);
    }
};

class UDPClient {
  private:
    int sockfd;
    struct addrinfo *p;

  public:
    bool init(std::string ip, std::string port) {

        struct addrinfo hints, *servinfo;
        int rv;
        int numbytes;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET; // set to AF_INET to use IPv4
        hints.ai_socktype = SOCK_DGRAM;

        if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) !=
            0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return false;
        }

        // loop through all the results and make a socket
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1) {
                perror("talker: socket");
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "talker: failed to create socket\n");
            return false;
        }

        return true;
    }

    int send(std::string msg) {
        return sendto(sockfd, msg.c_str(), msg.size() + 1, 0, p->ai_addr,
                      p->ai_addrlen);
    }
};

TCPServer con;

void CheckWallet(std::string user) {
    con.send("1000");
    UDPClient client;
    UDPServer server;
    client.init("localhost", SAPORT);
    server.init(UPORT);
    client.send("GET");
    printf("got: %s\n", server.recv().c_str());
}

void TransCoins(std::string sender, std::string receiver, int amount) {}

int main() {
    printf("The main server is up and running.\n");

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to fork.\n");
        return 0;
    }
    if (pid == 0) {
        con.init(CAPORT);
    } else {
        con.init(CBPORT);
    }

    while (1) {
        if (!con.listen())
            continue;

        if (!fork()) {
            std::stringstream ss;
            std::string clientName, op;
            ss << con.recv();
            std::getline(ss, clientName, ',');
            std::getline(ss, op, ',');
            if (op == "CHECK WALLET") {
                std::string username;
                std::getline(ss, username);
                printf("The main server received input=%s from the client "
                       "using TCP "
                       "over port %s.\n",
                       username.c_str(), con.port.c_str());
                CheckWallet(username);
            } else if (op == "TXCOINS") {
                std::string sender, receiver, amount;
                std::getline(ss, sender, ',');
                std::getline(ss, receiver, ',');
                std::getline(ss, amount);
                TransCoins(sender, receiver, atoi(amount.c_str()));
            }
            exit(0);
        }

        con.closeClient();
    }

    return 0;
}