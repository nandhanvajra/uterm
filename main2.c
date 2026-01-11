#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define MAX_HISTORY 100
#define MAX_CMD_LEN 1024

// History structure
typedef struct {
    char *commands[MAX_HISTORY];
    int count;
    int current;
} History;

History history = {0};

// Function to add command to history
void add_to_history(const char *cmd) {
    if (strlen(cmd) == 0) return;
    
    // Don't add duplicate of last command
    if (history.count > 0 && strcmp(history.commands[history.count - 1], cmd) == 0) {
        return;
    }
    
    if (history.count < MAX_HISTORY) {
        history.commands[history.count] = strdup(cmd);
        history.count++;
    } else {
        // Shift history and add new command
        free(history.commands[0]);
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            history.commands[i] = history.commands[i + 1];
        }
        history.commands[MAX_HISTORY - 1] = strdup(cmd);
    }
    history.current = history.count;
}

// Get command from history (up/down arrow)
char* get_history_cmd(int direction) {
    if (history.count == 0) return NULL;
    
    if (direction == -1) { // Up arrow
        if (history.current > 0) {
            history.current--;
            return history.commands[history.current];
        }
    } else if (direction == 1) { // Down arrow
        if (history.current < history.count - 1) {
            history.current++;
            return history.commands[history.current];
        } else if (history.current == history.count - 1) {
            history.current = history.count;
            return ""; // Empty command line
        }
    }
    return NULL;
}

// Set terminal to raw mode for reading arrow keys
void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}

// Clear current line in terminal
void clear_line() {
    printf("\r\033[K");
    fflush(stdout);
}

// Read command with arrow key support
int read_command(char *buffer, size_t size) {
    struct termios orig;
    enable_raw_mode(&orig);
    
    int pos = 0;
    buffer[0] = '\0';
    char temp_buffer[MAX_CMD_LEN] = {0};
    
    printf("$ ");
    fflush(stdout);
    
    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;
        
        // Handle escape sequences (arrow keys)
        if (c == 27) {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) break;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) break;
            
            if (seq[0] == '[') {
                if (seq[1] == 'A') { // Up arrow
                    char *hist_cmd = get_history_cmd(-1);
                    if (hist_cmd) {
                        clear_line();
                        printf("$ %s", hist_cmd);
                        strcpy(buffer, hist_cmd);
                        pos = strlen(buffer);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'B') { // Down arrow
                    char *hist_cmd = get_history_cmd(1);
                    if (hist_cmd) {
                        clear_line();
                        printf("$ %s", hist_cmd);
                        strcpy(buffer, hist_cmd);
                        pos = strlen(buffer);
                        fflush(stdout);
                    }
                }
            }
        } else if (c == 127 || c == 8) { // Backspace
            if (pos > 0) {
                pos--;
                buffer[pos] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == '\n' || c == '\r') { // Enter
            buffer[pos] = '\0';
            printf("\n");
            disable_raw_mode(&orig);
            return pos;
        } else if (c >= 32 && c < 127) { // Printable characters
            if (pos < size - 1) {
                buffer[pos++] = c;
                buffer[pos] = '\0';
                printf("%c", c);
                fflush(stdout);
            }
        }
    }
    
    disable_raw_mode(&orig);
    return -1;
}

int main() {
    char cmd[MAX_CMD_LEN];
    
    printf("Simple Shell with Command History\n");
    printf("Use Up/Down arrows to navigate history\n");
    printf("Type 'exit' to quit\n\n");
    
    while (1) {
        if (read_command(cmd, MAX_CMD_LEN) < 0) {
            break;
        }
        
        // Skip empty commands
        if (strlen(cmd) == 0) {
            continue;
        }
        
        // Exit command
        if (strcmp(cmd, "exit") == 0) {
            break;
        }
        
        // Add to history
        add_to_history(cmd);
        
        // Execute command (simplified - just echo for demo)
        printf("Executing: %s\n", cmd);
        
        // In a real shell, you would fork and exec here
        // For now, we'll just demonstrate the history feature
    }
    
    // Cleanup
    for (int i = 0; i < history.count; i++) {
        free(history.commands[i]);
    }
    
    printf("Goodbye!\n");
    return 0;
}
