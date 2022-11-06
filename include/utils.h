#ifndef UTILS_H
#define UTILS_H
#include <string>
#include <vector>

namespace SocketUtils {
int set_nonblocking(int fd);
int listen_on(int port, int backlog);
int accept_socket(int sockfd, std::string &client_ip, int &port);
void split_str(const std::string &str, char split_char, std::vector<std::string> &output);
ssize_t readn(int fd, char *buf, size_t num);
ssize_t writen(int fd, const char *buf, size_t num);
}  // namespace SocketUtils
#endif