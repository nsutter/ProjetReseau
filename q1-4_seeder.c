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

#define TAILLE_CHUNK 1000000

#define TAILLE_FRAGMENT 800

typedef struct {int id; int idmax; char * data;} fragments;

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}

/*
  -> vérifie si l'index est bon
  -> renvoie l'index du hashchunk ou -1 si erreur
*/
int indexChunk(char * hashChunkEntree, char * fichier)
{
  int i, nb;
  char ** hAllChunks = hashAllChunks(fichier, &nb);

  // on vérifie si le hashChunkEntree appartient bien à un hash de chunk du fichier et on récupère l'index
  for(i = 0; i < nb; i++)
  {
    if(strcmp(hashChunkEntree, hAllChunks[i]) == 0)
    {
      return i;
    }
  }

  return -1;
}

// tabFragments modifié par effet de bord
int recuperation_fragment(char * hashFichierEntree, char * hashChunkEntree, char * fichier, fragments * tabFragments)
{
  // on s'arrête si le fichier demandé n'est pas le fichier proposé
  if(strcmp(hashFichierEntree, hashFichier(fichier)) != 0)
  {
    printf("recuperation_fragment: hash du message et hash du fichier différent.");
    exit(1);
  }

  // on récupère l'index
  int iChunk = indexChunk(hashChunkEntree, fichier);

  if(iChunk == -1)
  {
    printf("indexChunk: échec de la récupération de l'index.");
    exit(1);
  }

  int iFichier = iChunk * TAILLE_CHUNK; // index lseek du fichier;

  int tFichier = tailleFichier(fichier);

  int nOctetsEnvoi; // nombre d'octets au total qu'on veut envoyer

  if(tFichier - iFichier > TAILLE_CHUNK)
  {
    nOctetsEnvoi = TAILLE_CHUNK; // cas où on envoie un chunk complet <=> 1 000 000 octets
  }
  else
  {
    nOctetsEnvoi = tFichier - (tFichier % TAILLE_CHUNK); // cas où on envoie le dernier chunk <=> < 1 000 000 octets
  }

  int nEnvoi = nOctetsEnvoi / TAILLE_FRAGMENT; // nombre de paquets qu'on doit envoyer pour envoyer nOctetsEnvoi octets

  if(nOctetsEnvoi % TAILLE_FRAGMENT != 0)
  {
    nEnvoi++;
  }

  // création du tableau
  tabFragments = malloc(nEnvoi * sizeof(fragments));

  int i, fd = open(fichier, O_RDONLY);

  if(lseek(fd, iFichier, SEEK_SET) == -1)
    erreur("recuperation_fragment");

  int tailleLue;

  for(i = 0; i < nEnvoi; i++)
  {
    tabFragments[i].id = i; // index courant
    tabFragments[i].idmax = nEnvoi - 1; // index max

    tabFragments[i].data = malloc(TAILLE_FRAGMENT * sizeof(char));

    tailleLue = read(fd, tabFragments[i].data, TAILLE_FRAGMENT);

    if(tailleLue == -1)
      erreur("recuperation_fragment");

    if(tailleLue < TAILLE_FRAGMENT)
      printf("DERNIER BLOC (censé s'arrêter mtn)\n");
  }

  return tailleLue;
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
