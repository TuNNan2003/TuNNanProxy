#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#define BUFFER_SIZE 4096

class message_send
{
public:
    message_send(std::string message_, std::string server_name_, std::string server_port_) : message(message_), server_name(server_name_), server_port(server_port_){};

    const std::string get_message()
    {
        return message;
    }

    const std::string get_server_name()
    {
        return server_name;
    }

    const std::string get_server_port()
    {
        return server_port;
    }

private:
    std::string message;
    std::string server_name;
    std::string server_port;
};

const char *ip = "localhost";
const int port = 10240;

message_send *get_user(char *localmessage);
int communicate_server(message_send *send_message, int user_socket);
int send_user(int user_socket, char *buffer, int *read_index);

int main(void)
{

    int listenfd, connfd;

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    if (ret == -1)
    {
        std::cout << "bind false" << std::endl;
        close(listenfd);
        return -1;
    }

    ret = listen(listenfd, 5);
    std::cout << "listening" << std::endl;
    assert(ret != -1);

    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    while (1)
    {
        connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            std::cout << "errno is : " << errno
                      << strerror(errno) << std::endl;
        }

        int flags = fcntl(connfd, F_GETFL, 0);
        if (flags == -1)
        {
            perror("Error getting socket flag");
            close(connfd);
            continue;
        }

        if (fcntl(connfd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            perror("Error setting socket to non-block mode");
            close(connfd);
            continue;
        }

        else
        {
            char buffer[BUFFER_SIZE];
            memset(buffer, '\0', BUFFER_SIZE);
            int data_read = 0;
            int read_index = 0;
            while (1)
            {
                data_read = recv(connfd, buffer + read_index, BUFFER_SIZE - read_index, 0);
                if (data_read < 0 && errno != EWOULDBLOCK)
                {
                    std::cout << "reading failed" << std::endl;
                    break;
                }
                else if (data_read == 0)
                {
                    std::cout << "User: remote client has closed the connection" << std::endl;
                    break;
                }
                else if (data_read < 0 && errno == EWOULDBLOCK)
                {
                    continue;
                }
                std::cout << "receive message from user" << std::endl
                          << buffer + read_index << std::endl;

                message_send *send_message = get_user(buffer + read_index);

                read_index += data_read;
                int ret = communicate_server(send_message, connfd);
                if (ret == -1)
                {
                    break;
                }
                else if (ret == 1)
                {
                    break;
                }
                memset(buffer, '\0', BUFFER_SIZE);
                read_index = 0;
            }
            close(connfd);
        }
    }
    close(listenfd);
    return 0;
}

message_send *get_user(char *localmessage)
{
    int method_end = strstr(localmessage, " ") - localmessage;
    int name_end = strstr(localmessage + method_end + 8, "/") - localmessage;
    char *method = (char *)malloc(sizeof(char) * method_end);
    char *name = (char *)malloc(sizeof(char) * (name_end - method_end - 1));
    memcpy(method, localmessage, method_end);
    memcpy(name, localmessage + method_end + 8, name_end - method_end - 8);
    std::cout << "method is: " << method << std::endl;
    std::cout << "name is: " << name << std::endl;

    int connection = strstr(localmessage, "Connection: keep-alive") - localmessage;
    char *message = (char *)malloc(sizeof(char) * connection);
    memcpy(message, localmessage, connection);
    std::string message_head = std::string(message);
    std::string message_tail = std::string("Connection: close\r\n\r\n");
    std::cout << message_head + message_tail;

    message_send *send_message = new message_send(message_head + message_tail, std::string(name), std::string("80"));
    free(method);
    free(name);
    return send_message;
}

int communicate_server(message_send *send_message, int user_socket)
{
    std::cout << "ready to send message to server" << std::endl;

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;

    struct hostent *result = gethostbyname(send_message->get_server_name().c_str());
    inet_pton(AF_INET, inet_ntoa(*(struct in_addr *)result->h_addr_list[0]), &address.sin_addr);

    int send_port = 80;
    address.sin_port = htons(send_port);

    int port = std::stoi(send_message->get_server_port());

    int connfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(connfd >= 0);

    if (connfd < 0)
    {
        std::cout << "errno is : " << errno
                  << strerror(errno) << std::endl;
    }

    int ret = (int)connect(connfd, (struct sockaddr *)&address, sizeof(address));
    if (ret == -1)
    {
        std::cout << errno << std::endl;
        close(connfd);
        return -1;
    }

    int flags = fcntl(connfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("Error getting socket flag");
        close(connfd);
        return -1;
    }
    if (fcntl(connfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("Error setting socket to non-block mode");
        close(connfd);
        return -1;
    }

    const char *writebuffer = send_message->get_message().c_str();
    std::cout << send_message->get_message() << std::endl;
    ret = send(connfd, (void *)writebuffer, send_message->get_message().length(), 0);
    std::cout << "write buffer length:" << send_message->get_message().length() << std::endl;
    if (ret > 0)
    {
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        int data_read = 0;
        int read_index = 0;
        while (1)
        {
            data_read = recv(connfd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if (data_read < 0 && errno != EWOULDBLOCK)
            {
                std::cout << "reading failed" << std::endl;
                close(connfd);
                return -1;
            }
            else if (data_read == 0)
            {
                std::cout << "Communicate server: no data to read" << std::endl;
                close(connfd);
                return 1;
            }
            else if (data_read < 0 && errno == EWOULDBLOCK)
            {
                std::cout << "read no data" << std::endl;
                continue;
            }
            std::cout << "send message to user" << std::endl;
            int ret = send(user_socket, (void *)(buffer + read_index), data_read - read_index, 0);
            if (ret == -1)
            {
                std::cout << strerror(errno) << std::endl;
                continue;
            }
            std::cout << "send end" << std::endl;
            read_index += data_read;
            memset(buffer, '\0', BUFFER_SIZE);
            read_index = 0;
        }
    }
    close(connfd);
    return 0;
}