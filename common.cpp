#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstring>

int recv_all_packet(int sockfd, struct packet *recv_packet) {
    // Mai intai primim header-ul pachetului
    int ret = recv_all(sockfd, &recv_packet->header, sizeof(struct header));
    DIE(ret <= 0, "recv_all");
    // Aloc spatiu pentru content (payload-ul pachetului)
    recv_packet->content = new char[recv_packet->header.length];
    // Apoi primim content-ul pachetului
    ret = recv_all(sockfd, recv_packet->content, recv_packet->header.length);
    DIE(ret <= 0, "recv_all");
    return ret;
}

int recv_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = (char *)buffer;

    while (bytes_remaining > 0) {
        ssize_t result = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        DIE(result < 0, "Eroare la recv");

        if (result == 0) {
            // Conexiunea a fost inchisa de cealalta parte, ar putea fi o problema daca ne asteptam la mai multe date
            break;
        }
        // Actualizeaza numarul de octeti primiti
        bytes_received += result;
        // Reduce numarul de octeti de care mai avem nevoie
        bytes_remaining -= result;
    }

    return bytes_received;
}

int send_all_packet(int sockfd, struct packet *sent_packet) {
    // Trimit mai intai header-ul pachetului
    int ret = send_all(sockfd, &sent_packet->header, sizeof(struct header));
    DIE(ret <= 0, "send_all");  // Verific ca s-a trimis cu succes
    // Apoi trimit content-ul pachetului
    ret = send_all(sockfd, sent_packet->content, sent_packet->header.length);
    DIE(ret <= 0, "send_all");  // Verific ca s-a trimis cu succes
    // Eliberez memoria alocata pentru content, ca nu mai am nevoie de ea
    free(sent_packet->content);
    return ret;
}

int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = (char *)buffer;

    while (bytes_remaining > 0) {
        ssize_t result = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        DIE(result < 0, "Eroare la send");
        // Actualizeaza numarul de octeti trimisi
        bytes_sent += result;
        // Reduce numarul de octeti ramasi de trimis
        bytes_remaining -= result;
    }
    return bytes_sent;
}

// functie pentru a initializa serverul
struct sockaddr_in init_server(uint16_t port) {
    struct sockaddr_in serv_addr;
    // Initializam structura sockaddr_in pentru server
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;    // setam familia de adrese ca fiind IPv4
    serv_addr.sin_port = htons(port);  // setam portul serverului
    return serv_addr;
}

// functie pentru a initializa socket-ul TCP pentru receptionarea conexiunilor
int init_tcp_connection(struct sockaddr_in serv_addr) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "nu am putut defini socket tcp");
    // Setam optiunea SO_REUSEADDR pentru socket
    int enable = 1;
    DIE(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0, "setsockopt a esuat (TCP/REUSEADDR)");
    // Asociem adresa serverului cu socketul creat folosind bind
    int rc = bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind tcp");
    // Ascultam pentru conexiuni pe socketul TCP
    rc = listen(sockfd, CLIENTS_MAXSIZE - 2);
    DIE(rc < 0, "listen tcp");

    return sockfd;
}

// functie pentru a initializa socket-ul UDP pentru receptionarea mesajelor
int init_udp_connection(struct sockaddr_in serv_addr) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockfd < 0, "nu am putut defini socket udp");
    // Setam optiunea SO_REUSEADDR pentru socket
    int enable = 1;
    DIE(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0, "setsockopt a esuat (UDP/REUSEADDR)");
    // Asociem adresa serverului cu socketul creat folosind bind
    int rc = bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind udp");

    return sockfd;
}

// functie pentru a ne conecta la un socket TCP pentru client (subscriber)
int init_tcp_client(struct sockaddr_in client_addr) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "nu am putut defini socket tcp client");
    // Setam optiunea TCP_NODELAY pentru socket
    int enable = 1;
    DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0, "setsockopt a esuat (tcp client)");
    // Ne conectam la server
    int rc = connect(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr));
    DIE(rc < 0, "connect tcp client");

    return sockfd;
}

std::string constuire_mesaj_udp(struct udp_packet message, sockaddr_in udp_client) {
    char output[UDP_MAXSIZE];

    // Extragem ip-ul si portul clientului UDP
    std::string ip = inet_ntoa(udp_client.sin_addr);
    uint16_t port = ntohs(udp_client.sin_port);
    std::string topic = message.topic;

    // Construiesc mesajul in output
    int offset = snprintf(output, sizeof(output), "%s:%u - %s - ", ip.c_str(), port, topic.c_str());

    if (message.tip_date == 0) {
        // Pentru INT avem un octet de semn urmat de uint32_t in network byte order
        uint8_t semn = message.content[0];
        uint32_t nr = ntohl(*(uint32_t *)(message.content + 1));
        if (semn == 1 && nr != 0) {
            offset += snprintf(output + offset, sizeof(output) - offset, "INT - -%u", nr);
        } else {
            offset += snprintf(output + offset, sizeof(output) - offset, "INT - %u", nr);
        }
    } else if (message.tip_date == 1) {
        // Pentru SHORT_REAL avem un uint16_t in network byte order
        uint16_t nr = ntohs(*(uint16_t *)(message.content));
        // Pe care trebuie sa-l impartim la 100, pentru ca vine ca modulul a numarului inmultit cu 100
        double real = static_cast<double>(nr) / 100.0;
        offset += snprintf(output + offset, sizeof(output) - offset, "SHORT_REAL - %.2f", real);
    } else if (message.tip_date == 2) {
        // Pentru FLOAT avem un octet de semn urmat de uint32_t in network byte
        // order, care reprezinta numarul obtinut din alipirea partii intregi si
        // partii zecimale, urmat de un uint8_t care reprezinta modulul puterii
        // negative a lui 10 cu care trebuie sa inmultim numarul pentru a-l obtine pe cel "original"
        uint8_t semn = message.content[0];
        uint32_t nr = ntohl(*(uint32_t *)(message.content + 1));
        uint8_t power = message.content[5];
        float number_float = static_cast<float>(nr) / std::pow(10.0f, power);
        if (semn == 1 && nr != 0) {
            offset += snprintf(output + offset, sizeof(output) - offset, "FLOAT - -%.*f", power, number_float);
        } else {
            offset += snprintf(output + offset, sizeof(output) - offset, "FLOAT - %.*f", power, number_float);
        }
    } else if (message.tip_date == 3) {
        // Pentru STRING avem maxim 1500 de caractere
        offset += snprintf(output + offset, sizeof(output) - offset, "STRING - %s", message.content);
    } else {
        return "";  // Return empty string if data type is unrecognized
    }

    return std::string(output);  // Convert the buffer to a std::string before returning
}

// functie pentru a completa campurile dintr-un pachet
void complete_packet(struct packet *packet, uint8_t tip_mesaj, uint16_t len, char *content) {
    packet->header.tip_mesaj = tip_mesaj;
    packet->header.length = len;
    packet->content = new char[len];
    memcpy(packet->content, content, len);
}
// Functie pentru a determina comanda primita de la client
comanda determinare_comanda(char *comanda) {
    if (strcmp(comanda, "subscribe") == 0)
        return SUBSCRIBE;
    if (strcmp(comanda, "unsubscribe") == 0)
        return UNSUBSCRIBE;
    if (strcmp(comanda, "exit") == 0)
        return EXIT;
    return ALTCEVA;
}

int is_number(const char *str) {
    // Sărim peste spațiile albe inițiale
    while (*str == ' ') str++;

    // Verificăm dacă restul șirului conține doar cifre
    while (*str) {
        if (!isdigit((unsigned char)*str)) return 0;
        str++;
    }
    return 1;
}
