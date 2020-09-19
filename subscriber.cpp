#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

int main(int argc, char *argv[]) {
  int sockfd,n,ret,fdmax;
  struct sockaddr_in serv_addr;
  fd_set read_fds,tmp_fds;
  char buffer[BUFLEN];
  // check number of arguments
  if (argc < 4) {
    fprintf(stderr, "Usage: %s <ID_CLIENT> <IP_Server> <Port_Server>\n",
            argv[0]);
    return 0;
  }

  // check id value
  if (strlen(argv[1]) > 10) {
    fprintf(stderr, "ID trebuie sa fie maxim 10 caractere \n");
    return 0;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[3]));
  ret = inet_aton(argv[2], &serv_addr.sin_addr);
  DIE(ret == 0, "inet_aton");
  
 // initialize tcp socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");
  
  ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(ret < 0, "connect");
  
  // empty fds
  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);
  
  FD_SET(STDIN_FILENO, &read_fds);
  FD_SET(sockfd, &read_fds);
  fdmax = sockfd;
  // sending client id
  n = send(sockfd, argv[1], strlen(argv[1]), 0);
  DIE(n < 0, "send");

  while (1) {
    tmp_fds = read_fds;
    ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
    DIE(ret < 0, "select");

    if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
      // read from keyboard
      memset(buffer, 0, BUFLEN);
      fgets(buffer, BUFLEN - 1, stdin);

      // if exit we disconnect
      if (strncmp(buffer, "exit", 4) == 0) {
        ret = send(sockfd, buffer, sizeof(buffer), 0);
        DIE(ret < 0, "exit");
        close(sockfd);
        return 0;
      } else if (strncmp(buffer, "subscribe", 9) == 0) {
        char *tok = strtok(buffer, " ");
        string maker = "s.", SF, topic;
        int k = 0;
        while (tok != NULL) {
          k++;
          if (k == 2) {
            topic = tok;
          }
          if (k == 3) {
            SF = tok;
          }
          tok = strtok(NULL, " ");
        }
        maker = maker + SF + "." + topic;

        if (k != 3 || topic.length() > 51 || (SF[0] != '1' && SF[0] != '0')) {
          cout << "Subscribe usage: subscribe topic SF " << endl;
        } else {
          ret = send(sockfd, maker.c_str(), maker.length(), 0);
          DIE(ret < 0, "subs");
        }
      } else if (strncmp(buffer, "unsubscribe", 11) == 0) {
        char *tok = strtok(buffer, " ");
        string maker = "u.", SF, topic;
        int k = 0;
        while (tok != NULL) {
          k++;
          if (k == 2) topic = tok;
          tok = strtok(NULL, " ");
        }
        maker = maker + topic;

        if (k != 2 || topic.length() > 51) {
          cout << "Unsubscribe usage: unsubscribe topic" << endl;
        } else {
          ret = send(sockfd, maker.c_str(), maker.length(), 0);
          DIE(ret < 0, "unsubs");
        }
      }
    }

    if (FD_ISSET(sockfd, &tmp_fds)) {
      memset(buffer, 0, BUFLEN);
      n = recv(sockfd, buffer, sizeof(buffer), 0);
      // messages received from server
      DIE(n < 0, "recv");
      if (!strncmp(buffer, "exit", 4)) {
        cout << "client closed" << endl;
        close(sockfd);
        return 0;
      } else if (strncmp(buffer, "unsubscribed", 12) == 0 ||
                 (strncmp(buffer, "subscribe", 9) == 0)) {
        cout << buffer << endl;
      } else if (!strncmp(buffer, "m.", 2)) {
        cout << buffer + 2 << endl;
      }
    }
  }
  close(sockfd);  // closing socket
  return 0;
}
