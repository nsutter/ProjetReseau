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

#include "dns_solve.h"

#define BUF_SIZE 1024

//associe a un hash une liste d'ip + port
typedef struct {char * hash; char * addr; int port;} association;

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}

association * tableau;
int longueur=0;

int test_existance(char * ad, int p, char * hash)
{
  int i;
  for(i=0; i<longueur; i++)
  {
    if(strcmp(tableau[i].hash, hash) == 0 && strcmp(tableau[i].addr, ad) == 0 && tableau[i].port == p)
    return 1;
  }
  return 0;
}

void ajout(char * ad, int p,char * hash)
{
  if(test_existance(ad, p, hash) == 0)
  {
    longueur= longueur +1;
    tableau= realloc(tableau, longueur * sizeof(association));
    tableau[longueur-1].hash= hash;
    tableau[longueur-1].addr= ad;
    tableau[longueur-1].port= p;
  }
  else
  {
    free(ad);
    free(hash);
  }
}

void get(char * msg, unsigned short int lg)
{
  int i;
  unsigned short int lg_hash= lg-6;
  for(i=0; i<longueur; i++)
  {
    if(strncmp(msg+6, tableau[i].hash, lg_hash))
    {
      lg= lg+21;
      msg= realloc(msg, lg);
      msg[lg-21]= 55;
      unsigned short int tmp= 18;
      memcpy(msg+lg-20, &tmp, 2);
      uint16_t tmp1;
      tmp1= ntohs(tableau[i].port);
      memcpy(msg+lg-18, &tmp1, 2);
      struct in6_addr tmp2;
      inet_pton(AF_INET6, tableau[i].addr,&tmp);
      memcpy(msg+lg-16, &tmp2, 16);
    }
  }
  memcpy(msg+1, &lg, 2);
}

char * deformatage(unsigned char* buf, char code, unsigned short int * lg_total)
{
  char * msg;
  struct sockaddr_in6 stock;
  if(buf[0] != code) return NULL;
  memcpy(lg_total,buf+1, 2);
  msg= malloc((*lg_total)*sizeof(char));
  memcpy(msg+1, buf+1, (*lg_total)-1);
  if(buf[3] != 50) return NULL;
  short unsigned int lg_hash= (unsigned short int)*(buf+4);
  char * hash= malloc((1+lg_hash)*sizeof(char));
  memcpy(hash, buf+6, lg_hash);
  hash[lg_hash]='\0';
  memcpy(&stock.sin6_port, buf+lg_hash+9, 2);
  memcpy(&stock.sin6_addr, buf+lg_hash+11, 16);
  char *adr= malloc(sizeof(INET6_ADDRSTRLEN));
  adr= (char * ) inet_ntop(AF_INET6, &(stock.sin6_addr), adr, INET6_ADDRSTRLEN);
  if(code == 110)
  {
    ajout(adr, ntohs(stock.sin6_port), hash);
    msg= malloc((*lg_total)*sizeof(char));
    memcpy(msg+1, buf+1, (*lg_total)-1);
  }
  else if(code == 112)
  {
    msg= malloc((lg_hash+6)*sizeof(char));
    memcpy(msg+3, buf+1, lg_hash+3);
    get(msg, lg_hash+6);
    memcpy(lg_total, msg+1, 2);
    *(lg_total) = *(lg_total)+1;
  }
  return msg;
}

void affiche()
{
  int i;
  for(i=0; i<longueur; i++)
  {
    printf("%s %d %s\n", tableau[i].hash, tableau[i].port, tableau[i].addr);
  }
}

// renvoi 1 si le couple hash ip existe déjà

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
  unsigned char buf[BUF_SIZE];
  socklen_t addrlen;

  struct sockaddr_in6 my_addr; // in = internet
  struct sockaddr_in6 client;

  // check the number of args on command line
  if(argc != 3)
  {
    printf("nombre d'arguments insuffisants \n");
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
  my_addr.sin6_port        = htons(atoi(argv[2])); // transformation en uint16_t
  addrlen                  = sizeof(struct sockaddr_in6);

  int res;
  res = inet_pton(AF_INET6, argv[1], &(my_addr.sin6_addr));
  if(res == 0)
  {
    my_addr.sin6_addr= getip(argv[1]);
  }



  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET6, &(my_addr.sin6_addr), str, INET6_ADDRSTRLEN) == NULL) {
    perror("inet_ntop");
    exit(EXIT_FAILURE);
  }

  memset(buf,'\0',BUF_SIZE);

  // bind addr structure with socket
  if(bind(sockfd, (struct sockaddr *) &my_addr, addrlen) == -1)
  {
    perror("bind");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("listening on %s port %s\n", str, argv[2]);

  while(42)
  {
    // reception de la chaine de caracteres
    if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1){ erreur("recvfrom"); }
    if(buf[0] == 110)
    {
      short unsigned int lg;
      char * msg= NULL;
      char code= 110;
      msg= deformatage(buf, code, &lg);
      msg[0]=111;
      if(sendto(sockfd, msg, lg, 0, (struct sockaddr *) &client, addrlen) == -1) erreur("sendto");
      free(msg);
    }
    else if(buf[0] == 112)
    {
      short unsigned int lg;
      char * msg= NULL;
      char code= 112;
      msg= deformatage(buf, code, &lg);
      msg[0]=113;
      if(sendto(sockfd, msg, lg, 0, (struct sockaddr *) &client, addrlen) == -1) erreur("sendto");
      free(msg);
    }
    else if(buf[0] == 150)
    {
      affiche();
    }
    memset(buf, '\0', BUF_SIZE);
  }

  // close the socket
  close(sockfd);

  return 0;
}
