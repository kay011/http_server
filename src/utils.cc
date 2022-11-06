#include "utils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "easylogging++.h"
namespace SocketUtils {
int set_nonblocking(int fd) {
  int flags;

  if (-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static inline std::string &ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
  return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) { return ltrim(rtrim(s)); }

void split_str(const std::string &str, char split_char, std::vector<std::string> &output) {
  std::stringstream ss(str);
  while (ss.good()) {
    std::string temp;
    std::getline(ss, temp, split_char);
    output.push_back(trim(temp));
  }
}

ssize_t readn(int fd, char *buf, size_t num) {
  size_t nleft = num;
  ssize_t nread = 0;
  ssize_t readSum = 0;
  char *ptr = buf;
  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0;
      else if (errno == EAGAIN) {
        return readSum;
      } else {
        return -1;
      }
    } else if (nread == 0) {
      break;
    }
    readSum += nread;
    nleft -= nread;
    ptr += nread;
  }
  return readSum;
}

ssize_t writen(int fd, const char *buf, size_t num) {
  size_t nleft = num;
  ssize_t nwritten = 0;
  ssize_t writeSum = 0;
  const char *ptr = buf;
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0) {
        if (errno == EINTR) {
          nwritten = 0;
          continue;
        } else if (errno == EAGAIN) {
          return writeSum;
        } else {
          return -1;
        }
      }
    }
    writeSum += nwritten;
    nleft -= nwritten;
    ptr += nwritten;
  }
  return writeSum;
}

int listen_on(int port, int backlog) {
  int listen_fd;

  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    LOG(FATAL) << "listen_fd create err";
  }

  struct sockaddr_in my_addr; /* my address information */
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;         /* host byte order */
  my_addr.sin_port = htons(port);       /* short, network byte order */
  my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(listen_fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
    LOG(FATAL) << "listen_fd bind err";
  }

  if (listen(listen_fd, backlog) == -1) {
    LOG(FATAL) << "listen_fd listen err";
  }
  LOG(INFO) << "start Server Socket on port: " << port;
  return listen_fd;
}

int accept_socket(int listenfd, std::string &client_ip, int &port) {
  int new_fd;
  struct sockaddr_in client_addr; /* connector's address information */
  socklen_t sin_size = sizeof(struct sockaddr_in);

  if ((new_fd = accept(listenfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
    LOG(FATAL) << "accept listen err";
    return -1;
  }

  client_ip = inet_ntoa(client_addr.sin_addr);
  port = ntohs(client_addr.sin_port);
  LOG(INFO) << "server: got connection from " << client_ip.c_str();
  return new_fd;
}
}  // namespace SocketUtils