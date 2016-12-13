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

#define TAILLE_FRAGMENT 800 // taille d'un fragment de chunk envoyé en UDP

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
fragments * recuperation_fragment(char * hashFichierEntree, char * hashChunkEntree, char * fichier, int index)
{
  printf("hashChunkEntree : %s\n", hashChunkEntree);

  printf("deubt %s\n", fichier);
  printf("%s\n", hashFichier(fichier));
  printf("fin\n");

  // on s'arrête si le fichier demandé n'est pas le fichier proposé
  if(strcmp(hashFichierEntree, hashFichier(fichier)) != 0)
  {
    printf("recuperation_fragment: hash du message et hash du fichier différent.");
    exit(1);
  }

  printf("1\n");

  // on récupère l'index
  int iChunk = indexChunk(hashChunkEntree, fichier); // ou int iChunk = index;

  if(iChunk == -1)
  {
    printf("indexChunk: échec de la récupération de l'index.");
    exit(1);
  }

  printf("2\n");

  int i, fd = open(fichier, O_RDONLY);

  int iFichier = iChunk * TAILLE_CHUNK; // index lseek du fichier;

  int tFichier = lseek(fd, 0, SEEK_END);

  int nOctetsEnvoi; // nombre d'octets au total qu'on veut envoyer

  printf("iFichier : %d, tFichier %d\n", iFichier, tFichier);

  if(tFichier - iFichier > TAILLE_CHUNK)
  {
    nOctetsEnvoi = TAILLE_CHUNK; // cas où on envoie un chunk complet <=> 1 000 000 octets
  }
  else
  {
    nOctetsEnvoi = tFichier - (index) * TAILLE_CHUNK; // cas où on envoie le dernier chunk <=> < 1 000 000 octets
  }

  // printf("nOctetsEnvoi : %d\n", nOctetsEnvoi);

  int nEnvoi = nOctetsEnvoi / TAILLE_FRAGMENT; // nombre de paquets qu'on doit envoyer pour envoyer nOctetsEnvoi octets

  if(nOctetsEnvoi % TAILLE_FRAGMENT != 0)
  {
    nEnvoi++;
  }

  // création du tableau

  fragments * tabFragments = malloc(nEnvoi * sizeof(fragments));

  if(lseek(fd, iFichier, SEEK_SET) == -1)
    erreur("recuperation_fragment");

  int tailleLue;

  printf("nEnvoi : %d\n", nEnvoi);

  for(i = 0; i < nEnvoi; i++)
  {
    tabFragments[i].id = i; // index courant
    tabFragments[i].idmax = nEnvoi - 1; // index max
    tabFragments[i].data = malloc(TAILLE_FRAGMENT * sizeof(char));
    if(tabFragments[i].data == NULL)
    {
      printf("erreur malloc\n");
    }

    tailleLue = read(fd, tabFragments[i].data, TAILLE_FRAGMENT);

    if(tailleLue == -1)
      erreur("recuperation_fragment");

    if(tailleLue < TAILLE_FRAGMENT)
       printf("DERNIER BLOC (censé s'arrêter mtn)\n");
  }

  tabFragments[i - 1].data[tailleLue -1] = '\0';

  return tabFragments;
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
    printf("./q1-4_seeder port fichier\n");
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
    printf("reçu\n");
    unsigned short int lg_total;
    unsigned short int lg_hash;
    int new_lg;

    if(buf[0] == 100) // get
    {
      printf("message reçu\n");
      memcpy(&lg_total, buf+1, 2);
      new_lg= lg_total;
      if(buf[3] != 50)
        continue;
      memcpy(&lg_hash, buf+4, 2);
      char * tmp_hash= malloc((lg_hash+1)*sizeof(char));
      memcpy(tmp_hash, buf+6, lg_hash);
      tmp_hash[lg_hash]='\0';
      if(buf[6+lg_hash]!= 51)
        continue;
      unsigned short int size_hash;
      memcpy(&size_hash, buf+lg_hash+7, 2);
      size_hash=size_hash-2;
      char * hash_chunk= malloc((size_hash+1)*sizeof(char));
      memcpy(hash_chunk, buf+9+lg_hash, size_hash);
      hash_chunk[size_hash]= '\0';
      unsigned short int index;
      memcpy(&index, buf+size_hash+lg_hash+9, 2);
      fragments * tabFragments;
      tabFragments = recuperation_fragment(tmp_hash, hash_chunk, argv[2], index);
      unsigned short int i;
      unsigned short int nb_segment= tabFragments[0].idmax;
      unsigned short int taille_frag= TAILLE_FRAGMENT;
      unsigned short int lg_total= 11+lg_hash+size_hash+7+taille_frag;
      char * fragment= malloc(lg_total*sizeof(char));
      memcpy(fragment, buf, 11+lg_hash+size_hash);
      fragment[0]=101;
      fragment[11+lg_hash+size_hash]=60;
      memcpy(fragment+11+lg_hash+size_hash+1, &taille_frag, 2);
      memcpy(fragment+1, &lg_total, 2);
      for(i=0; i<nb_segment; i++)
      {
        memcpy(fragment+11+lg_hash+size_hash+3, &i, 2);
        memcpy(fragment+11+lg_hash+size_hash+5, &nb_segment, 2);
        memcpy(fragment+11+lg_hash+size_hash+7, tabFragments[i].data, taille_frag);
        if(sendto(sockfd, fragment, lg_total, 0, (struct sockaddr *) &client, addrlen) == -1)
          erreur("sendto");
        if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1)
          erreur("recvfrom");
        if(buf[0] != 105)
          continue;
        if(memcmp(fragment+3, buf+3, 133) != 0)
          continue;
        memcpy(&i, buf+139, 2);
      }
      unsigned short int taille_dernier= strlen(tabFragments[i].data);
      lg_total= 11+lg_hash+size_hash+7+taille_dernier;
      memcpy(fragment+1, &lg_total, 2);
      memcpy(fragment+11+lg_hash+size_hash+1, &taille_dernier, 2);
      memcpy(fragment+11+lg_hash+size_hash+3, &i, 2);
      memcpy(fragment+11+lg_hash+size_hash+5, &nb_segment, 2);
      memcpy(fragment+11+lg_hash+size_hash+7, tabFragments[i].data, taille_dernier);
      while(i<=nb_segment)
      {
        if(sendto(sockfd, fragment, lg_total, 0, (struct sockaddr *) &client, addrlen) == -1)
          erreur("sendto");
        if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1)
          erreur("recvfrom");
        if(buf[0] != 105)
          printf("erreur\n");
        if(buf[0] != 105)
          continue;
        if(memcmp(fragment+3, buf+3, 133) != 0)
          continue;
        memcpy(&i, buf+139, 2);
        i++;
      }
      free(tmp_hash);
      free(hash_chunk);
    }
    else if(buf[0] == 102) // list
    {
      // on récupère le hash dans le msg list
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
