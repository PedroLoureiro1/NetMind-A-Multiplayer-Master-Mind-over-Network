// client.hpp
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <arpa/inet.h>
#include <mutex>
#include <thread>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include <sys/select.h>
#include <sstream>

#define MAXBUF 1024
#define DEFAULT_PORT 58047
#define TIMEOUT_SEC 1
#define RESEND_TIMEOUT 3
#define MAX_TIMEOUT 5
#define COLORS "RGBYOP"

using namespace std;

// Estrutura para armazenar o estado do cliente
struct ClientState {
    int plid;             // Identificador do jogador (PLID)
    int num_tries = 1;    // Número de tentativas feitas
    int max_playtime;
    bool game_active = false;
    int num_showtrials = 0;
    int num_scoreboard = 0;
};

// Funções de gerenciamento do jogo
void usage();
int open_udp_socket();
int create_tcp_socket();
bool send_and_receive_with_timeout(int sockfd, const char* message, struct sockaddr_in& server_addr, socklen_t addr_len, char* response);
void handle_start(const string& command, sockaddr_in server_addr, socklen_t addr_len, ClientState& state);
void handle_try(const string& command, sockaddr_in server_addr, socklen_t addr_len, ClientState& state);
void handle_show_trials(const char* server_ip, int port, ClientState& state);
void handle_scoreboard(const char* server_ip, int port, ClientState& state);
void handle_quit(sockaddr_in server_addr, socklen_t addr_len, ClientState& state);
void handle_exit(sockaddr_in server_addr, socklen_t addr_len, ClientState& state);
void handle_debug(const string& command, sockaddr_in server_addr, socklen_t addr_len, ClientState& state);

#endif // CLIENT_HPP
