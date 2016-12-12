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
  char * msg;

  // check the number of args on command line
  if(argc != 3)
  {
    printf("nombre d'arguments insuffisants \n");
    exit(-1);
  }

  // socket factory
  if((sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) // d'après man et pdf
    erreur("sockfd");

  addrlen                  = sizeof(struct sockaddr_in6);
  my_addr.sin6_family      = AF_INET6;
  my_addr.sin6_port        = htons(atoi(argv[1])); // transformation en uint16_t
  my_addr.sin6_addr        = in6addr_any;

  if(bind(sockfd, (struct sockaddr *) &my_addr, addrlen) == -1)
    erreur("bind");

  char * hash = hashFichier(argv[2]);

  int nbChunks;
  char ** allChunks = hashAllChunks(argv[2], &nbChunks);

  memset(buf, '\0', BUF_SIZE);
  while(42 == 42)
  {
    memset(buf, 0, BUF_SIZE);
    if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1)
      erreur("recvfrom");

    unsigned short int lg_total;
    int new_lg;

    if(buf[0] == 100) // get
    {

    }
    else if(buf[0] == 102) // list
    {
      // on récupère le hash dans le msg list
      unsigned short int lg_hash;
      memcpy(&lg_total, buf+1, 2);
      new_lg= lg_total;
      memcpy(&lg_hash, buf+4, 2);
      char * tmp_hash= malloc((lg_hash+1)*sizeof(char));
      memcpy(tmp_hash, buf+6, lg_hash);
      tmp_hash[lg_hash]='\0';

      printf("hash calculé : %s\n",hash);
      printf("hash reçu : %s\n",tmp_hash);
      // on compare avec le hash de notre seeder
      if(strcmp(hash, tmp_hash) == 0)
      {
        // envoyer la réponse avec les chunks
        unsigned short int i;
        msg= malloc(69*nbChunks*sizeof(char)+lg_total+3);
        memcpy(msg+3, buf+3,lg_total);
        msg[0]= 101;
        for(i=0; i<nbChunks; i++)
        {
          msg[lg_total+3+i*69]=51;
          unsigned short int impala= 64;
          memcpy(msg+lg_total+4+i*69, &impala, 2);
          memcpy(msg+lg_total+6+i*69, allChunks[i], 64);
          memcpy(msg+lg_total+70+i*69, &i, 2);
          new_lg= new_lg+69;
        }
        new_lg++;
        memcpy(msg+1, &new_lg, 2);
      }
      else
      {
        continue;
      }
    }
    else // aucun des 2
    {
      continue;
    }

    if(sendto(sockfd, msg, new_lg, 0, (struct sockaddr *) &client, addrlen) == -1)
      erreur("sendto");
    free(msg);
  }

  return 0;
}
