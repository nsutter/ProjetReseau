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

// fonction qui test si l'ip+le port + le hash sont deja dans la tableau
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

//fonction qui ajoute au tableau un hash, port et ip
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

//si la fonction get est appeler elle modifie msg par effet de bord et y ajoute
// tous les ip+ port correspondant a un hash
//msg contient deja le hash
void get(char * msg, unsigned short int lg)
{
  int i;
  unsigned short int lg_hash= lg-6;
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
  memcpy(msg+1, &lg, 2);
}

//prends en argument un buf contenant le message reçu, le char code correspondant
// au type de message, un entier longueur totale qui est modifié par effet de bord
// et un sockaddr client qui contient les infos du client
char * deformatage(unsigned char* buf, char code, unsigned short int * lg_total, struct sockaddr_in6 client)
{
  //on extrait les données dans buf
  char * msg;
  struct sockaddr_in6 stock;
  // test si on a bien le type attendu
  if(buf[0] != code) return NULL;
  //copie de la longueur du message
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
  //j'utilise l'ip obtenue avec revfrom car celle dans le message est fausse
  adr= (char * ) inet_ntop(AF_INET6, &(client.sin6_addr), adr, INET6_ADDRSTRLEN);


  if(code == 110)
  {
    //si c'est un code 110 on ajoute l'ip + hash + port et on renvoi msg qui a
    // juste le code de debut qui diffère du message reçu
    ajout(adr, ntohs(stock.sin6_port), hash);
    msg= malloc((*lg_total)*sizeof(char));
    memcpy(msg+1, buf+1, (*lg_total)-1);
  }
  else if(code == 112)
  {
    //si le code est 112 on ajout l'ip, on copie le contenu du buf dans msg et on
    // y ajoute les ip et port qui correspondent au hash demander.
    msg= malloc((lg_hash+6)*sizeof(char));
    memcpy(msg+3, buf+3, lg_hash+3);
    get(msg, lg_hash+6);
    ajout(adr, ntohs(stock.sin6_port), hash);
    memcpy(lg_total, msg+1, 2);
    *(lg_total) = *(lg_total)+1;
  }
  return msg;
}

// affiche toutes les données qu'a le tracker
void affiche()
{
  int i;
  for(i=0; i<longueur; i++)
  {
    printf("%s %d %s\n", tableau[i].hash, tableau[i].port, tableau[i].addr);
  }
}


int main(int argc, char **argv)
{
  int sockfd;
  unsigned char buf[BUF_SIZE];
  socklen_t addrlen;

  struct sockaddr_in6 my_addr; // in = internet
  struct sockaddr_in6 client;

  // vérification des arguments
  if(argc != 3)
  {
    printf("./q1-2_tracker traddr trport\n");
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
      msg= deformatage(buf, code, &lg, client);
      // on modifier le code pour qu'il corresponde a un get
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
      // on modifier le code pour qu'il corresponde a un get
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
    memset(buf, '\0', BUF_SIZE);
  }

  // close the socket
  close(sockfd);

  return 0;
}
