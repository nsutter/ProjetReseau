#ifndef HASH_FILE_H
#define HASH_FILE_H

long tailleFichier(FILE * fd);
char * hashFichier(char * fichier);
char ** hashAllChunks(char * fichier, int * nb);

#endif
