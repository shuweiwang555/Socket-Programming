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
#include <vector>

#define UPORT "24493" // UDP port
#define LOCALPORT "23493"
#define HOST "localhost"
#define SERVER_NAME "C"
#define FILE_NAME "block3.txt"

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

struct Transaction {
    int serialNum, amount;
    std::string sender, receiver;
    Transaction() {}
    Transaction(int num, std::string sender, std::string receiver, int amount)
        : serialNum(num), sender(sender), receiver(receiver), amount(amount) {}
};

std::vector<Transaction> trans;

void readData() {
    freopen(FILE_NAME, "r", stdin);
    const int BUFFER_SIZE = 256;
    char s[BUFFER_SIZE], r[BUFFER_SIZE];
    int id, amount;
    while (~scanf("%d%s%s%d", &id, s, r, &amount))
        trans.emplace_back(id, s, r, amount);
}

std::string getTransactions() {
    std::stringstream ret;
    ret << std::to_string(trans.size()) << "\n";
    for (auto it : trans)
        ret << it.serialNum << " " << it.sender << " " << it.receiver << " "
            << it.amount << "\n";
    return ret.str();
}

int main() {
    readData();
    printf("The Server%s is up and running using UDP on port %s.\n",
           SERVER_NAME, LOCALPORT);

    UDPServer server;
    UDPClient client;
    server.init(LOCALPORT);
    client.init("localhost", UPORT);

    while (1) {
        std::string msg = server.recv();
        printf("The Server%s received a request from the Main Server.\n",
               SERVER_NAME);
        client.send(getTransactions());
        printf(
            "The Server%s finished sending the response to the Main Server.\n",
            SERVER_NAME);
    }

    return 0;
}
