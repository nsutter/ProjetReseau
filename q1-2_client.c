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

#include "dns_solve.h"

#define BUF_SIZE 1024

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}

/* Formate une adresse IP en texte */
char * formatage_ip(struct sockaddr_in6 my_addr)
{
  char * retour= malloc(21*sizeof(char));
  retour[0]= 55;
  unsigned short int i=18;
  memcpy(retour+1, &i, 2);
  memcpy(retour+3, &my_addr.sin6_port,2);
  memcpy(retour+5, &my_addr.sin6_addr, 16);
  return retour;
}

// formate un hash suivi de l'adresse
char * formatage_donnee(char type, char * hash_f, struct sockaddr_in6 my_addr, int * lg_msg)
{
  char * retour;
  unsigned short int lg= (short unsigned int) strlen(hash_f);
  unsigned short int lg_total= 27+lg*sizeof(char);
  *(lg_msg)= lg_total;
  retour= malloc(lg_total);
  memset(retour, 0, lg_total);
  retour[0]= type;
  memcpy(retour+1, &lg_total, 2);
  retour[3]=50;
  memcpy(retour+4, &lg, 2);
  memcpy(retour+6, hash_f, lg);
  char * tmp= formatage_ip(my_addr);
  memcpy(retour+6+lg, tmp, 21);
  free(tmp);
  return retour;
}

int main(int argc, char **argv)
{
  int sockfd;
  char buf[BUF_SIZE];
  socklen_t addrlen;

  struct sockaddr_in6 my_addr;
  struct sockaddr_in6 client;
  struct sockaddr_in6 dest;

  // check the number of args on command line
  if(argc != 6 || (argc != 5 && strncmp(argv[4], "aff", 3) == 0))
  {
    printf("usage: ./q1-2_client traddr trport clport type hash\n");
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

  int res;
  res = inet_pton(AF_INET6, argv[1], &(dest.sin6_addr));
  if(res == 0)
  {
    dest.sin6_addr= getip(argv[1]);
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
  char * msg;
  int lg_msg;
  int taille_cmp, shift;
  if(strncmp(argv[4], "put", 3) ==0)
  {
    char tmp=110;
    msg= formatage_donnee(tmp, argv[5], my_addr, &lg_msg);
    taille_cmp= lg_msg-1;
    shift= 1;
  }
  else if(strncmp(argv[4], "get", 3) == 0)
  {
    char tmp=112;
    msg= formatage_donnee(tmp, argv[5], my_addr, &lg_msg);
    taille_cmp= lg_msg-24;
    shift = 3;
  }
  else if(strncmp(argv[4], "aff", 3) ==0)
  {
    msg= malloc(2*sizeof(char));
    msg[0]= 150;
    msg[1]=10;
    lg_msg=2;
    taille_cmp= 1;
    shift = 1;
  }
  else
  {
    msg= NULL;
  }

  printf("%s %s from %s port %s\n", argv[4], argv[5], str, argv[2]);

  memset(buf, '\0', BUF_SIZE);
  //temps que le code de retour n'est pas le bon et le message de retour ne
  // correspond pas a ce qu'on attends on envoie
  while(memcmp(buf+shift, msg+shift, taille_cmp) != 0 && buf[0]+1 != msg[0])
  {
    if(sendto(sockfd, msg, lg_msg, 0, (struct sockaddr *) &dest, addrlen) == -1)
    {
      perror("sendto");
      close(sockfd);
      exit(EXIT_FAILURE);
    }
    if(taille_cmp ==1)
    {
      break;
    }
    memset(buf, 0, BUF_SIZE);
    // reception de la chaine de caracteres
    if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1)
    {
      perror("recvfrom");
      close(sockfd);
      exit(EXIT_FAILURE);
    }
  }
  free(msg);

  //si on a fait une requete get on affiche les clients
  if(strncmp(argv[4], "get", 3) == 0)
  {
    short unsigned int lg_total;
    int i=6;
    memcpy(&lg_total, buf+1, 2);
    short unsigned int tmp;
    memcpy(&tmp, buf+4, 2);
    i=i+tmp;
    for(; i<lg_total;)
    {
      i+=3;
      struct sockaddr_in6 stock;
      memcpy(&stock.sin6_port, buf+i, 2);
      i=i+2;
      memcpy(&stock.sin6_addr, buf+i, 16);
      i=i+16;
      char *adr= malloc(sizeof(INET6_ADDRSTRLEN));
      adr= (char * ) inet_ntop(AF_INET6, &(stock.sin6_addr), adr, INET6_ADDRSTRLEN);
      printf("IP: %s Port: %d\n", adr, ntohs(stock.sin6_port));
      free(adr);
    }
  }

  // close the socket
  close(sockfd);

  return 0;
}
