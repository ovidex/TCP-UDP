#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <iostream>
#include <iterator>
#include <map>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

int main(int argc, char *argv[]) {
  int sock_tcp, newsock_tcp, portno, sock_udp;
  char buffer[BUFLEN];
  char mesaj_deconectare[BUFLEN];
  char clienti_conectat[BUFLEN];
  char mesaj_new_user[BUFLEN];
  struct sockaddr_in serv_addr, cli_addr;
  int n, i, ret;
  socklen_t clilen;
  fd_set read_fds;
  fd_set tmp_fds;
  int fdmax;
  unordered_map<int, string> socketmap1;  // socket - id corelation
  unordered_map<string, int> socketmap2;  //  id - socket corelation
  unordered_map<string, bool> online;     // online/offline ids
  map<string, string> sending;

  struct abonat {
    bool sf;  // 1 / 0
    string id;
  };
  struct tdc {  // for messages
    char topic[50];
    int type;
    char content[1500];
  };
  multimap<string, abonat> subs;  // subscribers map

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <PORT_DORIT>\n", argv[0]);
    exit(0);
  }

  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);  // emptying fds
  FD_SET(STDIN_FILENO, &read_fds);

  sock_tcp = socket(AF_INET, SOCK_STREAM, 0);  // TCP
  DIE(sock_tcp < 0, "socket tcp");
  sock_udp = socket(PF_INET, SOCK_DGRAM, 0);  //  UDP
  DIE(sock_udp < 0, "socket udp");

  portno = atoi(argv[1]);
  DIE(portno == 0, "atoi");

  memset((char *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(portno);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  ret = bind(sock_tcp, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
  DIE(ret < 0, "bind tcp");

  ret = bind(sock_udp, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
  DIE(ret < 0, "bind udp");

  ret = listen(sock_tcp, MAX_CLIENTS);
  DIE(ret < 0, "listen tcp");

  FD_SET(sock_tcp, &read_fds);
  FD_SET(sock_udp, &read_fds);

  fdmax = (sock_tcp > sock_udp) ? sock_tcp : sock_udp;
  bool run = 1;
  while (run) {
    tmp_fds = read_fds;

    memset(buffer, 0, BUFLEN);
    ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
    DIE(ret < 0, "select");

    for (i = 0; i <= fdmax; i++) {  // iterate through sockets
      if (FD_ISSET(i, &tmp_fds)) {  // check used sox

        if (i == STDIN_FILENO) {
          // stdin received
          fgets(buffer, BUFLEN - 1, stdin);
          if (!strncmp(buffer, "exit", 4)) {  // exit message stops server
            run = 0;
            char cexit[100];
            strcpy(cexit, "exit");

            for (auto itr = online.begin(); itr != online.end(); itr++) {
              if (itr->second == 1) {
                n = send(socketmap2.find(itr->first)->second, cexit,
                         strlen(cexit), 0);
                DIE(n < 0, "cexit");
              }
            }

          } else {
            cout << "Singura comanda acceptata este >exit< ." << endl;
          }
        } else if (i == sock_tcp) {  // if tcp
          clilen = sizeof(cli_addr);
          newsock_tcp = accept(sock_tcp, (struct sockaddr *)&cli_addr, &clilen);
          DIE(newsock_tcp < 0, "accept");
          memset(buffer, 0, BUFLEN);
          n = recv(newsock_tcp, buffer, sizeof(buffer), 0);
          DIE(n < 0, "recv");

          FD_SET(newsock_tcp, &read_fds);

          if (newsock_tcp > fdmax) {
            fdmax = newsock_tcp;
          }
          if (socketmap2.count(buffer) == 0) {
            socketmap1.insert(make_pair(newsock_tcp, buffer));
            socketmap2.insert(
                make_pair(buffer, newsock_tcp));     // corelate id & socket
            online.insert(make_pair(buffer, true));  // ids online/offline

            cout << "New client "
                 << (string)socketmap1.find(newsock_tcp)->second.c_str()
                 << " connected from " << inet_ntoa(cli_addr.sin_addr) << ":"
                 << htons(cli_addr.sin_port) << endl;
          } else {
            socketmap1.erase(socketmap2.find(buffer)->second);
            socketmap2.erase(buffer);
            socketmap1.insert(make_pair(newsock_tcp, buffer));
            socketmap2.insert(
                make_pair(buffer, newsock_tcp));  // corelate id & socket
            online[buffer] = true;
            cout << "Welcome back" << endl;
            if (sending[buffer].length()) {
              ret = send(newsock_tcp, sending[buffer].c_str(),
                         sending[buffer].length(), 0);
              DIE(ret < 0, "welcome back...");
              sending[buffer].clear();
            }
          }

        } else if (i == sock_udp) {  // if udp

          memset(buffer, 0, BUFLEN);

          socklen_t socklen = (socklen_t)sizeof(serv_addr);
          n = recvfrom(i, buffer, BUFLEN, 0, (struct sockaddr *)&serv_addr,
                       &socklen);
          DIE(n < 0, "recv");
          tdc msg;
          memset(msg.topic, 0, 50);
          strncpy(msg.topic, buffer, 49);
          msg.type = buffer[50];
          memset(msg.content, 0, 1499);
          strncpy(msg.content, buffer + 51, 1499);

          switch (msg.type) {
            case 0: {  // INT DATA
              string s = string("m.") + inet_ntoa(serv_addr.sin_addr) +
                         string(":") + to_string(htons(serv_addr.sin_port)) +
                         " ";
              s += string("- ") + msg.topic + " - " + "INT - ";
              int aux;
              memcpy(&aux, buffer + 52, sizeof(uint32_t));
              int value = ((int)buffer[51] == 1)
                              ? ntohl(aux) * (-1)
                              : ntohl(aux);  // check sign bit
              s += to_string(value);

              for (auto it = subs.begin(); it != subs.end(); ++it)// sending the message
                if (it->first == msg.topic) {
                  if (online[it->second.id]) {
                    ret = send(socketmap2[it->second.id], s.c_str(), s.length(),
                               0);
                    DIE(ret < 0, "INT");
                  } else if (it->second.sf) {
                    if (sending[it->second.id].length() > 1) {
                      sending[it->second.id] += (s.substr(2) + "\n");

                    }

                    else
                      sending[it->second.id] += (s + "\n");
                  }
                }

            } break;
            case 1: {  // SHORT DATA
              uint16_t aux;
              string s = string("m.") + inet_ntoa(serv_addr.sin_addr) +
                         string(":") + to_string(htons(serv_addr.sin_port)) +
                         " ";
              s += string("- ") + msg.topic + " - " + "SHORT REAL - ";
              memcpy(&aux, buffer + 51, sizeof(uint16_t));
              s += to_string(htons(aux) / 100.00);

              for (auto it = subs.begin(); it != subs.end(); ++it)// sending the message
                if (it->first == msg.topic) {
                  if (online[it->second.id]) {
                    ret = send(socketmap2[it->second.id], s.c_str(), s.length(),
                               0);
                    DIE(ret < 0, "short");
                  } else if (it->second.sf) {
                    if (sending[it->second.id].length() > 1) {
                      sending[it->second.id] += (s.substr(2) + "\n");

                    }

                    else
                      sending[it->second.id] += (s + "\n");
                  }
                }
            } break;
            case 2: {  // FLOAT DATA
              uint32_t aux;
              uint8_t power;
              string s = string("m.") + inet_ntoa(serv_addr.sin_addr) +
                         string(":") + to_string(htons(serv_addr.sin_port)) +
                         " ";
              s += string("- ") + msg.topic + " - " + "FLOAT - ";
              memcpy(&aux, buffer + 52, sizeof(uint32_t));
              memcpy(&power, buffer + 52 + sizeof(uint32_t), sizeof(uint8_t));
              int p = pow(10, power);
              float val = ntohl(aux) * 1.00 / p;
              val = ((int)buffer[51] == 1) ? val * (-1) : val;
              s += to_string(val);

              for (auto it = subs.begin(); it != subs.end(); ++it) // sending the message
                if (it->first == msg.topic) {
                  if (online[it->second.id]) {
                    ret = send(socketmap2[it->second.id], s.c_str(), s.length(),
                               0);
                    DIE(ret < 0, "float");
                  } else if (it->second.sf) {
                    if (sending[it->second.id].length() > 1) {
                      sending[it->second.id] += (s.substr(2) + "\n");

                    }

                    else
                      sending[it->second.id] += (s + "\n");
                  }
                }
            } break;
            case 3: {  // STRING DATA
              string s = string("m.") + inet_ntoa(serv_addr.sin_addr) +
                         string(":") + to_string(htons(serv_addr.sin_port)) +
                         " ";
              s += string("- ") + msg.topic + " - " + "STRING - ";
              char aux[1500];
              memset(aux, 0, 1499);
              memcpy(aux, buffer + 51, 1500);
              s += string(aux);

              for (auto it = subs.begin(); it != subs.end(); ++it)// sending the message
                if (it->first == msg.topic) {
                  if (online[it->second.id]) {
                    ret = send(socketmap2[it->second.id], s.c_str(), s.length(),
                               0);
                    DIE(ret < 0, "STRING");
                  } else if (it->second.sf) {
                    if (sending[it->second.id].length() > 1) {
                      sending[it->second.id] += (s.substr(2) + "\n");

                    }

                    else
                      sending[it->second.id] += (s + "\n");
                  }
                }
            } break;
            default:
              cout << "WRONG DATA TYPE\n";
          }
        } else {
          memset(buffer, 0, BUFLEN);
          ret = recv(i, buffer, sizeof(buffer), 0);
          DIE(ret < 0, "rec fin");
          if (!strncmp(buffer, "exit", 4)) {  // exit
            cout << "Client " << socketmap1.find(i)->second.c_str()
                 << " disconnected." << endl;
            online[socketmap1.find(i)->second] = false;
          }
          if (!strncmp(buffer, "s.", 2)) {  // subscribe
            abonat aux;
            aux.sf = buffer[2] - '0';
            aux.id = socketmap1[i];
            cout << aux.id << " s-a abonat la " << buffer + 5 << endl;
            subs.insert(make_pair(buffer + 5, aux));

            for (auto it = subs.begin(); it != subs.end(); ++it) {
              if (it->first == buffer + 5) {
                char v[100];
                string ax = "subscribed ";
                ax = ax + (buffer + 5);
                ret = send(i, ax.c_str(), ax.length(), 0);
                DIE(ret < 0, "subscr");
              }
            }
          }
          if (!strncmp(buffer, "u.", 2)) {  // unsubscribe
            string id = socketmap1[i];
            cout << id << " s-a dezabonat de la " << buffer + 2 << endl;
            string aux = buffer + 2;
            for (auto it = subs.begin(); it != subs.end(); ++it) {
              if (!strncmp((it->first).c_str(), aux.c_str(),
                           aux.length() - 1) &&
                  id == (it->second).id)
                subs.erase(it);
              char v[100];
              string ax = "unsubscribed ";
              ax = ax + (buffer + 2);
              ret = send(i, ax.c_str(), ax.length(), 0);
              DIE(ret < 0, "unns");
            }
          }
        }
      }
    }
  }
   // closing sockets
  close(sock_tcp);  
  close(sock_udp);

  return 0;
}
