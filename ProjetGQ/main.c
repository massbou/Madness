#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/stat.h>
#include <glib.h>
#include "httpget.h"
#include <getopt.h>


int main (int argc, char **argv)
{
    int profondeur = 1;
    int NbThreadsTelecharger = 1;
    int NbThreadsAnalyser = 1;
    int ModeDebug = 0;
    opterr = 0;
    int options;
    int option_index = 0;
    while(1){
        static struct option long_options[] =
        {
            {"debug", no_argument, 0, 'd'},
            {"help", no_argument, 0, 'h'},
            {"prof", required_argument, 0, 'p'},
            {"nb-th-get", required_argument, 0, 't'},
            {"nb-th-analyse", required_argument, 0, 'a'},
            {0, 0, 0, 0}
        };

        while((options = getopt_long(argc, argv, "d: h: p: t: a:", long_options, &option_index))!=-1){
            switch(options){
                case 'd':
                    ModeDebug = 1;
                    break;
                case 'h':
                    usage();
                    exit(1);
                    break;
                case 'p':
                    profondeur = atoi(optarg);
                    if(profondeur==0){
                        fprintf(stderr, "la taille du profondeur %s n'est pas correcte!!!\n", optarg);
                        exit(1);
                    }
                    break;
                case 't':
                    NbThreadsTelecharger = atoi(optarg);
                    if(NbThreadsTelecharger==0){
                        fprintf(stderr, "la taille du nombre de thread à télécharger %s n'est pas correcte!!!\n", optarg);
                        exit(1);
                    }
                    break;
                case 'a':
                    NbThreadsAnalyser = atoi(optarg);
                    if(NbThreadsAnalyser==0){
                        fprintf(stderr, "la taille du nombre de thread à analyser %s n'est pas correcte!!!\n", optarg);
                        exit(1);
                    }
                    break;
                default :

                    usage();
                    exit(1);
            }

        }
        break;

    }
    if(argc>=3)
    {
        char * url = malloc((strlen(argv[optind]) + 1) * sizeof(char));
        char * path = malloc((strlen(argv[optind+1]) + 1) * sizeof(char));
        strcpy(url, argv[optind]);
        strcpy(path, argv[optind+1]);
        mkdir(path, 0777);
        chdir(path);
        httpgetargs argument;
        cellule * c;
        c = malloc(sizeof(cellule));
        c->path = malloc(strlen(url + 1) * sizeof(char));
        strcpy(c->path, url);
        c->prof = profondeur;
        file_telecharger= g_queue_new();
        g_queue_push_tail(file_telecharger, c);
        int ret = 0;
        pageweb arpg;
        arpg.thread_telecharger = (pthread_t *)malloc(NbThreadsTelecharger*sizeof(pthread_t));
        int i;
        for(i=0;i<NbThreadsTelecharger;i++){
            ret = pthread_create (&arpg.thread_telecharger[i], NULL, telecharger, (void *)&argument);
            if (ret)
            {
                fprintf (stderr, "%s", strerror (ret));
            }
        }
        i = 0;
        if(!ret)
        {
            printf ("Creation des threads Analyse !\n");
            for(i=0;i<NbThreadsAnalyser;i++){
                ret = pthread_create (&arpg.thread_analyser[i], NULL, analyser, (void *)&argument);
                if (ret)
                {
                    fprintf (stderr, "%s", strerror (ret));
                }
            }
        }
        else
        {
            fprintf (stderr, "%s", strerror (ret));
        }
        i = 0;
        for(i=0;i<NbThreadsTelecharger;i++){
            pthread_join (arpg.thread_telecharger[i], NULL);
        }
        i=0;
        for(i=0;i<NbThreadsAnalyser;i++){
            pthread_join (arpg.thread_analyser[i], NULL);
        }
        free(arpg.thread_telecharger);
        free(c);
        free(url);
        free(path);
    }
    else
    {
        usage();
        exit(1);
    }
    return 0;
}
