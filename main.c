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
int history_ind = -1;
char path[PATH_MAX];
char err[100];
static struct termios saved;

char* print_cmd(char *str){
    if (str){
        char c[1024];
        strcpy(c,"\n");
        strcpy(c,str);
        return strdup(c);
    }
    return NULL;
}

char* get_files(char* path){
    char files[6767];
    files[0] = '\0';
    DIR* dir = opendir(path);
    struct dirent* result;
    if (!dir) {
        return NULL;
    }
    int i = 0;
    while ((result = readdir(dir)) != NULL && i < 999) {
        strcat(files, result->d_name);
        strcat(files,"\n");
    }
    closedir(dir);
    return strdup(files);
}

char* is_present(char *cmd){
    char* path = getenv("PATH");
    if (!path) return NULL;
    char* dup_path = strdup(path);
    char* saveptr1;
    char* token = strtok_r(dup_path, ":", &saveptr1);
    while (token) {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", token, cmd);
        if (access(full, X_OK) == 0) {
            free(dup_path);
            return strdup(full);
        }
        token = strtok_r(NULL, ":", &saveptr1);
    }
    free(dup_path);
    return NULL;
}

char* exec_cmd(char* path, char* args, char* cmd,char** err_val){
    char* argv[1000];
    int i = 0;
    argv[i++] = cmd;
    if (args) {
        char* saveptr2;
        char* token = strtok_r(args, " ", &saveptr2);
        while (token && i < 999) {
            argv[i++] = token;
            token = strtok_r(NULL, " ", &saveptr2);
        }
    }
    argv[i] = NULL;
    int fd[2];
    int fd_err[2];
    pipe(fd);
    pipe(fd_err);
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd_err[1], STDERR_FILENO);
        close(fd[0]);
        close(fd[1]);
        close(fd_err[0]);
        close(fd_err[1]);
        execv(path, argv);
        exit(1);
    } else {
        close(fd[1]);
        close(fd_err[1]);
        
        char buf[4096] = {0};
        char err_buf[4096] = {0};
        char temp[512];
        int n;
        
        while ((n = read(fd[0], temp, sizeof(temp)-1)) > 0){
            temp[n]='\0';
            strcat(buf, temp);
        }
        close(fd[0]);
        
        while ((n = read(fd_err[0], temp, sizeof(temp)-1)) > 0) {
            temp[n]='\0';
            strcat(err_buf, temp);
        }
        close(fd_err[0]);
        
        wait(NULL);
        if(err_val){
            if(strlen(err_buf)>0){
                *err_val=strdup(err_buf);
            } else {
                *err_val = NULL;
            }
        }
        return strdup(buf);
    }
    return NULL;
}

char *show_cmd(){
    char curr_dir[PATH_MAX];
    if (!getcwd(curr_dir, sizeof(curr_dir)))
        return NULL;
    return get_files(curr_dir);
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

char* exec_main(char* buff,char** err_val){
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
             return print_cmd(second);
        else if (strcmp(first, "show") == 0)
           return show_cmd();
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
                char* result = exec_cmd(full, second, first,err_val);
                free(full);
                return result;
            } else {
                char c[100];
                sprintf(c,"%s : no such command\n", first);
                return strdup(c);
            }
        }
    return NULL;
}

void expand_arg(char * arg){
    // printf("in exp \n");
    char temp_arg[1000];
    strcpy(temp_arg,arg);
    // printf("%s\n",temp_arg);
    char buff[1000];
    buff[0]='\0';
    char* saveptr3;
    char* token=strtok_r(temp_arg," ", &saveptr3);

    while(token){
        // printf("%s\n",token);
        if (strcmp(token,">")==0 || strcmp(token,"1>")==0 || strcmp(token,">>")==0 || strcmp(token,"1>>")==0) {
            // printf("im insideinside\n");
            int append=(strcmp(token,">")==0 || strcmp(token,"1>")==0);
            token=strtok_r(NULL," ", &saveptr3);
            // printf("%s\n",token);
            if (token){
                char* err_val=NULL;
                char * res=exec_main(buff,&err_val);
                char* mode=append==1?"w":"a";
                FILE *f=fopen(token,mode);
                if(f){
                    if(res) fprintf(f,"%s",res);
                    fclose(f);
                }
                if(err_val && strlen(err_val)>0){
                    printf("%s",err_val);
                    free(err_val);
                }
                free(res);
                buff[0]='\0';
            }
            buff[0]='\0';
        }
        else if (strcmp(token,"2>")==0 || strcmp(token,"2>>")==0){
            int append=(strcmp(token,"2>>")==0);
            token=strtok_r(NULL," ", &saveptr3);

            if (token){
                char* err_val=NULL;
                char * res=exec_main(buff,&err_val);
                char* mode=append?"a":"w";
                FILE *f=fopen(token,mode);
                if(f){
                    if(err_val && strlen(err_val)>0){
                        fprintf(f,"%s",err_val);
                    }
                    fclose(f);
                }
                if(res && strlen(res)>0){
                    printf("%s",res);
                }
                free(res);
                if(err_val) free(err_val);
                buff[0]='\0';
            }
        }
        else{
            strcat(buff,token);
            strcat(buff," ");
        }
        token=strtok_r(NULL," ", &saveptr3);
    }
    if(strlen(buff)>0){
        char* err_val=NULL;
        char* res=exec_main(buff,&err_val);
        if(res) printf("%s",res);
        if(err_val) printf("%s",err_val);
        free(res);
        if(err_val) free(err_val);
    }
}

int main(){
    setbuf(stdout,NULL);
    getcwd(path, sizeof(path));
    char buff[100];
    buff[0]='\0';
    int i=0;
    int hist_pos = history_ind + 1;
    
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
            write(STDOUT_FILENO, "\r\n", 2);

            if(strlen(buff)>0){
                strcpy(history[++history_ind],buff);
            }
            hist_pos = history_ind + 1;

            disable_raw();
            // printf("%s random buff \n",buff);
            //
            expand_arg(buff);
            enable_raw();
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
                    if(hist_pos > 0){
                        strcpy(buff,history[--hist_pos]);
                        i=strlen(buff);
                    }
                }
                if(s[1]=='B'){
                    if(hist_pos < history_ind){
                        strcpy(buff,history[++hist_pos]);
                        i=strlen(buff);
                    }
                    else{
                        hist_pos = history_ind + 1;
                        buff[0]='\0';
                        i=0;
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

