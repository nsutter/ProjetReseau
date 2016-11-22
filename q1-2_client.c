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

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}

char * concat(char* str1, char * str2)
{
  int lg1= strlen(str1);
  int lg2= strlen(str2);
  char * res= malloc(sizeof((lg1+lg2+2)*sizeof(char)));
  int i;
  for(i=0; i<lg1; i++)
  {
    res[i]=str1[i];
  }
  res[lg1]=' ';
  for(i=1; i<=lg2; i++)
  {
    res[i+lg1]= str2[i-1];
  }
  res[lg1+lg2+1]= '\0';
  return res;
}

int main(int argc, char **argv)
{
  int sockfd;
  char buf[1024];
  socklen_t addrlen;

  struct sockaddr_in6 my_addr; // in = internet
  struct sockaddr_in6 client;
  struct sockaddr_in6 dest;

  // check the number of args on command line
  if(argc != 6)
  {
    printf("nombre d'arguments incorrect \n");
    exit(-1);
  }

  // socket factory
  if((sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) // d'après man et pdf
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // init local addr structure and other params
  my_addr.sin6_family      = AF_INET6;
  my_addr.sin6_port        = htons(atoi(argv[3])); // transformation en uint16_t
  my_addr.sin6_addr        = in6addr_any;
  addrlen                  = sizeof(struct sockaddr_in6);

  dest.sin6_family = AF_INET6;
  dest.sin6_port   = htons(atoi(argv[2]));

  if(strcmp(argv[1], "localhost") == 0) dest.sin6_addr = in6addr_any;
  else
  {
    if(inet_pton(AF_INET6, argv[1], &(dest.sin6_addr)) != 1) erreur("inet_pton");
  }


  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET6, &(dest.sin6_addr), str, INET6_ADDRSTRLEN) == NULL) {
    perror("inet_ntop");
    exit(EXIT_FAILURE);
  }

  memset(buf,'\0',1024);

  // bind addr structure with socket
  if(bind(sockfd, (struct sockaddr *) &my_addr, addrlen) == -1)
  {
    perror("bind");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("listening on %s \n", argv[3]);
  char *msg = concat(argv[4],argv[5]);

  printf("%s from %s port %s\n", msg, str, argv[2]);
  if(sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *) &dest, addrlen) == -1)
  {
    perror("sendto");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
  free(msg);

  while(42)
  {
    // reception de la chaine de caracteres
    if(recvfrom(sockfd, buf, 1024, 0, (struct sockaddr *) &client, &addrlen) == -1)
    {
      perror("recvfrom");
      close(sockfd);
      exit(EXIT_FAILURE);
    }

    // print the received char
    printf("%s\n", buf);
  }

  // close the socket
  close(sockfd);

  return 0;
}
