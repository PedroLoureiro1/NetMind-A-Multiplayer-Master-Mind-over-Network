# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -pthread

# Directories
SERVER_DIR = Server
CLIENT_DIR = Client

# Source files
SERVER_SRC = $(SERVER_DIR)/main.cpp $(SERVER_DIR)/server.cpp $(SERVER_DIR)/debug_command.cpp \
             $(SERVER_DIR)/quit_command.cpp $(SERVER_DIR)/st_command.cpp \
             $(SERVER_DIR)/start_command.cpp $(SERVER_DIR)/try_command.cpp
CLIENT_SRC = $(CLIENT_DIR)/main.cpp $(CLIENT_DIR)/player.cpp

# Object files
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)

# Executables
SERVER_EXEC = GS
CLIENT_EXEC = player

# Targets
all: $(SERVER_EXEC) $(CLIENT_EXEC)

$(SERVER_EXEC): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CLIENT_EXEC): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile server source files
$(SERVER_DIR)/%.o: $(SERVER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile client source files
$(CLIENT_DIR)/%.o: $(CLIENT_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(SERVER_EXEC) $(CLIENT_EXEC)

# Rebuild
rebuild: clean all
