#include <termios.h>
#include <stdbool.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ncurses.h>

char* builtins[] = {"print", "show", "type"};
char history[1000][1000];
int history_ind = 0;
char path[PATH_MAX];


static struct termios saved;


void print_cmd(char *str){
    if (str)
        printf( "%s\n", str);
}

char** get_files(char* path){
    char** files = malloc(1000 * sizeof(char *));
    DIR* dir = opendir(path);
    struct dirent* result;

    if (!dir) {
        files[0] = NULL;
        return files;
    }

    int i = 0;
    while ((result = readdir(dir)) != NULL && i < 999) {
        files[i++] = strdup(result->d_name);
    }
    files[i] = NULL;
    closedir(dir);
    return files;
}

char* is_present(char *cmd){
    char* path = getenv("PATH");
    if (!path) return NULL;

    char* dup_path = strdup(path);
    char* token = strtok(dup_path, ":");

    while (token) {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", token, cmd);
        if (access(full, X_OK) == 0) {
            free(dup_path);
            return strdup(full);
        }
        token = strtok(NULL, ":");
    }

    free(dup_path);
    return NULL;
}

void exec_cmd(char* path, char* args, char* cmd){
    char* argv[1000];
    int i = 0;

    argv[i++] = cmd;

    if (args) {
        char* token = strtok(args, " ");
        while (token && i < 999) {
            argv[i++] = token;
            token = strtok(NULL, " ");
        }
    }
    argv[i] = NULL;

    int fd[2];
    pipe(fd);

    pid_t pid = fork();
    if (pid == 0) {
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        close(fd[0]);
        close(fd[1]);
        execv(path, argv);
        exit(1);
    } else {
        close(fd[1]);
        char buf[512];
        int n;
        while ((n = read(fd[0], buf, sizeof(buf)-1)) > 0) {
            buf[n] = '\0';
            printf("%s", buf);
        }
        close(fd[0]);
        wait(NULL);
    }
}

char* expand_arg(char * arg){
    static int i=0;
    char buff[1000];
    char* curr=&arg[i+1];
    char* token=strtok(arg," ");
    while(token!=NULL){
        if(strcmp(token,">")==0){
            strcat(buff,">67");
            return strdup(buff);
        }
        if(strcmp(token,">>")==0){
            strcat(buff,">>67");
            return strdup(buff);
        }
        i++;
        i+=strlen(token);
        strcat(buff,strdup(token));
        strcat(buff," ");
        token=strtok(NULL," ");
    }
    return strdup(buff);
}

void show_cmd(){
    char curr_dir[PATH_MAX];
    if (!getcwd(curr_dir, sizeof(curr_dir)))
        return;

    char **files = get_files(curr_dir);
    for (int i = 0; files[i]; i++)
        printf("%s\n", files[i]);
}

void change_path(char *new_path){
   if(chdir(new_path)==0){
     if (new_path[0]=='/'){
        strcpy(path,new_path);
     }
     else{
         strcat(path,"/");
         strcat(path,new_path);
     }
  }
   else{
       perror(new_path);
   }
}
void enable_raw(){
    tcgetattr(STDIN_FILENO,&saved);
    struct termios t=saved;


    t.c_lflag &=~(ICANON|ECHO|ISIG);
    t.c_iflag &=~(IXON|ICRNL);
    t.c_oflag &=~(OPOST);
    t.c_cc[VMIN]=1;
    t.c_cc[VTIME]=0;

    tcsetattr(STDIN_FILENO,TCSAFLUSH,&t);
    

}
void disable_raw(){
    tcsetattr(STDIN_FILENO,TCSAFLUSH ,&saved );
}
void clear_line(){
    write(STDOUT_FILENO,"\r\033[2K",5);
}
void write_line(char* buff){
    // printf("in the writeline buff \n");
    clear_line();
    write(STDOUT_FILENO,buff,strlen(buff));
    
}

void exec_main(char* buff){
        disable_raw();
        char *first = buff;
        char *second = NULL;

        for (int j = 0; buff[j]; j++) {
            if (buff[j] == ' ') {
                buff[j] = '\0';
                second = &buff[j + 1];
                break;
            }
        }

        if (strcmp(first, "print") == 0)
            print_cmd(second);
        else if (strcmp(first, "show") == 0)
            show_cmd();
        else if(strcmp(first,"clear")==0){
            enable_raw();
            write(STDOUT_FILENO,"\033[2J\033[H",7);
            disable_raw();
        }
        else if(strcmp(first,"exit")==0){
            exit(0);
        }
        else if (strcmp(first,"cd")==0){
            if (second == NULL) {
                chdir(getenv("HOME"));
                getcwd(path, sizeof(path));
            }
            else if (strcmp(second, "..") == 0) {
                dirname(path);
                chdir(path);
            }
            else {
                change_path(second);
            }
        }
        else {
            char* full = is_present(first);
            if (full) {
                exec_cmd(full, second, first);
                free(full);
            } else {
                printf("%s : no such command\n", first);
            }
        }

        enable_raw();


}

int main(){
    setbuf(stdout,NULL);
    getcwd(path, sizeof(path));
    char buff[100];
    buff[0]='\0';
    int i=0;
    int k=history_ind;
    
    enable_raw();
    while(1){

        unsigned char c;
        if(read(STDIN_FILENO,&c,1)!=1){
            continue;
        }
        //echoing and redrawing is not gud
        // write(STDOUT_FILENO,&c,1);
        if(c=='\n'||c=='\r'){
            // printf("pressed enter\n");
            if(k<history_ind){
                
                strcpy(buff,history[k]);
            }
            else{
                strcpy(history[history_ind++],buff);
                k++;

            }

            exec_main(buff);
            buff[0]='\0';

            i=0;

        }
        else if (c==127 || c==8){
            if (i>0){
                i--;
                // write(STDOUT_FILENO,"\b \b",3);
                buff[i]='\0';
            }
        }
        else if(c==27){

            // write_line("in the up ir down");
            unsigned char s[2];
            if((read(STDIN_FILENO,&s[0],1))!=1) continue;
            
            if((read(STDIN_FILENO,&s[1],1))!=1) continue;
            // write_line("in the up ir down");
            if (s[0]=='['){
                if(s[1]=='A'){
                    if(k>0){

                        strcpy(buff,history[--k]);
                        i=strlen(buff);
                    }
                }
                if(s[1]=='B'){

                    if(k<history_ind){

                        strcpy(buff,history[++k]);
                        i=strlen(buff);

                    }
                }
            }
        }
        else{
             buff[i]=c;
             buff[i+1]='\0';
             i++;

        }

         write_line(buff);
    }
    return 0;
    }

