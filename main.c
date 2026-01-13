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
    DIR* dir = opendir(path);
    struct dirent* result;

    if (!dir) {
        return NULL;
    }

    int i = 0;
    while ((result = readdir(dir)) != NULL && i < 999) {
            strcat(files, strdup(result->d_name));
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

char* exec_cmd(char* path, char* args, char* cmd){
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
            return strdup(buf);
        }
        close(fd[0]);
        wait(NULL);
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



char* exec_main(char* buff){
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
                return exec_cmd(full, second, first);
                free(full);
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

                            char * res=exec_main(buff);
                            char* mode=append==1?"w":"a";
                            FILE *f=fopen(token,mode);
                            if(f){
                                fprintf(f,"%s",res);
                                // printf("success\n");
                                fclose(f);
                            }
                            else{
                                printf("wrong outpurrr\n");
                            }
                            free(res);
                            buff[0]='\0';
                        }
                        else{
                            printf("wrong tokerrr");
                        }
                        buff[0]='\0';
                }
        else{
            strcat(buff,token);
            strcat(buff," ");
        }
        token=strtok_r(NULL," ", &saveptr3);
    }
        
    printf("%s",exec_main(buff));

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
            write(STDOUT_FILENO, "\r\n", 2);
            if(k<history_ind){
                
                strcpy(buff,history[k]);
            }
            else{
                strcpy(history[history_ind++],buff);
                k++;

            }
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
