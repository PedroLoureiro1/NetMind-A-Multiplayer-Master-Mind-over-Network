// game_server.c
#include "server.hpp"

void verbose_print(const string& message) {
    if (verbose) {
        cout << message << endl;
    }
}

// Function to get client IP and port from sockaddr_in
pair<string, int> getClientIPAndPort(const struct sockaddr_in& client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN); // Convert IP to string
    int client_port = ntohs(client_addr.sin_port); // Convert port from network byte order to host byte order
    return {string(client_ip), client_port};  
}

// Function to check if a game is ongoing for the given PLID
bool has_ongoing_game(const string& plid) {
    string games_dir = "Server/GAMES/";
    DIR* dir;
    struct dirent* entry;
    {
        shared_lock<shared_mutex> lock(games_mutex); 
        // Open the GAMES directory
        dir = opendir(games_dir.c_str());
        if (!dir) {
            perror("Failed to open GAMES directory");
            return false;
        }

        while ((entry = readdir(dir)) != nullptr) {
            // Skip hidden files and directories (e.g., "." and "..")
            if (entry->d_name[0] == '.') {
                continue;
            }

            // Full path of the current entry
            string full_path = games_dir + entry->d_name;

            // Get file status
            struct stat entry_stat;
            if (stat(full_path.c_str(), &entry_stat) == 0 && S_ISREG(entry_stat.st_mode)) {
                // It's a file, check if the filename contains the PLID
                string filename = entry->d_name;
                if (filename.find(plid) != string::npos) {
                    closedir(dir);
                    return true;
                }
            }
        }
        closedir(dir);
    }
    return false;
}

int FindLastGame(const char* PLID, char* fname) {
    struct dirent** filelist;
    int nentries, found = 0;
    char dirname[64];

    // Create the directory path
    snprintf(dirname, sizeof(dirname), "Server/GAMES/%s/", PLID);

        // Scan the directory and sort entries alphabetically
        nentries = scandir(dirname, &filelist, nullptr, alphasort);
    {
        shared_lock<shared_mutex> lock(games_mutex);
        if (nentries <= 0) {
            return 0; // No files found or directory not accessible
        } else {
            while (nentries--) {
                // Skip entries that start with '.'
                if (filelist[nentries]->d_name[0] != '.') {
                    // Construct the full path of the last valid file
                    snprintf(fname, 512, "%s%s", dirname, filelist[nentries]->d_name);
                    found = 1;
                }
                free(filelist[nentries]); // Free memory for this entry
                if (found) {
                    break; // Stop searching after finding the most recent file
                }
            }
            free(filelist); // Free the list of file entries
        }

        return found;
    }
}
int CompareDescending(const struct dirent** a, const struct dirent** b) {
    // Extract the numerical score from the filename (assuming filename starts with the score as an integer)
    int score_a = atoi((*a)->d_name); // Convert the first part of the filename to an integer
    int score_b = atoi((*b)->d_name); // Convert the first part of the filename to an integer

    // Return the comparison in reverse order for descending sort
    return score_a - score_b;
}

int FindTopScores(SCORELIST* list) {
    struct dirent** file_list;
    int n_entries, i_file = 0;
    char filename[300];
    FILE* file;
    char temp_PLID[8];
    char temp_colorcode[8];
    char c1, c2, c3, c4;
    char mode[8]; // Temporary buffer to hold the mode string

    // Scan the "SCORES/" directory
    n_entries = scandir("Server/SCORES/", &file_list, 0, CompareDescending);
    if (n_entries < 0) {
        return 0; // No files found or error scanning directory
    } else {
        while (n_entries--) {
            // Skip hidden files (those starting with '.')
            if (file_list[n_entries]->d_name[0] != '.') {
                // Build the file path
                snprintf(filename, sizeof(filename), "Server/SCORES/%s", file_list[n_entries]->d_name);
                
                // Open the file for reading
                file = fopen(filename, "r");
                if (file != nullptr) {
                    // Read data from the file and store it in the SCORELIST structure
                    fscanf(file, "%d %s %c %c %c %c %d %s",
                           &list->scores[i_file],
                           temp_PLID,  // Temporary writable buffer for PLID
                           &c1,
                           &c2,
                           &c3,
                           &c4,
                           &list->no_tries[i_file],
                           mode);

                    list->PLID[i_file] = temp_PLID;     
                    snprintf(temp_colorcode, sizeof(temp_colorcode), "%c%c%c%c", c1, c2, c3, c4);
                    temp_colorcode[sizeof(temp_colorcode) - 1] = '\0'; // Ensure null termination
                    list->colcode[i_file] = temp_colorcode; // Assign buffer to string

                    // Map mode strings to constants
                    if (!strcmp(mode, "PLAY")) {
                        list->mode[i_file] = MODE_PLAY;
                    } else if (!strcmp(mode, "DEBUG")) {
                        list->mode[i_file] = MODE_DEBUG;
                    }

                    fclose(file);
                    ++i_file;
                }
            }
            free(file_list[n_entries]); // Free memory allocated by scandir for the file name
            if (i_file == 10) {
                break; // Stop once we have the top 10 scores
            }
        }
        free(file_list); // Free the memory allocated by scandir for the file list
    }
    list->n_scores = i_file; // Store the number of scores read
    return i_file; // Return the number of scores read
}

// Function to get the current UTC time as a formatted string
string getFormattedTime() {
    // Get the current time in seconds since the Unix epoch
    time_t fulltime;
    time(&fulltime); // Get current time in seconds since 1970-01-01

    // Convert the time to a broken-down UTC time
    struct tm *current_time = gmtime(&fulltime); // Convert to UTC (GMT)

    char timestr[20];
    // Format the time as YYYY-MM-DD HH:MM:SS
    sprintf(timestr, "%4d-%02d-%02d %02d:%02d:%02d",
            current_time->tm_year + 1900, // Year since 1900
            current_time->tm_mon + 1,    // Month [0-11] => [1-12]
            current_time->tm_mday,       // Day of the month [1-31]
            current_time->tm_hour,       // Hour [0-23]
            current_time->tm_min,        // Minute [0-59]
            current_time->tm_sec);       // Second [0-59]

    return string(timestr);
}

// Function to manually parse the date string and calculate seconds since the given date
int secondsSinceStartDate(const time_t start_time) {
    // Obter o tempo atual (em segundos desde a época Unix)
    time_t current_time;
    time(&current_time);

    // Calcular a diferença em segundos entre o tempo atual e o tempo de início
    return static_cast<int>(difftime(current_time, start_time));
}


// Gera a chave secreta com 4 cores
void generate_secret_key(string& key) {
    key.clear();
    for (int i = 0; i < 4; i++) {
        key += COLORS[rand() % 6];
        if (i < 3) { // Adiciona um espaço após cada cor, exceto a última
            key += ' ';
        }
    }
}

// Calcula o feedback com base no palpite e na chave secreta
void calculate_feedback(const string& guess, const string& secret, int& black, int& white) {
    black = white = 0;
    int count_secret[6] = {0}, count_guess[6] = {0};

    for (int i = 0; i < guess.size(); i+=2) {
        if (guess[i] == secret[i]) {
            black++;
        } else {
            count_secret[strchr(COLORS, secret[i]) - COLORS]++;
            count_guess[strchr(COLORS, guess[i]) - COLORS]++;
        }
    }
    for (int i = 0; i < 6; i++) {
        white += min(count_secret[i], count_guess[i]);
    }
}

// Salva o histórico de tentativas do jogo em um arquivo
void save_trials(const Game& game, bool start) {
    char filepath[100];
    snprintf(filepath, sizeof(filepath), "Server/GAMES/GAME_%s.txt", game.plid.c_str());

    // Verifica se o arquivo já existe
    {
        if (!start){
            shared_lock<shared_mutex> lock(games_mutex);   // read lock 
            ifstream file_check(filepath); 
            if (file_check.is_open()) {
                ofstream file(filepath, ios::app);

                // Save only the last trial
                if (!game.trial_history.empty()) {
                    file << game.trial_history.back() << "\n"; // Write the last trial
                }

                file.close();
                return;
            }
        }
    }

    {
        unique_lock<shared_mutex> lock(games_mutex); //write lock
        // Se o arquivo não existir, abre para escrita 
        ofstream file(filepath);
        if (!file.is_open()) {
            perror("Failed to open game state file");
            return;
        }

        // Escreve os metadados
        file << game.plid << " "
             << game.mode << " "
             << game.secret_key << " "
             << game.max_playtime << " "
             << game.start_date << " "
             << game.start_time << "\n";

        // Save only the last trial
        if (!game.trial_history.empty()) {
            file << game.trial_history.back() << "\n"; // Write the last trial
        }

        file.close();
    }
}

void send_trials(int client_sockfd, const string& plid, Game& game, int time_left) {
    string header;
    game.is_active = has_ongoing_game(plid);//read lock inside func

    // Se o jogo estiver ativo, usamos "RST ACT"
    if (game.is_active) {
        char filename[128];

        FILE* file = nullptr;    // Diretório específico onde você quer procurar o arquivo
        snprintf(filename, sizeof(filename), "Server/GAMES/GAME_%s.txt", plid.c_str());

        // Tenta abrir o arquivo com as tentativas no diretório GAMES/
        file = fopen(filename, "rb");//read lock

        if (!file) {
            // If the file is not found in the GAMES/ directory, send an error response
            const char response[MAXBUF] = "RST NOK\n";
            sendMessageTCP(client_sockfd, response);
            close(client_sockfd);
            return;
        }
        // Verifica o tamanho do arquivo
        fseek(file, 0, SEEK_END);

        long file_size = ftell(file);
        rewind(file);

        if (file_size == 0) {
            fclose(file);
            const char response[MAXBUF] = "RST NOK\n";
            sendMessageTCP(client_sockfd, response);
            close(client_sockfd);
            return;
        }

        // Extract only the file name from the full path
        string full_filename(filename);
        size_t last_slash = full_filename.find_last_of("/\\"); // Find the last '/' or '\\'
        string file_name = (last_slash != string::npos) ? full_filename.substr(last_slash + 1) : full_filename;

        //header = "RST ACT " + string(file_name) + " " + to_string(file_size) + "\n";
        string response;  // String to store the complete response

        char line[MAXBUF];
        bool is_first_line = true;
        string last_line;
        int line_count = 0;

        while (fgets(line, sizeof(line), file)) {
            line_count++;
            string current_line(line);
            if (is_first_line) {
                is_first_line = false;
                continue;
            }
            if (current_line.size() > 22) {
                current_line = current_line.substr(0, 22);
            }

            // Append the previous line to the response
            if (!last_line.empty()) {
                response.append(last_line);
                if (last_line.back() != '\n') {
                    response.append("\n");
                }
            }

            // Store the current line as the last line
            last_line = current_line;
        }
        fclose(file);

        if (line_count == 1) {
            response.append("No tries were made in this game.\n");
        }
        
        // Append the last line without modifying it
        if (!last_line.empty()) {
            last_line = last_line.substr(0, last_line.find_last_not_of("\n\r") + 1); // Trim trailing newlines
            response.append(last_line);
        }

        // Add the "seconds to go" as the new last line
        if (line_count > 1) {  // Ensure that there was at least one line processed
            response.append("\n" + to_string(time_left) + '\n');
        }

        // Send the entire response using write in a loop
        size_t response_size = response.size(); // Total size of the response

        header = "RST ACT " + string(file_name) + " " + to_string(response_size) + " \n";

        string final_response;
        final_response.append(header);
        final_response.append(response);
        final_response.push_back('\0');

        sendMessageTCP(client_sockfd, final_response.c_str());
    }
    else{
        char last_game_file[128];

        // Find the most recent game file for the given PLID
        if (FindLastGame(plid.c_str(), last_game_file)) { //readlock inside func
            // Open the most recent game file
            FILE* file = fopen(last_game_file, "rb");
            if (!file) {
                const char response[MAXBUF] = "RST NOK\n";
                sendMessageTCP(client_sockfd, response);
                close(client_sockfd);
                return;
            }

            // Verifica o tamanho do arquivo
            fseek(file, 0, SEEK_END);

            long file_size = ftell(file);
            rewind(file);

            if (file_size == 0) {
                fclose(file);
                const char response[MAXBUF] = "RST NOK\n";
                sendMessageTCP(client_sockfd, response);
                close(client_sockfd);
                return;
            }

            // Extract only the file name from the full path
            string full_filename(last_game_file);
            size_t last_slash = full_filename.find_last_of("/\\"); // Find the last '/' or '\\'
            string file_name = (last_slash != string::npos) ? full_filename.substr(last_slash + 1) : full_filename;

            //header = "RST FIN " + string(file_name) + " " + to_string(file_size) + "\n";
            string response;  // String to store the complete response

            char line[MAXBUF];
            bool is_first_line = true;
            string last_line;
            int line_count = 0;  

            while (fgets(line, sizeof(line), file)) {
                line_count++;
                string current_line(line);
                if (is_first_line) {
                    is_first_line = false;
                    continue;
                }
                if (current_line.size() > 22) {
                    current_line = current_line.substr(0, 22);
                }

                // Append the previous line to the response
                if (!last_line.empty()) {
                    response.append(last_line);
                    if (last_line.back() != '\n') {
                        response.append("\n");
                    }
                }

                // Store the current line as the last line
                last_line = current_line;
            }
            fclose(file);

            if (line_count == 2) {
                response.append("No tries were made in this game.\n");
            }

            // Append the final line (last_line) after the loop finishes
            if (!last_line.empty()) {
                response.append(last_line);
                if (last_line.back() != '\n') {
                    response.append("\n");
                }
            }

            // Send the entire response using write in a loop
            size_t response_size = response.size(); // Total size of the response

            header = "RST FIN " + string(file_name) + " " + to_string(response_size) + " \n";

            string final_response;
            final_response = header + response;
            final_response.push_back('\0');

            sendMessageTCP(client_sockfd, final_response.c_str());

        } else {
            // If no game files are found, send an error message
            const char response[MAXBUF] = "RST NOK\n";
            sendMessageTCP(client_sockfd, response);
        }
    }
    // Fecha o arquivo e o socket após enviar os dados
    shutdown(client_sockfd, SHUT_WR); // Finaliza a escrita para o cliente
    close(client_sockfd); // Fecha o socket
}

void save_game(const Game& game, char code) {
    char filename[32];
    snprintf(filename, sizeof(filename), "Server/GAMES/GAME_%s.txt", game.plid.c_str());

    // Verificar ou criar diretoria do jogador 
    char player_dir[32];
    snprintf(player_dir, sizeof(player_dir), "Server/GAMES/%s/", game.plid.c_str());
    struct stat st = {0};

    {                                           // write lock
        unique_lock<shared_mutex> lock(games_mutex); 
        if (stat(player_dir, &st) == -1) {
            if (mkdir(player_dir, 0700) < 0) {
                perror("Erro ao criar a diretoria do jogador");
                return;
            }
        }
    }
    // Calculating two timestamps
    time_t now = time(nullptr);

    // Timestamp for max playtime expiration
    time_t max_playtime_timestamp = game.start_time + game.max_playtime;

    // Format the timestamps
    char timestamp1[32], timestamp2[32];
    strftime(timestamp1, sizeof(timestamp1), "%Y-%m-%d %H:%M:%S", localtime(&max_playtime_timestamp));
    strftime(timestamp2, sizeof(timestamp2), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Choose which timestamp to use
    bool ended_by_timeout = now > max_playtime_timestamp;
    const char* final_timestamp = ended_by_timeout ? timestamp1 : timestamp2;

    char dest_file_name[32];
    if (ended_by_timeout){
        strftime(dest_file_name, sizeof(dest_file_name), "%Y%m%d_%H%M%S", localtime(&max_playtime_timestamp));
    }else{
        strftime(dest_file_name, sizeof(dest_file_name), "%Y%m%d_%H%M%S", localtime(&now));
    }
    
    // Caminho completo do arquivo destino
    char dest_file[128];
    snprintf(dest_file, sizeof(dest_file), "%s%s_%c.txt", player_dir, dest_file_name, code);

    {   // write lock
        unique_lock<shared_mutex> lock(games_mutex);
        // Mover o arquivo para a diretoria do jogador
        if (rename(filename, dest_file) != 0) {
            perror("Erro ao mover o arquivo do jogo");
            return;
        }
    }

    // Adicionar a última linha com o timestamp ao arquivo movido
    ofstream outfile(dest_file, ios::app);  // Abrir em modo append
    if (!outfile) {
        perror("Erro ao abrir o arquivo para adicionar a linha final");
        return;
    }

    // Obter timestamp formatado
    int game_duration = min(secondsSinceStartDate(game.start_time), game.max_playtime);

    // Escrever linha final
    outfile << final_timestamp << " " << game_duration <<"\n";
    outfile.close();
}

bool is_duplicate_try(const string& buffer, const vector<string>& trial_history) {
    for (const auto& trial : trial_history) {
        // Find the position of "T: " and extract the guess
        size_t pos = trial.find("T: ");
        if (pos != string::npos) {
            // Extract the guess substring (format is "C1C2C3C4")
            string guess_in_history = trial.substr(pos + 3, 7);
            if (guess_in_history == buffer) {
                return true; // Duplicate found
            }
        }
    }
    return false; // No duplicates found
}

void save_score(const Game& game) {
    // Format the date and time
    char timestamp[32];
    time_t now = time(nullptr);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));

    //calculate the score
    int score;
    if (game.trials == 1){
        score = 100;
    }
    else if (game.trials == 8){
        score = 1;
    } else{
        score = 100 - static_cast<int>((game.trials ) * 12.5);
    }

    // Format the filename: score PLID DDMMYYYY HHMMSS.txt
    ostringstream filename;
    filename << "Server/SCORES/" << score << "_" << game.plid << "_" << timestamp << ".txt";

    string mode;
    if (game.mode == 'P'){
        mode = "PLAY";
    } else{
        mode = "DEBUG";
    }

    // Format the score line: SSS PPPPPP CCCC N mode
    ostringstream score_line;
    score_line << setw(3) << setfill('0') << score << " "      // Score with 3 digits, padded
               << setw(6) << setfill('0') << game.plid << " "  // PLID with 6 digits, padded
               << game.secret_key << " "                       // Secret code
               << game.trials << " "                           // Number of moves
               << mode;                                        // Mode (PLAY or DEBUG)

    {
        unique_lock<shared_mutex> lock(scoreboard_mutex); 
        // Write to the score file
        ofstream score_file(filename.str());
        if (score_file.is_open()) {
            score_file << score_line.str() << "\n";
            score_file.close();
        } else {
            cerr << "Failed to create score file: " << filename.str() << "\n";
        }
    }
}

// Envia a tabela de pontuação para o cliente
void processSSBCommand(int client_sockfd, struct sockaddr_in client_addr) {
    SCORELIST top_scores;
    int num_scores;
    streamsize file_size;
    vector<char> file_content(0);

    //verbose mode 
    auto [client_ip, client_port] = getClientIPAndPort(client_addr);                    
    string message = "[REQUEST]: SSB ,from: " + client_ip + ":" + to_string(client_port);
    verbose_print(message);

    {
    shared_lock<shared_mutex> lock(scoreboard_mutex);
    // Find the top scores using the provided function
    num_scores = FindTopScores(&top_scores);
    }

    if (num_scores == 0) {
        // If no scores are found, send an EMPTY status
        const char response[MAXBUF] = "RSS EMPTY\n";
        sendMessageTCP(client_sockfd, response);
    } else {
        {   
            unique_lock<shared_mutex> lock(scoreboard_mutex);
            // Generate the top score file
            ofstream score_file(SCOREBOARD_FILE);
            if (!score_file.is_open()) {
                cerr << "Failed to create temporary score file." << endl;
                const char response[MAXBUF] = "RSS ERR\n";
                sendMessageTCP(client_sockfd, response);
                return;
            } 

            // Write top scores to the file
            for (int i = 0; i < num_scores; ++i) {
                score_file  << i + 1 << "- "                   // Entry number (1-based index)
                            << top_scores.PLID[i] << " "       // Player ID
                            << top_scores.colcode[i] << " "    // Secret key
                            << top_scores.no_tries[i] << "\n"; // Total plays
            }
            score_file.close();

            // Determine file size
            ifstream file(SCOREBOARD_FILE, ios::binary | ios::ate);
            if (!file.is_open()) {
                cerr << "Failed to open the temporary score file." << endl;
                const char response[MAXBUF] = "RSS ERR\n";
                sendMessageTCP(client_sockfd, response);
                return;
            }
            file_size = file.tellg();
            file.seekg(0, ios::beg);

            // Read the file content into a buffer
            file_content.resize(file_size);
            if (!file.read(file_content.data(), file_size)) {
                cerr << "Failed to read the score file content." << endl;
                const char response[MAXBUF] = "RSS ERR\n";
                sendMessageTCP(client_sockfd, response);
                return;
            }
        }

        string response;
        // Create the RSS response header
        string header = "RSS OK " + string(SCOREBOARD_FILE) + " " + to_string(file_size) + " ";

        response.append(header);
        response.append(file_content.begin(), file_content.end());
        response.append("\n");

        sendMessageTCP(client_sockfd, response.c_str());
    }

    // Close the client socket
    close(client_sockfd);
}