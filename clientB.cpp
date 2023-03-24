#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define CBPORT "26493" // TCP port with clientA

const std::string CLIENT_NAME = "B";

class TCPClient {
  private:
    static const int MAX_DATASIZE = 256;
    int sockfd;
    char buffer[MAX_DATASIZE];

    void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in *)sa)->sin_addr);
        }

        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }

  public:
    TCPClient(std::string ip, std::string port) {
        struct addrinfo hints, *servinfo, *p;
        int rv;
        char s[INET6_ADDRSTRLEN];

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) !=
            0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            exit(1);
        }

        // loop through all the results and connect to the first we can
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                perror("client: connect");
                close(sockfd);
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "client: failed to connect\n");
            exit(1);
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
                  sizeof s);
        // printf("client: connecting to %s\n", s);

        freeaddrinfo(servinfo); // all done with this structure
    }

    ~TCPClient() { close(sockfd); }

    int send(std::string msg) {
        return ::send(sockfd, msg.c_str(), msg.size() + 1, 0);
    }

    std::string recv() {
        if ((::recv(sockfd, buffer, MAX_DATASIZE - 1, 0)) == -1) {
            perror("recv");
            exit(1);
        }
        return std::string(buffer);
    }
};

struct Response {
    bool success;
    std::string msg;
    Response(std::string s) {
        if (s[0] == '1')
            success = true;
        else
            success = false;
        msg = s.substr(1);
    }
};

int main(int argc, char *argv[]) {
    TCPClient con = TCPClient("localhost", CAPORT);
    printf("The client B is up and running.\n");
    if (argc == 2) {
        if (strcmp(argv[1], "TXLIST") == 0) {
            // todo
        } else { // CHECK WALLET
            std::string username = std::string(argv[1]);
            printf("%s sent a balance enquiry request to the main server.\n",
                   username.c_str());
            con.send(CLIENT_NAME + ",CHECK WALLET," + username);
            Response res = Response(con.recv());
            if (res.success) {
                printf("The current balance of %s is :%s alicoins.\n",
                       username.c_str(), res.msg.c_str());
            } else {
                printf("%s is not part of the network.\n", username.c_str());
            }
        }
    } else if (argc == 4) { // TXCOINS
        std::string senderUsername = std::string(argv[1]);
        std::string receiverUsername = std::string(argv[2]);
        std::string amount = std::string(argv[3]);
        printf("%s has requested to transfer %s coins to %s.\n",
               senderUsername.c_str(), receiverUsername.c_str(),
               amount.c_str());
        con.send(CLIENT_NAME + ",TXCOINS," + senderUsername + "," +
                 receiverUsername + "," + amount);
    }
    return 0;
}