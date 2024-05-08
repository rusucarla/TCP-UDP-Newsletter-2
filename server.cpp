#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <unordered_map>

#include "./client_class.cpp"  // <- De aici am importat clasa Client
#include "common.h"

// Vreau sa am o lista cu toti clientii conectati
// care vor fi retinuti in perechea ID - Client
std::unordered_map<std::string, Client *> client_lista;

// Vreau socket-ul pentru conexiunea TCP
int tcp_connect;

// Vreau socket-ul pentru conexiunea UDP
int udp_connect;

// Numarul de sockets deschisi (STDIN, UDP, TCP, clientii TCP)
// incep cu 3 pentru conexiunile STDIN, UDP si listener TCP 
int numar_sockets = 3;

// Lista de socketi
struct pollfd socket_list[CLIENTS_MAXSIZE + 3];

// O valoarea pentru return-uri pentru DIE-uri
int ret;

// Functie pentru a gestiona evenimente STDIN (in cazul acestei teme, exit)
void gestiune_STDIN() {
    // Incepem de la 3 pentru ca primele 3 socket-uri sunt STDIN, UDP si TCP
    // si noi vrem la inchiderea serverului sa inchidem toate conexiunile cu
    // clientii TCP (subscriberii cu fd > 2)
    for (int i = 3; i < numar_sockets; i++) {
        int current_fd = socket_list[i].fd;  // Descriptorul de socket curent

        // Parcurgem lista de clienti pentru a gasi clientul cu socket-ul corespunzator
        for (auto it = client_lista.begin(); it != client_lista.end(); ++it) {
            if (it->second->socket == current_fd) {
                delete it->second;       // Sterg (eliberez memoria) clientul
                client_lista.erase(it);  // Sterg clientul din lista de clienti
                break;                   // Ies din bucla
            }
        }
    }
}

// functie pentru a gestiona evenimente UDP
// primire mesaj de la clientul UDP + parsare mesaj + trimitere mesaj la
// clientii TCP abonati
void gestiune_UDP() {
    struct udp_packet udp_packet;
    memset(&udp_packet, 0, sizeof(udp_packet));

    struct sockaddr_in udp_client;
    socklen_t len = sizeof(udp_client);

    ret = recvfrom(udp_connect, &udp_packet, sizeof(udp_packet), 0, (struct sockaddr *)&udp_client, &len);
    DIE(ret <= 0, "recvfrom");

    std::string udp_mesaj = constuire_mesaj_udp(udp_packet, udp_client);
    // Verificam daca mesajul este gol (eroare la parsare)
    if (udp_mesaj == "") {
        return;
    }
    // Caut clientii care sunt abonati la topicul primit
    // si catre care se adresaza mesajul
    // in plus, verificam daca clientul este conectat (socket != -1)
    for (auto &client : client_lista) {
        if (client.second->subscribed_to_topic(udp_packet.topic) && client.second->socket != -1) {
            int socket = client.second->socket;
            struct packet am_dat_pachet;
            // voi pune la size + 1 pentru ca vreau sa trimit si '\0' la final
            complete_packet(&am_dat_pachet, NUI_COMANDA, udp_mesaj.size() + 1, (char *)udp_mesaj.c_str());
            ret = send_all_packet(socket, &am_dat_pachet);
            DIE(ret <= 0, "send");
        }
    }
}

void inchidere_socket(int i) {
    // Inchidem socket-ul
    close(socket_list[i].fd);

    // Stergem socket-ul din lista de socketi
    for (int j = i; j < numar_sockets - 1; j++) {
        socket_list[j] = socket_list[j + 1];
    }

    // Scadem numarul de socketi
    numar_sockets--;
}

void deschidere_socket(int socket) {
    // Adaugam socket-ul in lista de socketi
    socket_list[numar_sockets].fd = socket;
    // Vrem sa citim de pe socket
    socket_list[numar_sockets].events = POLLIN;
    socket_list[numar_sockets].revents = 0;
    // Crestem numarul de socketi
    numar_sockets++;
}

bool gasire_client(std::string id_client) {
    auto it = client_lista.find(id_client);
    if (it == client_lista.end()) {
        return false;
    }
    return true;
}

void run_server() {
    while (1) {
        DIE(poll(socket_list, numar_sockets, -1) < 0, "eroare la poll");
        struct packet mia_dat_pachet;
        for (int i = 0; i < numar_sockets; i++) {
            if (socket_list[i].revents & POLLIN) {
                if (socket_list[i].fd == STDIN_FILENO) {
                    // Ne oprim daca citim exit de la tastatura
                    std::string buffer_tastatura;
                    getline(std::cin, buffer_tastatura);

                    if (buffer_tastatura == "exit") {
                        // Inchidem serverul
                        gestiune_STDIN();
                        return;
                    }
                    continue;  // Daca nu este exit, trecem peste
                } else if (socket_list[i].fd == udp_connect) {
                    gestiune_UDP();
                } else if (socket_list[i].fd == tcp_connect) {
                    // Vrem sa acceptam conexiuni de la clientii TCP
                    struct sockaddr_in adresa_client;
                    socklen_t length_client = sizeof(adresa_client);

                    int socket_client = accept(tcp_connect, (struct sockaddr *)&adresa_client, &length_client);
                    DIE(socket_client < 0, "Este o eroare la acceptare");

                    int enable = 1;
                    int rc = setsockopt(socket_client, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
                    DIE(rc < 0, "setsockopt(IPPROTO_TCP, TCP_NODELAY) a esuat");

                    ret = recv_all_packet(socket_client, &mia_dat_pachet);
                    DIE(ret <= 0, "recv_all");

                    // Eu din subscriber, trimit un pachet cu ID-ul meu
                    // si in content-ul pachetului este doar ID-ul meu
                    std::string id_client = std::string(mia_dat_pachet.content, mia_dat_pachet.header.length);

                    bool client_exista = gasire_client(id_client);
                    if (client_exista) {
                        // Daca clientul este deja conectat, afisam un mesaj si inchidem conexiunea
                        if (client_lista[id_client]->socket != -1) {
                            std::cout << "Client " << id_client << " already connected.\n";
                            close(socket_client);
                            continue;
                        }
                        // Daca clientul s-a reconectat, ii actualizam socket-ul
                        client_lista[id_client]->socket = socket_client;
                    } else {
                        // Daca clientul este nou, il adaugam in lista de clienti
                        Client *client_nou = new Client(socket_client);
                        DIE(client_nou == nullptr, "client_nou");
                        client_lista.insert({id_client, client_nou});
                    }
                    // Adaugam socket-ul clientului in lista de socketi
                    deschidere_socket(socket_client);

                    // Afisez mesajul de conectare conform cerintei
                    std::cout << "New client " << id_client << " connected from " << inet_ntoa(adresa_client.sin_addr) << ":" << ntohs(adresa_client.sin_port) << ".\n";
                } else {
                    // Primim comenzi de la clientii TCP
                    ret = recv_all_packet(socket_list[i].fd, &mia_dat_pachet);
                    DIE(ret <= 0, "recv_all_packet");
                    // SUBSCRIBE SAU UNSUBSCRIBE SAU EXIT
                    if (mia_dat_pachet.header.tip_mesaj == COMANDA_VALIDA && mia_dat_pachet.header.length == sizeof(topic_packet)) {
                        // Deschidem pachetul (papusa matrioska) si il parsam
                        struct topic_packet *topic_packet = (struct topic_packet *)mia_dat_pachet.content;

                        // Verificam daca clientul exista
                        bool client_exista = gasire_client(topic_packet->client_id);

                        if (!client_exista) {
                            continue;
                        } else {
                            // Daca clientul exista, il gasim in map-ul de clienti
                            Client *client = client_lista[topic_packet->client_id];
                            // Verificam daca comanda este una valida
                            comanda comanda = determinare_comanda(topic_packet->comanda);
                            switch (comanda) {
                                case SUBSCRIBE:
                                    client->subscribe_topic(topic_packet->topic);
                                    break;
                                case UNSUBSCRIBE:
                                    client->unsubscribe_topic(topic_packet->topic);
                                    break;
                                case EXIT:
                                    // Afisam mesajul de deconectare conform cerintei
                                    std::cout << "Client " << topic_packet->client_id << " disconnected.\n";
                                    client->socket = -1;
                                    client_lista[topic_packet->client_id]->socket = -1;
                                    inchidere_socket(i);
                                    break;
                                case ALTCEVA:
                                    // Daca nu este niciuna din cele 3 comenzi valide, trecem mai departe
                                    continue;
                            }
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    // Verific daca comanda a fost data corect
    DIE(argc != 2, "Cum trebuie sa arate comanda: ./server <PORT_DORIT>");

    // Adaugam STDIN in lista de socketi
    socket_list[0].fd = STDIN_FILENO;
    socket_list[0].events = POLLIN;

    // Parsam port-ul ca un numar
    DIE(!is_number(argv[1]), "Format incorect pentru PORT_DORIT");
    uint16_t port = atoi(argv[1]);

    // Pentru portul dat ca argument, initializam serverul
    struct sockaddr_in serv_addr = init_server(port);

    // Obtinem un socket TCP pentru receptionarea conexiunilor
    tcp_connect = init_tcp_connection(serv_addr);

    socket_list[2].fd = tcp_connect;
    socket_list[2].events = POLLIN;

    // Obtinem un socket UDP pentru receptionarea mesajelor
    udp_connect = init_udp_connection(serv_addr);

    socket_list[1].fd = udp_connect;
    socket_list[1].events = POLLIN;

    run_server();

    // Inchid socket-ii (STDIN, UDP, TCP, etc)
    for (int i = 1; i < numar_sockets; i++) {
        close(socket_list[i].fd);
    }

    return 0;
}