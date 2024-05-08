# PCOM TEMA 2 : TCP UDP

## Descrierea implementarii

Baza temei a fost experienta dobandita in laboratorul 7, unde am avut de
imbunatatit un server TCP de comunicare intre 2 clienti.

De acolo am inteles principiile de baza ale multiplexarii.

### Structurile folosite

Am vrut sa fac o implementare a transportului de date cat mai asemanatoare cu o
papusa matroska, pentru a semana cu realitatea.

Diagrama de mai jos arata structura packet, care este folosita pentru a
transporta datele intre clienti si server:

``` markdown
+--------------+-----------------------+
|  tip_mesaj   |       length          |
+--------------+-----------------------+
|                 content              |
+--------------------------------------+
```

Tipul mesajului este un "flag" (in realitate e un `uint8_t`) care este folosit
pentru a diferentia intre un mesaj de la un client TCP care vrea sa transmita
comenzile catre server si orice alt fel de mesaj.

Astfel ca, am creat doua constante `NUI_COMANDA` si `COMANDA_VALIDA` care sa fie
folosite cand se trimite un pachet pentru a fi usor de inteles ce se doreste.

Content-ul pachetului este payload care contine la randul lui un alt pachet,
fie el de tipul `UDP` sau `TCP`.

Diagrama pentru un pachet de tipul `UDP` este urmatoarea:

``` markdown
+------------------+-------------------+-----------------------+
|      topic       |     tip_date      |        content        |
+------------------+-------------------+-----------------------+
```

Diagrama pentru un pachet de tipul `TCP` este urmatoarea:

``` markdown
+--------------+------------------+-----------------------+
|   comanda    |      topic       |      client_id        |
+--------------+------------------+-----------------------+
```

In plus, am mai folosit o enumeratie pentru a defini tipurile de comenzi care
pot fi trimise de la client catre server.

### Server-ul

Server-ul este implementat in fisierul `server.cpp`. Acesta este construit cu
constrangerile prezentate in enuntul temei:

- Modul in care se da comanda (`./server <PORT_DORIT>`) pentru care se verifica daca
  se primeste un numar acceptabil de argumente si daca portul este valid.
- Mesajele afisate (pentru conectarea/deconectarea unui client) sunt afisate
  pe `stdin`.
- Pentru comanda `exit` se inchide server-ul, se elibereaza resursele si se
  inchid si clientii TCP.

In rest, server-ul urmareste modeulul celui din laboratorul 7, multiplexarea
fiind realizata cu poll.

Pentru a tine cont de clienti si starea lor de activitate, am folosit un map in
care sa retin informatii despre fiecare client:

- `client_id` - id-ul clientului
- `Client *` - un pointer catre o instanta a clasei `Client` care contine toate
  informatiile necesare pentru a comunica cu clientul

In functie de ce fel de eveniment se intampla, server-ul va face urmatoarele:

- Daca primeste un eveniment de la `STDIN`, atunci se verifica daca se primeste
  comanda `exit` si se inchide server-ul.
- Daca primeste un eveniment de la `clientul UDP`, atunci se verifica daca se
  primeste un mesaj de tipul `UDP` si se trimite mesajul catre clientii TCP
  care sunt abonati la topic-ul respectiv.
- Daca primeste un eveniment de la `socket-ul TCP`, atunci se verifica ce client a
  trimis mesajul si se verifica daca este un client cunoscut sau nu.
- Daca primeste un primeste un mesaj de la un `client TCP`, atunci se verifica
  daca este o comanda valida si se salveaza informatiile despre client.

### Clasa Client

Pentru ca am decis sa lucrez intr-un limbaj orientat pe obiect, am decis sa
profit de aceasta oportunitate si sa creez o clasa `Client` care sa contina
toate metodele necesare pentru a rezolva toate comenzile care vin de la clienti.

Client-ul are 3 atribuite:

- un map care contine informatii despre topic-urile fara wildcard-uri la care
  este abonat
- un alt map care contine informatii despre topic-urile cu wildcard-uri la care
  este abonat
- un socket pe care se comunica cu clientul respectiv

Pentru a satisfice cerintele temei, am implementat urmatoarele metode:

- `subscribe_topic`
- `unsubscribe_topic`
- `subscribed_to_topic`

Mai sunt si metodele care ajuta la parsarea topic-urilor pentru a verifica daca
clientul este abonat la un anumit topic dintr-un mesaj de tipul `UDP`.

Pentru a verifica abonarea la un topic cu wildcard-uri, am constuit mai multe
metode auxiliare :

- `match` : care imparte topic-ul cautat si pattern-ul cu wildcard in niste
  liste de string-uri si verifica daca acestea se potrivesc
- `split` : care imparte un string dupa delimitatorul `/` si returneaza o lista
  de string-uri
- `match_pe_bucati` : care cu ajutorul a 2-a index-uri, verifica daca se
  potrivesc string-urile (topic-ul cautat si pattern-ul cu wildcard)

### Client-ul TCP

Client-ul TCP este implementat in fisierul `subscriber.cpp`. Acesta este
construit dupa modelul clientului din laboratorul 7. In plus, am fost atenta sa
respect constrangerile temei:

- Modul in care se da comanda
  (`./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>`) pentru care se
  verifica daca se primeste un numar acceptabil de argumente si daca portul este
  valid
- Afisarea mesajelor de subscribe/unsubscribe se face pe `stdout`
- Afisarea mesajelor de la server se face in formatul cerut de cerinta :
  `<IP_CLIENT_UDP>:<PORT_CLIENT_UDP> - <TOPIC> - <TIP_DATE> - <VALOARE_MESAJ>`

La clientul TCP am folosit doar 2 socket-uri, unul pentru a comunica cu
server-ul si altul pentru a primi comenzile de la tastatura.

Mesajele de la tastatura sunt interpretate cu ajutorul functiei
`determinare_comanda` care verifica ce tip de comanda se primeste.

Daca comanda este una valida, atunci se pregateste un pachet pentru a fi
transmis. Mai intai se completeaza pachetul de tip `topic_packet` cu
informatiile necesare si mai apoi se completeaza pachetul de tip `packet` cu
optiunea `COMANDA_VALIDA` pentru a fi usor de inteles ce se doreste.

Pentru mesajele care vin de la server, se verifica daca pachetul are tipul
`NUI_COMANDA` si se afiseaza mesajul in formatul cerut de cerinta.

Formatul mesajului este construit cu ajutorul functiei `construire_mesaj_udp`
care primeste un pachet udp si returneaza un string cu mesajul formatat conform
cerintei.

### Dezactivare Nagle

Urmarind hint-ul din cerinta, am folosit optiunea `TCP_NODELAY` pentru a
dezactiva algoritmul Nagle. Aceasta optiune am folosit-o in momentul in care se
conecteaza un client TCP la server.

### Alte observatii

Am observat ca in functia `construire_mesaj_udp` pentru cazul in care am tipul
de date `FLOAT` sau `INT` si valoarea `0.0` nu se afisa corect valoarea uneori.
Am rezolvat prin a verifica in plus daca numarul este diferit de `0` sau `0.0`,
pe langa verificarea valorii de semn.
