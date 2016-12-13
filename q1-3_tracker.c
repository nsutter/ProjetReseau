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

#define BUF_SIZE 1024
#define SLEEP_THREAD_TIMER 1
#define TIMEOUT 5

//associe a un hash une liste d'ip + port
typedef struct {char * hash; char * addr; int port; time_t timer;} association;

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}

// mutex pour protéger le tableau des associations et l'entier longueur à cause du thread qui gère l'expiration du temps
pthread_mutex_t mutex_association;

association * tableau;
int longueur=0;

/*
Supprime l'entrée à l'index id du tableau d'association
*/
void suppression(int id)
{
  if(id < longueur - 1)
  {
    int i;

    free(tableau[id].hash);
    free(tableau[id].addr);

    for(i = id; i < longueur - 1; i++) // on décale tout le tableau après id
    {
      tableau[i] = tableau[i + 1];
    }

    longueur--;

    tableau = realloc(tableau, longueur * sizeof(association));
  }
  else if(id == longueur - 1)
  {
    free(tableau[id].hash);
    free(tableau[id].addr);

    longueur--;

    tableau = realloc(tableau, longueur * sizeof(association));
  }
}

/*
  thread qui gère l'expiration du temps
*/
void * f_thread_timer(void * arg)
{
  // toutes les secondes on vérifie si un timer a expiré
  int i;

  struct timeval tv;

  while(42 == 42)
  {
    sleep(SLEEP_THREAD_TIMER);

    if(gettimeofday(&tv, NULL) == -1)
      erreur("gettimeofday");

    pthread_mutex_lock(&mutex_association);

    for(i = 0; i < longueur; i++)
    {
      if((tableau[i].timer + TIMEOUT) < tv.tv_sec)
      {
        suppression(i);
      }
    }

    pthread_mutex_unlock(&mutex_association);
  }
}

int test_existance(char * ad, int p, char * hash)
{
  int i;

  pthread_mutex_lock(&mutex_association);

  for(i=0; i<longueur; i++)
  {
    if(strncmp(tableau[i].hash, hash, strlen(hash)) == 0 && strcmp(tableau[i].addr, ad) == 0 && tableau[i].port == p)
    {
      struct timeval tv;
      if(gettimeofday(&tv, NULL) == -1)
        erreur("gettimeofday");
      tableau[i].timer= tv.tv_sec;
    }
  }

  pthread_mutex_unlock(&mutex_association);

  return 0;
}

void ajout(char * ad, int p,char * hash)
{
  if(test_existance(ad, p, hash) == 0)
  {
    pthread_mutex_lock(&mutex_association);

    longueur= longueur+1;
    tableau= realloc(tableau, longueur * sizeof(association));
    tableau[longueur-1].hash= hash;
    tableau[longueur-1].addr= ad;
    tableau[longueur-1].port= p;
    struct timeval tv;
    if(gettimeofday(&tv, NULL) == -1)
      erreur("gettimeofday");
    tableau[longueur-1].timer= tv.tv_sec;

    pthread_mutex_unlock(&mutex_association);
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

  pthread_mutex_lock(&mutex_association);

  for(i=0; i<longueur; i++)
  {
    if(strncmp(msg+6, tableau[i].hash, lg_hash) == 0)
    {
      lg= lg+21;
      msg= realloc(msg, lg);
      msg[lg-21]= 55;
      unsigned short int tmp= 18;
      memcpy(msg+lg-20, &tmp, 2);
      uint16_t tmp1;
      tmp1= ntohs(tableau[i].port);
      memcpy(msg+lg-18, &tmp1, 2);
      struct sockaddr_in6 sa;
      inet_pton(AF_INET6, tableau[i].addr, &(sa.sin6_addr));
      memcpy(msg+lg-16, &(sa.sin6_addr), 16);
    }
  }

  pthread_mutex_unlock(&mutex_association);

  memcpy(msg+1, &lg, 2);
}

char * deformatage(unsigned char* buf, char code, unsigned short int * lg_total, struct sockaddr_in6 client)
{
  char * msg;
  struct sockaddr_in6 stock;

  // si le code du message n'est pas le bon, on s'arrête
  if(buf[0] != code) return NULL;

  // on récupère la longueur totale
  memcpy(lg_total,buf+1, 2);

  if(buf[3] != 50) return NULL;
  short unsigned int lg_hash= (unsigned short int)*(buf+4);
  char * hash= malloc((1+lg_hash)*sizeof(char));
  memcpy(hash, buf+6, lg_hash);
  hash[lg_hash]='\0';
  memcpy(&stock.sin6_port, buf+lg_hash+9, 2);
  memcpy(&stock.sin6_addr, buf+lg_hash+11, 16);
  char *adr= malloc(INET6_ADDRSTRLEN*sizeof(char));
  adr= (char * ) inet_ntop(AF_INET6, &(client.sin6_addr), adr, INET6_ADDRSTRLEN);
  if(code == 110)
  {
    ajout(adr, ntohs(stock.sin6_port), hash);
    msg= malloc((*lg_total)*sizeof(char));
    memcpy(msg+1, buf+1, (*lg_total)-1);
  }
  else if(code == 112)
  {
    msg= malloc((lg_hash+6)*sizeof(char));
    memcpy(msg+3, buf+3, lg_hash+3);
    get(msg, lg_hash+6);
    ajout(adr, ntohs(stock.sin6_port), hash);
    memcpy(lg_total, msg+1, 2);
    *(lg_total) = *(lg_total)+1;
  }
  return msg;
}

void affiche()
{
  int i;

  pthread_mutex_lock(&mutex_association);

  for(i=0; i<longueur; i++)
  {
    printf("%s %d %s\n", tableau[i].hash, tableau[i].port, tableau[i].addr);
  }

  pthread_mutex_unlock(&mutex_association);
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
    printf("./q1-3_tracker traddr trport\n");
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

  pthread_mutex_init(&mutex_association, NULL);

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

  // création du thread qui gère l'expiration du temps
  pthread_t thread_timer;

  if(pthread_create(&thread_timer, NULL, f_thread_timer, NULL) != 0)
    exit(1);

  while(42)
  {
    // reception de la chaine de caracteres
    if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1){ erreur("recvfrom"); }
    if(buf[0] == 110)
    {
      short unsigned int lg;
      char * msg= NULL;
      char code= 110;
      msg= deformatage(buf, code, &lg, client);
      msg[0]=111;
      if(sendto(sockfd, msg, lg, 0, (struct sockaddr *) &client, addrlen) == -1) erreur("sendto");
      free(msg);
    }
    else if(buf[0] == 112)
    {
      short unsigned int lg;
      char * msg= NULL;
      char code= 112;
      msg= deformatage(buf, code, &lg, client);
      msg[0]=113;
      short unsigned int tmp;
      memcpy(&tmp, msg+4, 2);
      if(sendto(sockfd, msg, lg, 0, (struct sockaddr *) &client, addrlen) == -1) erreur("sendto");
      free(msg);
    }
    else if(buf[0] == 150)
    {
      affiche();
    }
    else if(buf[0] == 114)
    {
      unsigned short int lg;
      memcpy(&lg, buf+1, 2);
      if(buf[3] == 50)
      {
        unsigned short int lg_hash;
        memcpy(&lg_hash, buf+4, 2);
        char * hash=malloc((lg_hash+1)*sizeof(char));
        memcpy(hash, buf+6, lg_hash);
        hash[lg_hash]='\0';
        char addr[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &(client.sin6_addr), addr, INET6_ADDRSTRLEN) == NULL)
          erreur("inet_ntop");
        test_existance(addr,ntohs(client.sin6_port),hash);
        free(hash);
        char * msg= malloc((6+lg_hash)*sizeof(char));
        memcpy(msg, &buf, 6+lg_hash);
        msg[0]=115;
        if(sendto(sockfd, msg, 6+lg_hash, 0, (struct sockaddr *) &client, addrlen) == -1) erreur("sendto");
      }


    }
    memset(buf, '\0', BUF_SIZE);
  }

  // close the socket
  close(sockfd);

  return 0;
}
