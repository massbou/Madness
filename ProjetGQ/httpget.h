

GQueue * file_telecharger;
GQueue * file_analyser;

char * Deja_tel[1000];

typedef struct
{
   pthread_mutex_t mutex_telecharger;
   pthread_cond_t cond_telecharger;
   pthread_cond_t cond_analyser;
   pthread_t *thread_telecharger;
   pthread_t thread_analyser[];
}
pageweb;


typedef struct
{
   char * serveur;
   char * port;
   char * chemin;
   char * fichier;
}
httpgetargs;

typedef struct
{
   char * path;
   int prof;
}
cellule;

char *RecoieLigne(int sock);
void usage();
void * analyser(void * arguments);
void parserFichier(httpgetargs * args);
httpgetargs parserUrl(char * lien);
httpgetargs parserchemin(httpgetargs args);
void * telecharger(void *arguments);
char * revstrstr_supr(char * src, char cmp);
int getIndiceLast(char * src, char cmp);