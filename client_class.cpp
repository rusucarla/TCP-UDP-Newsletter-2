#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

class Client {
   public:
    // Map-uri pentru a retine topic-urile la care este abonat clientul
    std::unordered_map<std::string, bool> topic_without_wildcard;
    std::unordered_map<std::string, bool> topic_with_wildcard;
    int socket;

    // Constructor pentru client
    Client(int socket) {
        this->socket = socket;
    }

    void subscribe_topic(std::string topic) {
        // Verific daca am vreun wildcard in topic (+ sau *)
        if (topic.find("+") != std::string::npos || topic.find("*") != std::string::npos) {
            topic_with_wildcard[topic] = true;
        } else {
            topic_without_wildcard[topic] = true;
        }
    }

    void unsubscribe_topic(std::string topic) {
        // Vreau sa caut topic-ul in ambele map-uri si sterg daca il gasesc
        if (topic_without_wildcard.find(topic) != topic_without_wildcard.end()) {
            topic_without_wildcard.erase(topic);
        }
        if (topic_with_wildcard.find(topic) != topic_with_wildcard.end()) {
            topic_with_wildcard.erase(topic);
        }
    }

    // Pentru a gestiona mai usor cautarea potrivirii cu wildcard-uri, voi
    // desparti string-ul in lista de string-uri folosind drept delimitator '/'
    std::vector<std::string> split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::istringstream iss(str);
        std::string token;
        while (std::getline(iss, token, delim)) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }

    // Functie pentru a verifica daca un topic se potriveste cu un pattern
    // (topic la care este abonat clientul cu wildcard-uri)
    bool match_pe_bucati(const std::vector<std::string>& parti_topic, const std::vector<std::string>& parti_pattern, size_t topicIndex, size_t patternIndex) {
        // Parcurg topic-ul si pattern-ul in acelasi timp
        while (patternIndex < parti_pattern.size() && topicIndex < parti_topic.size()) {
            // Daca gasesc un wildcard '*'
            if (parti_pattern[patternIndex] == "*") {
                // Daca '*' este la final, inseamna ca se potriveste cu orice,
                // pentru ca '*' poate sa inlocuiasca oricate niveluri din topic
                if (patternIndex + 1 == parti_pattern.size()) {
                    return true;
                }
                // Verific daca se potriveste restul pattern-ului cu restul topic-ului
                patternIndex++;
                // Acum cat timp mai am parti din topic, incerc sa gasesc o potrivire
                while (topicIndex < parti_topic.size()) {
                    if (match_pe_bucati(parti_topic, parti_pattern, topicIndex++, patternIndex)) {
                        return true;
                    }
                }
                // Daca nu am gasit o potrivire, inseamna ca nu se potriveste cu pattern-ul
                return false;
            }  // Altfel, daca nu am gasit vreun wildcard si daca partile nu se potrivesc este clar ca nu se potriveste cu pattern-ul
            else if (parti_pattern[patternIndex] != "+" && parti_pattern[patternIndex] != parti_topic[topicIndex]) {
                return false;
            }
            // Nu verific in mod explicit daca pattern-ul contine '+', pentru ca
            // '+' poate inlocui un singur nivel din topic -> astfel ca trec
            // peste si verific urmatoarele parti din topic si pattern
            topicIndex++;
            patternIndex++;
        }
        // Daca am ajuns la finalul topic-ului si pattern-ului inseamna ca se
        // potrivesc si returnez true
        // Daca inca mai sunt parti in topic sau pattern, inseamna ca nu se
        // potriveste perfect si returnez false
        if (topicIndex == parti_topic.size() && patternIndex == parti_pattern.size()) {
            return true;
        } else {
            return false;
        }
    }

    // Functie pentru a verifica daca un topic se potriveste cu un pattern
    bool match(const std::string& topic_cautat, const std::string& pattern_cu_wildcard) {
        std::vector<std::string> parti_topic = split(topic_cautat, '/');
        std::vector<std::string> parti_pattern = split(pattern_cu_wildcard, '/');
        return match_pe_bucati(parti_topic, parti_pattern, 0, 0);
    }

    bool subscribed_to_topic(std::string topic_cautat) {
        // Aici trebuie sa caut topic-ul in ambele map-uri
        // topic-ul nu va avea + sau * in el asa ca trebuie sa fac match pe el
        if (topic_without_wildcard.find(topic_cautat) != topic_without_wildcard.end()) {
            return true;
        }
        // Daca nu se afla in topic_without_wildcard, atunci trebuie sa caut in
        // topic_with_wildcard
        for (const auto& it : topic_with_wildcard) {
            if (match(topic_cautat, it.first)) {
                return true;
            }
        }
        return false;
    }
};