#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <pthread.h>
#include <regex.h>
#include <dirent.h>
#include <sys/stat.h>
#include <glib.h>
#include "httpget.h"

#define TAILLE_BUFF 10
#define FAUX 0
#define VRAI 1

#define DEBUG


#define KRED  "\e[31;1m"
#define KGRN  "\e[32;1m"
#define KYEL  "\e[33;1m"
#define KBLU  "\e[34;1m"
#define KMAG  "\x1B[35m"
#define KCYN  "\e[38;5;39m"


static pageweb arpg =
{
   .mutex_telecharger = PTHREAD_MUTEX_INITIALIZER,
   .cond_telecharger = PTHREAD_COND_INITIALIZER,
   .cond_analyser = PTHREAD_COND_INITIALIZER,
};


/* Guide d'utilisation */
void usage()
{
    fprintf(stderr,"Le programme s'utilise de la manière suivante: ./html [-h ou --help][-p ou --prof profondeur]" );
    fprintf(stderr, "[-d ou --debug][--nb-th-get NbThreadTelecharger][--nb-th-analyse NbThreadAnalyse] <url> <nom du fichier>\n");
    fprintf(stderr,"L'url doit être écrite sans le http:// \n");
    fprintf(stderr,"Ex: ./html -p 2 --nb-th-get 1 --nb-th-analyse 3 restaurant-selam.fr/menus menus.html \n" );
}




char *RecoieLigne(int sock) {
  char buff[TAILLE_BUFF+1];
  int res;
  int fini = FAUX;
  int taillerecu = 0;
  int tailletotal = 0;
  int taillereserve = TAILLE_BUFF;
  int aretirer;

  // le restultat est une chaine vide au départ
  char *result = (char*)malloc(taillereserve*sizeof(char));
  result[0] = '\0';

  while(!fini) {
    // lecture sans vraiment lire pour savoir si la chaine contient un \n
    res = recv(sock, buff, TAILLE_BUFF, MSG_PEEK);
    if (res < 0) {
      perror("Probleme à la lecture de la socket");
      free(result);
      return NULL;
    }
    if (res == 0) {
      // la socket est coupée sans retour chariot,
      if (taillerecu > 0) {
    // si on a recu des octets,  on suppose que ce n'est pas une erreur
    // et on retourne ce qu'on a.
    break;
      } else {
    // sinon on est en train de lire sur une socket fermée, on retourne NULL
    free(result);
    return NULL;
      }
    }
    // on s'assure que la chaine buff ne sera pas analysée après la fin des octets reçus
    buff[res] = '\0';
    taillerecu += res;

    int nbchar = res;
    // on cherche si c'est fini en utilisant strtok_r (version reentrant de strtok).
    char *pos = strchr(buff, '\n');
    if (pos == NULL) {
      // le carractère \n n'a pas été trouvé, la chaine n'est pas terminée dans ce qu'on a recu
      tailletotal += res;
      // il faut tout retirer
      aretirer = res;
    } else {
      // le caractère \n a été trouvé en posision pos
      nbchar = pos-buff;
      tailletotal += nbchar;
      buff[nbchar] = '\0';
      aretirer = nbchar+1;
      fini = VRAI;
    }

    if (tailletotal+1 > taillereserve) {
      // il manque de place dans le tableau
      taillereserve *= 2;
      result = (char *) realloc(result, taillereserve);
    }
    // strlen(result)+strlen(buff) est forcement < taillereserve car soit tailletotal<taillereserve soit on vient de rajouter au moins
    //TAILLE_BUFF>res
    strcat(result, buff);

    // il faut vider la socket de ce qu'on a lu c'est à dire la chaine + le \n
    res = recv(sock, buff, aretirer, 0);
    if (res < 0) {
      perror("second recv");
      free(result);
      return NULL;
    }
    if (res == 0) {
      perror("second recv renvoie 0");
      free(result);
      return NULL;
    }
  }

  // certaine fois, les fins de lignes comportent \r\n, dans ce cas, ce code pose problème car il laisse le \r
  if (result[tailletotal-1] == '\r') {
    result[tailletotal-1] = '\0';
  }

  return result;
}



int recoiTailleFixee(int s, int taille, int fd){
	int recoie = 0;
	int tailleTemporaire =100;
	int tailleRecue=0;
	char buf[tailleTemporaire];
    memset (&buf, 0, sizeof(buf));

	while(tailleRecue<taille){
        if((recoie+tailleRecue)>taille){
            recoie = recv(s, buf, taille-tailleRecue, 0);
        }
        else{
		  recoie = recv(s, buf, tailleTemporaire, 0);
        }
		if(recoie==-1)
		{
		    perror("Erreur lors du recv");
		    exit(1);
		}
		/* Si la page a terminé. */
		if (recoie == 0) {
		    break;
		}
		//printf("%s\n", );
		tailleRecue += recoie;
        int res = write(fd, buf,recoie);
        if(res==-1){
            perror("erreur sur l'écriture");
            exit(1);
        }
	}
    //close(fd);
	return tailleRecue;
}




void http_get(httpgetargs * args)
{

    /* Notre message pour recevoir la page*/
    char * msg;

    // structure pour faire la demande
    struct addrinfo hints;
    // structure pour stocker et lire les résultats
    struct addrinfo *result, *rp;
    // socket  (s)
    int s=-1;
    // variables pour tester si les fonctions donnent un résultats ou une erreur
    int res;
    int bon;
    // Des variable pour contenir de adresse de machine et des numero de port afin de les afficher
    char hname[NI_MAXHOST], sname[NI_MAXSERV];


    // on rempli la structure hints de demande d'adresse
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* IPv4 ou IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* socket flux connectée */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;

    res = getaddrinfo(args->serveur, args->port, &hints, &result);
    if (res != 0) { // c'est une erreur
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
    exit(1);
    }

    // si res = 0 le véritable résultat de la fontion est l'argument result
    // qui contient une liste d'addresse correspondant à la demande on va les
    // rester jusqu'a trouver une qui convient
    rp = result;
    bon = 0;
    while (rp != NULL) {
    // on parcourt la liste pour en trouver une qui convienne

    // on récupère des informations affichables
    res = getnameinfo(rp->ai_addr, rp->ai_addrlen,
              hname, NI_MAXHOST,
              sname, NI_MAXSERV,
              NI_NUMERICSERV|NI_NUMERICHOST);
    if (res != 0) {
      fprintf(stderr, "getnameinfo: %s\n", gai_strerror(res));
      exit (1);
    }
    fprintf (stderr, "On tente l'adresse %s sur le port %s .....",
         hname, sname);

    // on essaye
    s = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
    // si le résultat est -1 cela n'a pas fonctionné on recommence avec la prochaine
    if (s == -1) {
      perror("Création de la socket");
      rp = rp->ai_next;
      continue;
    }

    // si la socket a été obtenue, on essaye de se connecter
    res = connect(s, rp->ai_addr, rp->ai_addrlen);
    if (res == 0 ) {// cela a fonctionné on est connecté
      bon = 1;
      fprintf (stderr, "OK\n");
      break;
    }
    else { // sinon le bind a été impossible, il faut fermer la socket
      perror("Imposible de se connecter");
      close (s);
    }

    rp = rp->ai_next;
    }

    if (bon == 0) { // Cela n'a jamais fonctionné
    fprintf(stderr, "Aucune connexion possible\n");
    exit(1);
    }



    /* "requete" pour recevoir la page web, qu'on demande au serveur */
    const char * requete ="GET /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: \r\n\r\n";

    /* This holds return values from functions. */
    int status;
    msg = malloc((strlen(requete)+strlen(args->chemin)+strlen(args->serveur))*sizeof(char));
    sprintf (msg, requete, args->chemin, args->serveur);

    /* Send the request. */
    status = send (s, msg, strlen (msg), 0);
    if(status==-1)
    {
        perror("Erreur dans le send");
        exit(1);
    }

    int taille;
    char * taillec;
    char *ligne;
    int content_length=0;
    int chunck=0;
    int chunck_taille=0;
    taille=100;
    /* tableau où l'on recoie la page. */

    /* on initialise la mémoire de taille buf à 100*/
    printf(KCYN "ON RENTRE DANS: %s\n", args->chemin);
    char * chemin = (char *) malloc((strlen(args->chemin) + strlen(args->serveur) + strlen("index.html") + 3) * sizeof(char));
    strcpy(chemin, "./");
    strcat(chemin, args->serveur);
    strcat(chemin, args->chemin);
    if(strcmp(args->chemin, "/") == 0)
    {
        strcat(chemin, "index.html");
    }
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        fprintf(stdout, "On est dans le doissier: %s\n", cwd);

    while(1)
    {
        ligne = RecoieLigne(s);
            printf("%s\n", ligne);
        if(strlen(ligne)==0)
            break;
        if(strncasecmp(ligne, "Content-length: ", 16)==0)
        {
            taillec = strstr(ligne, ":");
            taillec += 2;
            taille = atoi(taillec);
            content_length=1;
        }
        if(strncasecmp(ligne, "Transfer-Encoding: chunked", 26)==0)
        {
            chunck=1;
        }
        if(strncasecmp(ligne, "Content-Type: text/html", 23)==0)
        {
            cellule * c;
            c = malloc(sizeof(cellule));
            c->path = malloc((strlen(chemin) + 1) * sizeof(char));
            strcpy(c->path, chemin);
            c->prof = 1;
            file_analyser= g_queue_new();
            g_queue_push_head(file_analyser, c);
        }
    }
    int fd=open(chemin, O_CREAT | O_WRONLY ,0777);
    if(fd< 0){
        perror("Erreur pour ouvrir!");
        exit(1);
    }
    free(chemin);
    while (1) {
            /* On commence à recevoir. */
    	int tailleRecue;
        if(content_length)
        {
    		tailleRecue= recoiTailleFixee(s, taille,fd);
                printf("taille recue = %d\n",tailleRecue);
        }
        else if(chunck)
        {
            while(1)
            {
                ligne = RecoieLigne(s);
                    // printf("%s\n", ligne);
                if(ligne==NULL /*|| strlen(ligne) == 0*/)
                {
                       printf("ligne vide ou nulle\n");

                    break;
                }
		        sscanf(ligne,"%x\n",&chunck_taille);
                    printf("taille a recevoir = %d\n", chunck_taille);
		        if (chunck_taille==0 && strlen(ligne) != 0)
		        {
                       printf("chunk taille est 0\n");

			        break;
		        }
                if(strlen(ligne)!=0){
                    tailleRecue=recoiTailleFixee(s, chunck_taille,fd);
                }
                    printf("taille recue = %d\n",tailleRecue);
            }
        }
        break;
    }
    if (close(s)< 0) {
        perror("Problème à la fermeture de la socket");
    }
    free (msg);
    printf("finii\n");
}





void * telecharger(void * arguments)
{
    int i = 0;
    printf(KYEL "TELECHARGEMENT!!!\n");
    while(1)
    {
        printf(KBLU "Avant\n");
        pthread_mutex_lock (& arpg.mutex_telecharger);
        if(g_queue_is_empty(file_telecharger) == TRUE)
        {
            if(g_queue_is_empty(file_analyser) == TRUE)
            {
                g_queue_free(file_telecharger);
                g_queue_free(file_analyser);
                printf(KRED "*******************PROGRAMME a FINI**************************\n");
                exit(42);
            }
            else
            {
                pthread_cond_signal(& arpg.cond_analyser);
                pthread_cond_wait(& arpg.cond_telecharger, & arpg.mutex_telecharger);
            }
        }
        else
        {
            httpgetargs * args = arguments;
            cellule * c = g_queue_pop_head(file_telecharger);
            printf(KRED "TELECHARGEMENT PR: LE PATH EST %s et la profondeur EST %d\n", c->path, c->prof);
            (*args) = parserUrl(c->path);
            (*args) = parserchemin((*args));
            http_get(args);
            if(Deja_tel[i] == NULL)
                printf(KYEL "CA passe!!!\n");
            while(Deja_tel[i] != NULL)
                i++;
            Deja_tel[i] = malloc((strlen(c->path) + 1) * sizeof(char));
            strcpy(Deja_tel[i], c->path);
            pthread_mutex_unlock (& arpg.mutex_telecharger);
        }
    }
    return NULL;
}


void * analyser(void * arguments)
{
    printf(KRED "ANALYSE!!!\n");
    while(1)
    {
        httpgetargs * args = arguments;
        pthread_mutex_lock (& arpg.mutex_telecharger);
        if(g_queue_is_empty(file_analyser) == TRUE)
        {
            if(g_queue_is_empty(file_telecharger) == TRUE)
            {
                g_queue_free(file_telecharger);
                g_queue_free(file_analyser);
                printf(KRED "*******************PROGRAMME a FINI**************************\n");
                exit(42);
            }
            else
            {
                pthread_cond_signal(& arpg.cond_telecharger);
                pthread_cond_wait(& arpg.cond_analyser, & arpg.mutex_telecharger);
            }
        }
        else
        {
            parserFichier(args);
        }
        pthread_mutex_unlock (& arpg.mutex_telecharger);
    }
    return NULL;
}

int getIndiceLast(char * src, char cmp)
{
    int i;
    int ret = 0;
    for(i=strlen(src); i>=0; i-- )
    {
        if(src[i] == cmp)
        {
            ret = i;
            break;
        }
    }
    return ret;
}

char * revstrstr_supr(char * src, char cmp)
{
    char * ret;
    int i;
    int indice = getIndiceLast(src, cmp);
    ret = malloc((indice + 1) * sizeof(char));
    for(i= 0; i< indice; i++)
    {
        ret[i] = src[i];
    }
    ret[indice] = '\0';
    return ret;
}

void parserFichier(httpgetargs * args)
{
    const char * liens = "(<a(.*)href=\"([^\"]*)\"(.*)>)|(<link(.*)href=\"([^\"]*)\"(.*)>)|(<img(.*)src=\"([^\"]*)\"(.*)/>)|(<script(.*)src=\"([^\"]*)\"(.*)>)";
    regex_t regliens;
    int ret;
    FILE *fd;
    size_t len = 0;
    char * line = NULL;
    ssize_t read;
    int match;
    char * href = "href=\"";
    char * src = "src=\"";
    char * stop = "\"";
    char * dhref;
    char * dsrc;
    char * urld;
    char * urlf;
    char * reste;
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        fprintf(stdout, "On est dans le doissierRRRRRRRR: %s\n", cwd);
    cellule * c = g_queue_pop_head(file_analyser);
    fprintf(stdout, "On ANALYSE LE FICHIER: %s\n", c->path);
    fd = fopen(c->path, "r+");
    if(fd == NULL)
    {
        fprintf(stderr,"erreur ouverture de fichier \n");
        exit(42);
    }

    ret = regcomp(&regliens, liens, REG_EXTENDED);
    if(ret)
    {
        fprintf(stderr, "Erreur compilation Ex Reg \n");
        exit(1);
    }
    while ((read = getline(&line, &len, fd)) != -1)
    {
           // printf("Retrieved line of length %zu :\n", read);
            //printf("%s", line);
        match = regexec(&regliens, line, 0, NULL, 0);
        if (match == 0)
        {
            dhref = strstr(line, href);
            if(dhref != NULL)
            {
                urld = malloc((strlen(dhref + 6) + 1) * sizeof(char));
                strcpy(urld,dhref += 6);
                reste = strstr(dhref, stop);
            }
            dsrc = strstr(line, src);
            if(dsrc != NULL)
            {
                urld = malloc((strlen(dsrc + 5) + 1) * sizeof(char));
                strcpy(urld,dsrc += 5);
                reste = strstr(dsrc, stop);
            }
            urlf = malloc((strlen(urld) - strlen(reste) + 1) * sizeof(char));
            int i;
            for(i = 0; i < strlen(urld) - strlen(reste); i++)
            {
                urlf[i] = urld[i];
            }
            urlf[strlen(urld) - strlen(reste)] ='\0';
            if(urlf[0] != '#')
            {
                printf(KRED "%s ", urlf);
                if (urlf[0] != '.' && urlf[0] != '/' && urlf[0] !='#')
                {
                    printf(KCYN "C'est un chemin TESTTTTTTT\n");
                    if(strstr(urlf, "http://") != NULL  || strstr(urlf, "https://") != NULL || strncmp(urlf, "www.", 4) == 0)
                    {
                        if(strstr(urlf, "https://") == NULL)
                        {
                            cellule * c2 = malloc(sizeof(cellule));
                            c2->path = malloc((strlen(urlf) + 1) * sizeof(char));
                            strcpy(c2->path, urlf);
                            c2->prof = c->prof + 1;
                            g_queue_push_head(file_telecharger, c2);
                            printf("C2PATH %s\n", c2->path );
                        }
                    }
                    else
                    {
                        if(strncmp(urlf, "tel:", 4) != 0)
                        {
                            char * url;
                            url= malloc((strlen(urlf) + strlen(c->path + 2))*sizeof(char));
                            strcpy(url, c->path+2);
                            while(strstr(url, "/") != NULL)
                            {
                                strcpy(url, revstrstr_supr(url, '/'));
                            }
                            strcat(url, "/");
                            strcat(url, urlf);
                            printf(KRED "URLLLLL FINALLL %s\n", url);
                            cellule * c2 = malloc(sizeof(cellule));
                            c2->path = malloc((strlen(url) + 1) * sizeof(char));
                            strcpy(c2->path, url);
                            c2->prof = c->prof;
                            g_queue_push_head(file_telecharger, c2);
                            printf("C2PATH %s\n", c2->path );

                        }

                    }
                }
                else if(urlf[0] == '/' && urlf[1] == '/')
                {
                    printf(KCYN "C'est un chemin relatif\n");
                    char * url;
                    url = malloc((strlen(urlf) + strlen("http:"))*sizeof(char));
                    strcpy(url, "http:");
                    strcat(url,urlf);
                    printf(KMAG "C'est un chemin vers un autre site ===> %s\n", url);
                    cellule * c2 = malloc(sizeof(cellule));
                    c2->path = malloc((strlen(url) + 1) * sizeof(char));
                    strcpy(c2->path, url);
                    c2->prof = c->prof + 1;
                    g_queue_push_head(file_telecharger, c2);
                    printf("C2PATH %s\n", c2->path );
                }
                else
                {

                    char * url;
                    if(strstr(c->path, "www.") != NULL)
                    {
                        url= malloc((strlen(revstrstr_supr(c->path + 2, '/')) + strlen("http://") + strlen(urlf)) * sizeof(char));
                        strcpy(url, "http://");
                    }
                    else
                    {
                        url= malloc((strlen(revstrstr_supr(c->path + 2, '/')) + strlen("http://www.") + strlen(urlf)) * sizeof(char));
                        strcpy(url, "http://www.");
                    }
                    strcat(url, revstrstr_supr(c->path + 2, '/'));
                    strcat(url, "/");
                    printf("CPATH %s\n", c->path + 2 );
                    printf(KGRN "LA CHAINEEEE: %s\n", revstrstr_supr(c->path + 2, '/'));
                    printf("L'indice est %d\n", getIndiceLast(c->path + 2, '/'));
                    strcat(url, urlf);
                    printf(KMAG "C'est un chemin absolue ===> %s\n", url);
                    cellule * c2 = malloc(sizeof(cellule));
                    c2->path = malloc((strlen(url) + 1) * sizeof(char));
                    strcpy(c2->path, url);
                    c2->prof = c->prof;
                    g_queue_push_head(file_telecharger, c2);
                    printf("C2PATH %s\n", c2->path );
                }
            }
        }
        // else
        // {
        //      fprintf(stderr, "Doesn't work on : %s\n", line);
        // }
    }
    regfree(&regliens);
    free(line);
    fclose(fd);
}






httpgetargs parserUrl(char * lien)
{
    char *serveur;
    char *chemin;
    char *port_chemin;
    char *port;
    char *url;
    char *slash="/";
    char *deuxpoint=":";
    char *http="http://";
    char *url_sansHttp;
    int tailleChem;

    url = lien;
    url_sansHttp = strstr(lien, http);
    if(url_sansHttp == NULL)
    {
        url_sansHttp = url;
    }
    else
    {
        url_sansHttp = url_sansHttp + 7;
    }
    chemin = strstr(url_sansHttp,slash);
    if(chemin != NULL)
    {
        tailleChem = strlen(chemin);
    }
    if(chemin == NULL)
    {
        chemin="/";
        tailleChem= 0;
    }
    port_chemin = strstr(url_sansHttp,deuxpoint);
    int taillePort;
    if(port_chemin!=NULL)
    {
        int j;
        taillePort = strlen(port_chemin)-tailleChem;
        port= malloc((taillePort) * sizeof(char));
        for(j = 0;j<taillePort;j++)
        {
            port[j] = port_chemin[j];
            printf("%c",port[j]);
        }
        port[taillePort]='\0';
        printf("le port est : %s\n", port);
    }
    else
    {
        port="80";
        taillePort = 0;
        printf("Le port par defaut sera : %s\n", port);
    }
    int i;
    serveur = malloc((strlen(url_sansHttp)-tailleChem) * sizeof(char));
    for(i = 0; i < strlen(url_sansHttp) - tailleChem - taillePort ; i++)
    {
        serveur[i] = url_sansHttp[i];
    }
    serveur[strlen(url_sansHttp) - tailleChem - taillePort] = '\0';
    printf("le serveur est : %s\n", serveur);
    printf("Le chemin est : %s\n",chemin );
    httpgetargs argument;
    argument.serveur = (char *) malloc((strlen(serveur) + 1) * sizeof(char));
    argument.port = (char *) malloc((strlen(port) + 1) * sizeof(char));
    argument.chemin = (char *) malloc((strlen(chemin) + 1) * sizeof(char));
    strcpy(argument.serveur, serveur);
    strcpy(argument.port, port);
    strcpy(argument.chemin, chemin);
    free(serveur);

    return argument;
}






httpgetargs parserchemin(httpgetargs args)
{
    char * chemin;
    char * cheminF;
    char * test;
    char * dossier;
    char * c_sans_d;
    int i;
    int repart = 1;
    mkdir(args.serveur, 0777);
    chdir(args.serveur);
    chemin = (char *) malloc((strlen(args.chemin) + 1) * sizeof(char));
    strcpy(chemin, args.chemin);
    printf("CHEMINNN ********************************************** %s\n", chemin);
    cheminF = (char *) malloc((strlen(args.chemin) + 1) * sizeof(char));
    strcpy(cheminF, "/");
    printf(KGRN "Test chemin %s\n", chemin);
    test = strstr(chemin, "/");
    printf(KGRN "%s\n\n", test);
    while(strstr(chemin+1, "/") != NULL)
    {
        printf(KMAG "Je rentre\n");
        long unsigned int taille = strlen(chemin);
        chemin += 1;
        c_sans_d = strstr(chemin, "/");
        long unsigned int taille_ap = strlen(c_sans_d);
        dossier = malloc((taille - taille_ap) * sizeof(char));
        for(i = 0; i<= taille - taille_ap -1; i++)
        {
            dossier[i] = chemin[i];
        }
        dossier[taille - taille_ap - 1] = '\0';
        if(strcmp(dossier, ".") != 0)
        {
            if(strcmp(dossier, "..") == 0)
            {
                chdir("./..");
            }
            else
            {
                strcat(cheminF, dossier);
                strcat(cheminF, "/");
                mkdir(dossier, 0777);
                chdir(dossier);
                repart++;
            }
        }

        printf(KRED "Le dossier est: %s\n", dossier);
        chemin = c_sans_d;
        printf(KBLU "Le nouveau chemin: %s\n", chemin);
    }
    // if(strcmp(chemin, "/") == 0)
    // {
    //     args.fichier = (char *) malloc((strlen("index.html") + 1) * sizeof(char));
    //     strcat(args.fichier, "index.html");
    // }
    // else
    // {
    //     args.fichier = (char *) malloc((strlen(chemin) + 1) * sizeof(char));
    //     strcpy(args.fichier, chemin+1);
    //     if(strstr(chemin+1, ".") == NULL)
    //     {
    //         printf("Je RENTREEEEEEEEEEE\n");
    //         strcat(args.fichier, ".html");
    //     }
    // }
    // printf(KYEL "LE FICHIER est: %s\n\n", args.fichier);
    int desc;
    for (desc = 0; desc < repart; desc++)
    {
        chdir("./..");
    }

    return args;
}


void test()
{
    printf("Je ne saus pas \n", );
}
