#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <sys/time.h>
#include <pthread.h>
#include "dns_solve.h"
#include "hash-file.h"

#define BUF_SIZE 1024

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}


int main(int argc, char ** argv)
{
  int sockfd;
  unsigned char buf[BUF_SIZE];
  socklen_t addrlen;

  struct sockaddr_in6 my_addr; // in = internet
  struct sockaddr_in6 client;
  struct sockaddr_in6 dest;
  char * msg;

  // check the number of args on command line
  if(argc != 6)
  {
    printf("nombre d'arguments insuffisants \n");
    exit(-1);
  }

  // socket factory
  if((sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) // d'après man et pdf
    erreur("sockfd");

  addrlen                  = sizeof(struct sockaddr_in6);
  my_addr.sin6_family      = AF_INET6;
  my_addr.sin6_port        = htons(atoi(argv[3])); // transformation en uint16_t
  my_addr.sin6_addr        = in6addr_any;

  dest.sin6_family      = AF_INET6;
  dest.sin6_port        = htons(atoi(argv[2])); // transformation en uint16_t

  int res;
  res = inet_pton(AF_INET6, argv[1], &(dest.sin6_addr));
  if(res == 0)
  {
    dest.sin6_addr= getip(argv[1]);
  }

  if(bind(sockfd, (struct sockaddr *) &my_addr, addrlen) == -1)
    erreur("bind");

  msg=malloc(38);
  msg[0]=102;
  unsigned short int tmp= 35;
  memcpy(msg+1, &tmp, 2);
  msg[3]=50;
  tmp= 32;
  memcpy(msg+4, &tmp, 2);
  memcpy(msg+6, argv[4], 32);
  memset(buf, '0', BUF_SIZE);
  while(buf[0] != 103 && memcmp(msg+6, buf+6, 32) ==0)
  {
    if(sendto(sockfd, msg, 38, 0, (struct sockaddr *) &client, addrlen) == -1)
      erreur("sendto");

    memset(buf, '0', BUF_SIZE);

    if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1)
      erreur("recvfrom");

  }
  printf("fin list chunk\n");
  unsigned short int lg;
  memcpy(&lg, buf+1, 2);
  int i;
  printf("%d\n", lg);
  char * chunk= malloc(32);
  for(i=38; i<lg; i++)
  {
    i=i+3;
    memcpy(chunk, buf+i, 32);
    printf("%s\n", chunk);
    i=i+34;
  }
  free(msg);
  while(42 == 42)
  {
    if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1)
      erreur("recvfrom");

    memset(buf, '\0', BUF_SIZE);

    if(sendto(sockfd, msg, BUF_SIZE, 0, (struct sockaddr *) &client, addrlen) == -1)
      erreur("sendto");
  }

  return 0;
}
