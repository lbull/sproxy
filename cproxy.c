/*cproxy.c is used on client machine
 * It should listen to port 5200
 *Date: Thu Apr 23 23:38:20 MST 2015
 *Contact: emanuelvalente@gmail.com
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>

#include "protocol.h"

#define MAXPAYLOAD 10000
#define LISTEN_PORT 5200
#define SPROXY_PORT 6200
//#define SPROXY_PORT 23b

int startSProxyClient(char *serverIPAddr, int port);
void startCProxyServer();

int socketFromTelnetClient, socketFromSProxy;


char payload[MAXPAYLOAD];

void error(char *msg){
  perror(msg);
  exit(1);
}

/*It will receive incoming connections from the CProxy client*/
void startCProxyServer(char *sproxyIPAddr) {

  int sockfd, bytes_received, clientlen;
  struct sockaddr_in local_serv_addr, client_addr, serv_addr;
  fd_set readfds;
  struct timeval tv;
  int n, ret;
  proxyPacket_t proxyPacket;
  unsigned char heartBreak;
  unsigned int nextACK;

  /*connects to the SPROXY server*/
  socketFromSProxy = socket (AF_INET, SOCK_STREAM, 0);
  if (socketFromSProxy < 0)
    error ("ERROR opening socket\n");

  memset(&serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  /*convert and copy server's ip addr into serv_addr*/
  if(inet_pton(AF_INET, sproxyIPAddr, &serv_addr.sin_addr) <= 0) {
      fprintf(stderr, "%s is a bad address!\n", sproxyIPAddr);
      error ("ERROR, copying server ip address");
    }
  serv_addr.sin_port = htons(SPROXY_PORT);
  /* connect to server */
  if (connect(socketFromSProxy,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error ("ERROR connecting");
  fprintf(stderr, "Connected on %s:%d\n", sproxyIPAddr, SPROXY_PORT);

 /*at this point we can send some heartbeats*/

  /*attempting */
  while(1) {
      /*create a stream socket (TCP) -> to listen..*/
      sockfd = socket (AF_INET, SOCK_STREAM, 0);
      if(sockfd < 0)
          error ("ERROR opening socket");

      memset (&serv_addr, 0, sizeof(serv_addr));
      local_serv_addr.sin_family = AF_INET;
      local_serv_addr.sin_addr.s_addr = INADDR_ANY;
      local_serv_addr.sin_port = htons(LISTEN_PORT);

      if(bind(sockfd, (struct sockaddr *)&local_serv_addr, sizeof(struct sockaddr_in)) < 0)
          error("ERROR on binding");


      /*Listen!*/
      listen(sockfd, 5);
      fprintf(stderr, "Listen on port %d\n", LISTEN_PORT);
      clientlen = sizeof(client_addr);

      /*accept first incoming connection on the pending queue,
  returns a new file descriptor*/
      socketFromTelnetClient = accept(sockfd, (struct sockaddr *) &client_addr, &clientlen);

      if(socketFromTelnetClient < 0)
          error("ERROR on accept");

      /*to initialize telnet connection on sproxy side*/
      proxyPacket.header.type = NEW_CONNECTION_TYPE;
      send(socketFromSProxy, &proxyPacket, sizeof(proxyPacket_t), 0);



      /*At this point we have two active sockets:
   * one is socketFromTelnetClient connected to the end user
   * and socketFromTelnetClient connected to the SProxy
   * */
      proxyPacket.header.ack = 0;
      nextACK = 1;
      heartBreak = 0;
      while(1) {
          /*clear the set ahead of time*/
          FD_ZERO(&readfds);
          FD_SET(socketFromTelnetClient, &readfds);
          /*this socket is going to be "greater"*/
          FD_SET(socketFromSProxy, &readfds);


          /*setting our delay for the events*/
          tv.tv_sec = 1;
          tv.tv_usec = 500000;

          /*param for select()*/
          n = socketFromTelnetClient + 1;

          ret = select(n, &readfds, NULL, NULL, &tv);

          if(ret == -1)
              fprintf(stderr, "Error in select()!\n");
          else if(ret == 0) {
              fprintf(stderr, "Timeout occurred! No data after the specified time!\nSending heartbeat...\n");
              proxyPacket.header.type = HEARTBEAT_TYPE;
              proxyPacket.header.beatHeart = heartBreak++;
              send(socketFromSProxy, &proxyPacket, sizeof(proxyPacket_t), 0);
          }
          else {
              /*one of the both sockets has data to be received*/
              if(FD_ISSET(socketFromTelnetClient, &readfds)) {

                  bytes_received = recv(socketFromTelnetClient, proxyPacket.payload, sizeof(char) * MAXPAYLOAD, 0);
                  proxyPacket.header.type = APP_DATA_TYPE;
                  fprintf(stderr, "ACK: %d\n", proxyPacket.header.ack);
                  send(socketFromSProxy, &proxyPacket, sizeof(proxyHeader_t) + sizeof(char) * bytes_received, 0);
              }

              if(FD_ISSET(socketFromSProxy, &readfds)) {
                  bytes_received = recv(socketFromSProxy, &proxyPacket, sizeof(proxyPacket_t), 0);
                  if(proxyPacket.header.type == APP_DATA_TYPE && bytes_received > 0) {
                      /*TODO verify ACK*/
                      proxyPacket.header.ack++;
                      send(socketFromTelnetClient, proxyPacket.payload, bytes_received - sizeof(proxyHeader_t), 0);
                  }
              }


              memset(proxyPacket.payload, 0, sizeof(char) * MAXPAYLOAD);
              bytes_received = 0;

          }
      }

  close(socketFromTelnetClient);

  }


  close (socketFromSProxy);
  close(socketFromTelnetClient);
  close (sockfd);

}

int main(int argc, char * argv[]){

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip_addr>\n", argv[0]);
        exit(1);
    }

    startCProxyServer(argv[1]);

  return 0;
}
