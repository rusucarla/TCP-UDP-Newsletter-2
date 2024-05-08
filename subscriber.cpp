
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "common.h"

// Vreau socket-ul TCP pentru a comunica cu serverul
int tcp_socket = -1;
// Vreau sa am un numar de socket-uri pentru a face polling
int numar_sockets = 2;
// Vreau sa am un vector de pollfd pentru a face polling : lista de socket-uri :
// unul pentru tastatura si unul pentru server
struct pollfd socket_list[2];
// Vreau sa am un id pentru client
char *client_id;
// Vreau sa am un buffer_tastatura pentru a citi de la tastatura comenzile
char buffer_tastatura[BUFSIZ];
// O valoarea pentru return-uri pentru DIE-uri
int ret;

void run_client() {
    // Structura pentru a trimite un pachet catre server
    // care va fi header-ul (tipul mesajului, in cazul asta doar comenzi date si
    // doar mesaje udp primite) si content-ul (topic_packet)
    struct packet am_dat_pachet;

    while (1) {
        // Golesc buffer-ul de la tastatura
        memset(buffer_tastatura, 0, BUFSIZ);
        ret = poll(socket_list, numar_sockets, -1);
        DIE(ret < 0, "poll");

        for (int i = 0; i < numar_sockets; i++) {
            if (socket_list[i].revents & POLLIN) {
                if (socket_list[i].fd == STDIN_FILENO) {
                    // Daca am primit ceva de la tastatura, citesc comanda
                    std::cin.getline(buffer_tastatura, sizeof(buffer_tastatura));

                    // Daca am primit o comanda goala, o ignor
                    if (strlen(buffer_tastatura) == 0) {
                        continue;
                    }
                    // Daca am ce citi de la tastatura, trebuie sa scot
                    // newline-ul de la finalul comenzii
                    if (buffer_tastatura[strlen(buffer_tastatura) - 1] == '\n') {
                        buffer_tastatura[strlen(buffer_tastatura) - 1] = '\0';
                    }

                    // Extragem comanda din buffer-ul de la tastatura
                    // structura comenzii este: subscribe/unsubscribe <TOPIC>
                    // sau exit
                    char *comanda_buffer = strtok(buffer_tastatura, " ");

                    // Verific daca am primit o comanda valida
                    comanda comanda = determinare_comanda(comanda_buffer);

                    switch (comanda) {
                        case SUBSCRIBE:
                        case UNSUBSCRIBE: {
                            // Daca am primit subscribe/unsubscribe, trebuie sa
                            // vad si topic-ul asociat
                            char *topic = strtok(NULL, " ");
                            // Verific daca am primit un topic valid
                            if (topic == NULL || strtok(NULL, " ") != NULL || strlen(topic) > 50) {
                                break;
                            }
                            // Voi trimite un pachet de tip subscribe/unsubscribe
                            struct topic_packet topic_pachet;
                            strcpy(topic_pachet.topic, topic);
                            strcpy(topic_pachet.client_id, client_id);
                            if (comanda == SUBSCRIBE) {
                                strcpy(topic_pachet.comanda, "subscribe");
                                // Afisez mesajul corespunzator cerintei
                                std::cout << "Subscribed to topic " << topic << "\n";
                            } else if (comanda == UNSUBSCRIBE) {
                                strcpy(topic_pachet.comanda, "unsubscribe");
                                // Afisez mesajul corespunzator cerintei
                                std::cout << "Unsubscribed from topic " << topic << "\n";
                            }
                            // Completez pachetul cu informatiile necesare
                            complete_packet(&am_dat_pachet, COMANDA_VALIDA, sizeof(topic_pachet), (char *)&topic_pachet);
                            // Trimit pachetul catre server
                            ret = send_all_packet(socket_list[1].fd, &am_dat_pachet);
                            DIE(ret <= 0, "Ceva nu a mers bine la send");
                            break;
                        }
                        case EXIT: {
                            // Consruim un pachet de tip exit pentru a anunta
                            // serverul de intentiile mele
                            struct topic_packet exit_pachet;
                            strcpy(exit_pachet.comanda, "exit");
                            strcpy(exit_pachet.client_id, client_id);
                            complete_packet(&am_dat_pachet, COMANDA_VALIDA, sizeof(exit_pachet), (char *)&exit_pachet);
                            // Trimit pachetul de tip exit catre server
                            ret = send_all_packet(tcp_socket, &am_dat_pachet);
                            DIE(ret <= 0, "send");
                            break;
                        }
                    }
                    continue;
                } else if (socket_list[i].fd == tcp_socket) {
                    // Daca am primit ceva de la server, citesc pachetul
                    struct packet mia_dat_pachet;
                    ret = recv_all_packet(tcp_socket, &mia_dat_pachet);
                    DIE(ret <= 0, "Ceva nu a mers bine la recv");
                    if (mia_dat_pachet.header.tip_mesaj == NUI_COMANDA) {
                        std::cout << mia_dat_pachet.content << "\n";
                    }
                    if (mia_dat_pachet.content != NULL)
                        free(mia_dat_pachet.content);
                    continue;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != 4, "Format corect: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>");

    // Adaugam STDIN_FILENO la lista de socket-uri pentru a putea citi de la
    // tastatura comenzile (exit, subscribe, unsubscribe)
    socket_list[0].fd = STDIN_FILENO;
    socket_list[0].events = POLLIN;

    // Extragem id-ul clientului
    client_id = argv[1];
    DIE(strlen(client_id) > 10 || strlen(client_id) == 0, "Format incorect pentru ID_CLIENT");

    // Extragem mai intai portul serverului, pentru a putea initializa serverul
    // cu portul corect
    // Verific daca port-ul este un numar
    DIE(!is_number(argv[3]), "Format incorect pentru PORT_SERVER");
    uint16_t port = atoi(argv[3]);

    // Initializam serverul cu portul primit ca argument
    struct sockaddr_in adresa_server = init_server(port);
    // Cu adresa IP din argumentul 2, completam adresa serverului
    ret = inet_aton(argv[2], &adresa_server.sin_addr);
    DIE(ret <= 0, "Format incorect pentru IP_SERVER (inet_pton)");

    // Deschidem un socket TCP pentru a comunica cu serverul
    tcp_socket = init_tcp_client(adresa_server);

    socket_list[1].fd = tcp_socket;
    socket_list[1].events = POLLIN;

    // Ma prezin serverului
    struct packet am_dat_pachet;
    complete_packet(&am_dat_pachet, NUI_COMANDA, strlen(client_id), client_id);
    ret = send_all_packet(tcp_socket, &am_dat_pachet);
    DIE(ret <= 0, "send");

    run_client();

    // Inchidem conexiunea cu serverul
    close(tcp_socket);

    return 0;
}