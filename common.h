#ifndef __COMMON_H__
#define __COMMON_H__

#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>

#include <complex>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#define DIE(assertion, call_description)                       \
    do {                                                       \
        if (assertion) {                                       \
            fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__); \
            perror(call_description);                          \
            exit(EXIT_FAILURE);                                \
        }                                                      \
    } while (0)

#define MESAJ_MAXSIZE 1500
#define TOPIC_MAXSIZE 50
// Am ales pentru a trimite mesajul UDP sa fie de maxim 1700 de octeti
// 1500 din mesaj + 50 din topic + 10 (1 din tipul de date care poate fi INT,
// SHORT_REAL, FLOAT, STRING -> adica maxim 10 caractere) + 15 pentru IP + 5
// pentru PORT + cateva octeti pentru spatii, liniute, etc => 1700 cu un
// overhead pentru siguranta
#define UDP_MAXSIZE 1700
// Maxim 10 caractere ASCII
#define ID_CHAR_MAXSIZE 10
// Am ales un numar de maxim 1000 de clienti
#define CLIENTS_MAXSIZE 1000
// = strlen("unsubscribe")
#define TCP_COMMAND_SIZE 11
#define NUI_COMANDA 0
#define COMANDA_VALIDA 1
// Am un pachet care transporta mesaje intre server si clienti
// Acesta are un header si un content (payload)
// Header-ul are tipul mesajului si lungimea payload-ului
// Tipul mesajului poate fi:
// 0 - connection request sau afisare mesaj (NUI_COMANDA)
// 1 - topic_packet struct (COMANDA_VALIDA)
struct header {
    uint8_t tip_mesaj;
    uint16_t length;
};
struct packet {
    struct header header;
    char *content;
};
// Un payload pentru un mesaj UDP are un topic, un tip de date si un continut
// Tipul de date poate fi:
// 0 - INT
// 1 - SHORT_REAL
// 2 - FLOAT
// 3 - STRING
struct udp_packet {
    char topic[TOPIC_MAXSIZE];
    uint8_t tip_date;
    char content[MESAJ_MAXSIZE + 1];
};
// Un payload pentru un mesaj TCP are o comanda, un topic si un client_id
// Comanda poate fi (cum este prezentat mai jos in enum comanda):
// 0 - subscribe
// 1 - unsubscribe
// 2 - exit
struct topic_packet {
    char comanda[TCP_COMMAND_SIZE + 1];
    char topic[TOPIC_MAXSIZE + 1];
    char client_id[ID_CHAR_MAXSIZE + 1];
};
enum comanda {
    SUBSCRIBE,
    UNSUBSCRIBE,
    EXIT,
    ALTCEVA
};
// Astea-s functii pe care le-am implementat in laboratorul 7
int send_all(int sockfd, void *buffer, size_t len);
int send_all_packet(int sockfd, struct packet *sent_packet);
int recv_all_packet(int sockfd, struct packet *recv_packet);
int recv_all(int sockfd, void *buffer, size_t len);

struct sockaddr_in init_server(uint16_t port);
int init_tcp_connection(struct sockaddr_in serv_addr);
int init_udp_connection(struct sockaddr_in serv_addr);
int init_tcp_client(struct sockaddr_in client_addr);

std::string constuire_mesaj_udp(struct udp_packet message, sockaddr_in udp_client);

void complete_packet(struct packet *packet, uint8_t tip_mesaj, uint16_t len, char *content);

comanda determinare_comanda(char *comanda);
int is_number(const char *str);
#endif
