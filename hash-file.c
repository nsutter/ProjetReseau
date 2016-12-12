#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gcrypt.h>

/* renvoie la taille du fichier FILE * fd en octet
*/
long tailleFichier(FILE * fd)
{
  long taille;

  if(fd)
  {
    fseek(fd, 0, SEEK_END);
    taille = ftell(fd);
    fseek(fd, 0, SEEK_SET);
  }

  return taille;
}

/* renvoie le hash d'un fichier
  char * fichier = chemin du fichier
*/
char * hashFichier(char * fichier)
{
  FILE * fd = fopen(fichier, "r");

  long taille = tailleFichier(fd);

  void * buffer = malloc(taille); // buffer pour stocker le fichier entier

  fread(buffer, 1, taille, fd); // buffer contient maintenant les données du fichier

  // CALCUL DU HASH
  unsigned char hash[32]; // hash calculé

  gcry_md_hash_buffer(GCRY_MD_SHA256, hash, buffer, taille);

  // COPIE DU HASH
  char * sortie = malloc(sizeof(char) * ((32 * 2) + 1)), * p = sortie;

  for(int i = 0; i < 32; i++, p += 2) // hash converti
  {
    snprintf(p, 3, "%02x", hash[i]);
  }

  fclose(fd);

  return sortie;
}

/* renvoie un tableau des hashs des chunks d'un fichier (1 chunk = 1 Mo)
  char * fichier = chemin du fichier
  int * nb modifié par effet de bord, prend le nb. de chunks
*/
char ** hashAllChunks(char * fichier, int * nb)
{
  FILE * fd = fopen(fichier, "r");

  long taille = tailleFichier(fd);

  void * buffer = malloc(1000000); // buffer pour stocker le fichier entier

  int nChunks = taille / 1000000 + 1;

  unsigned char ** hashs = malloc(nChunks * sizeof(unsigned char *)); // tableau des hashs calculés
  char ** sorties = malloc(nChunks * sizeof(char *)); // tableau des hashs convertis
  char ** p = malloc(nChunks * sizeof(char *)); // tableau des hashs convertis (seconde référence pour la conversion)

  for(int i = 0; i < nChunks; i++)
  {
    hashs[i] = malloc(32 * sizeof(unsigned char));
    sorties[i] = malloc(sizeof(char) * ((32 * 2) + 1));
    p[i] = sorties[i];
  }

  // boucle
  for(int k = 0; k < nChunks; k++)
  {
    fread(buffer, 1, 1000000, fd); // buffer contient maintenant les données du chunk

    gcry_md_hash_buffer(GCRY_MD_SHA256, hashs[k], buffer, 1000000); // calcul du hash

    for(int j = 0; j < 32; j++, p[k] += 2) // conversion du hash
    {
      snprintf(p[k], 3, "%02x", hashs[k][j]);
    }
  }

  *nb = nChunks;

  fclose(fd);

  return sorties;
}

// int main(int argc, char *argv[])
// {
//   if(argc != 2)
//   {
//     exit(1);
//   }
//
//   printf("%s", hashFichier(argv[1]));
//
//   // hashAllChunks(argv[1]);
// }
