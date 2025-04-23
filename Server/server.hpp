// game_server.h
#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include <iostream>
#include <dirent.h>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <chrono>
#include <thread>  
#include <ctime>
#include <sstream>
#include <sys/stat.h>   
#include <sys/types.h> 
#include <cstdio> 
#include <iomanip>
#include <map>
#include <signal.h>
#include <shared_mutex>
#include <mutex>

#define PORT 58047
#define MAXBUF 1024
#define MAX_TRIALS 8
#define COLORS "RGBYOP"
#define SCOREBOARD_FILE "scoreboard.txt"
#define TRIALS_FILE "trials_%s.txt"
#define MODE_PLAY 1
#define MODE_DEBUG 2

using namespace std;

// Define a struct to hold game state
struct Game {
    string plid;                 // Player ID (6 digits)
    bool is_active = false;
    char mode;
    string start_date;
    time_t start_time;  
    int trials = 0;
    string secret_key;           // 4 colors
    vector<string> trial_history; // Trial history
    int max_playtime = 0;
    bool timer_expired = false;

    void print() const {
        cout << "Game Details:" << endl;
        cout << "  plid: " << plid << endl;
        cout << "  is_active: " << (is_active ? "true" : "false") << endl;
        cout << "  mode: " << mode << endl;
        cout << "  start_date: " << start_date << endl;
        cout << "  start_time: " << ctime(&start_time); // Converts to human-readable format
        cout << "  trials: " << trials << endl;
        cout << "  secret_key: " << secret_key << endl;
        cout << "  trial_history: ";
        if (trial_history.empty()) {
            cout << "None";
        } else {
            for (const auto& trial : trial_history) {
                cout << trial << " ";
            }
        }
        cout << endl;
        cout << "  max_playtime: " << max_playtime << " seconds" << endl;
    }
};

// Structure to hold score information
struct SCORELIST {
    int scores[10];
    string PLID[10];
    string colcode[10];
    int no_tries[10];
    int mode[10];
    int n_scores;
};

// Global game instance
extern map<string, Game> games; 
extern shared_mutex map_mutex;
extern shared_mutex scoreboard_mutex;
extern shared_mutex games_mutex;
extern bool verbose;    

// Function declarations
bool isValidPLID(const char* plid);
bool isValidPlaytime(int time_available);
void sendMessageUDP(int sockfd, const char* message, struct sockaddr* client_addr, socklen_t addr_len);
void initializeGame(Game& game, const string& plid, int time_available);
void processSNGCommand(int udp_sockfd, const char* buffer, struct sockaddr_in client_addr, socklen_t addr_len, map<string, Game>& games, shared_mutex& game_mutex);
void handle_game_end(Game& game, int udp_sockfd, sockaddr_in& client_addr, socklen_t addr_len, char status, int black, int white);
void processTRYCommand(const char* buffer, int udp_sockfd, sockaddr_in& client_addr, socklen_t addr_len, map<string, Game>& games);
void processQUTCommand(int udp_sockfd, char* buffer, struct sockaddr_in client_addr, socklen_t addr_len);
void initializeGameDebug(Game& game, const string& plid, int time_available, const string& guess);
void processDBGCommand(int udp_sockfd, const char* buffer, struct sockaddr_in client_addr, socklen_t addr_len, map<string, Game>& games, shared_mutex& game_mutex);
void sendMessageTCP(int client_sockfd, const char *message);
void processSTRCommand(int client_sockfd, const char *buffer, struct sockaddr_in client_addr);
void processSSBCommand(int sockfd, struct sockaddr_in client_addr);
void verbose_print(const string& message);
pair<string, int> getClientIPAndPort(const struct sockaddr_in& client_addr);
bool has_ongoing_game(const string& plid);
int FindLastGame(const char* PLID, char* fname);
int CompareDescending(const struct dirent** a, const struct dirent** b);
int FindTopScores(SCORELIST* list);
string getFormattedTime();
int secondsSinceStartDate(const time_t start_time);
void generate_secret_key(string& key);
void calculate_feedback(const string& guess, const string& secret, int& black, int& white);
void save_trials(const Game& game, bool start);
void send_trials(int client_sockfd, const string& plid, Game& game, int time_left);
void save_game(const Game& game, char code);
bool is_duplicate_try(const string& buffer, const vector<string>& trial_history);
void save_score(const Game& game);

#endif // GAME_SERVER_H
