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

#define TAILLE_FRAGMENT 800

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}

/*
  Écrit dans le fichier "char * fichier" les données data du paquet d'index "int index"
  len : longueur des données + en-tête du segment
  index : index du segment
  data : données
  fichier : chemin du fichier à écrire
*/
void ecriture_fragment(int len, int index, char * data, char * fichier)
{
  int fd = open(fichier, O_RDWR | O_CREAT, 0666);

  if(fd == -1) erreur("open - ecriture_chunk");

  // TAILLE_FRAGMENT
  if(lseek(fd, index * TAILLE_FRAGMENT, SEEK_SET) == -1)
    erreur("lseek - ecriture_chunk");

  if(write(fd, data, len - 4) == -1)
    erreur("write - ecriture_chunk");

  if(close(fd) == -1);
    erreur("close - ecriture_chunk");
}

int main(int argc, char ** argv)
{
  int sockfd;
  unsigned char buf[BUF_SIZE];
  socklen_t addrlen;

  struct sockaddr_in6 my_addr; // in = internet
  struct sockaddr_in6 client;
  struct sockaddr_in6 dest;
  unsigned char * msg;

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

  msg=malloc(70*sizeof(char));
  msg[0]=102;
  unsigned short int tmp= 67;
  memcpy(msg+1, &tmp, 2);
  msg[3]=50;
  tmp= 64;
  memcpy(msg+4, &tmp, 2);
  memcpy(msg+6, argv[4], 64);
  memset(buf, '0', BUF_SIZE);
  while(buf[0] != 103 && memcmp(msg+6, buf+6, 64) != 0)
  {
    int size_msg=70;
    if(sendto(sockfd, msg, size_msg, 0, (struct sockaddr *) &dest, addrlen) == -1)
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
  char * chunk= malloc(65);
  int nbChunk=0;
  char **tab= malloc(((lg-70)/68) * sizeof(char *));
  printf("%d\n", ((lg-70)/68));
  for(i=70; i<lg; i++)
  {
    i=i+3;
    memcpy(chunk, buf+i, 64);
    chunk[64]='\0';
    tab[nbChunk]= malloc(65*sizeof(char));
    strcpy(tab+nbChunk, chunk);
    printf("%s\n", chunk);
    i=i+65;
    nbChunk++;
  }
  free(msg);

  msg= malloc(139*sizeof(char));
  msg[0]=100;
  unsigned short int tmp=136;
  memcpy(msg,+1 &tmp, 2);
  msg[3]= 50;
  tmp=64;
  memcpy(msg+4, &tmp, 2);
  memcpy(msg, argv[4], 64);

  int pos_ecriture=0;
  for(i=0; i<nbChunk; i++)
  {
    msg[70]=51;
    tmp=66;
    memcpy(msg+71, &tmp, 2);
    memcpy(msg+73, tab[i], 64);
    memcpy(msg+137, &i, 2);

    if(sendto(sockfd, msg, 139, 0, (struct sockaddr *) &client, addrlen) == -1)
      erreur("sendto");

    memset(buf, '\0', BUF_SIZE);
    int nb_part=2;
    int nb_courant=1;
    while(nb_courant <= nb_part)
    {
      memset(buf, '\0', BUF_SIZE);
      if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1)
        erreur("recvfrom");
      if(memcpy(buf+1, msg+1, 138) != 0)
        continue;
      if(buf[139] != 60)
        continue;
      unsigned short int nb_courant_buf;
      unsigned short int lg;
      memcpy(&lg, buf+140, 2);
      memcpy(&nb_courant_buf, buf+142, 2);
      if(nb_courant_buf != nb_courant)
      {
        i--;
        break;
      }
      memcpy(&nb_part, buf+144, 2);
      char buf_ecriture= malloc((lg-3)*sizeof(char));
      memcpy(buf_ecriture, buf+146, lg-4);
      buf_ecriture[lg-4]= '\0';
      pos_ecriture= pos_ecriture+lg-4;
      //faire write dans le fichier;
      // envoyer ack
    }
  }

  return 0;
}
