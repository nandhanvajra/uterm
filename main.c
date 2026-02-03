#include <termios.h>
#include <sys/stat.h>
#include<fcntl.h>
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
#include "uthash.h"


char* builtins[] = {"print", "show", "type","exit","cd"};
int builtin_len=5;
char history[1000][1000];
int history_ind = -1;
char path[PATH_MAX];
char err[100];
static struct termios saved;

char* metaops[]={"2>>","1>>",">>","2>","1>",">","|"};
int metaops_ind=7;


enum cmd_type{
    NRML_CMD,
    PIPE_CMD,
    APPEND_CMD,
    REPLACE_CMD,
    E_APPEND_CMD,
    E_REPLACE_CMD,
    DEFAULT_CMD
};

typedef struct Node{
   enum cmd_type type;
   struct  Node* left;
    struct Node* right;
    char* filename;
    char** arg;

}Node;

void parse_arg(Node *n);

typedef struct{
    char * cmd;
    enum cmd_type type;
}cmd_pairs;

cmd_pairs cmd_arr[]={
    {"|",PIPE_CMD},
    {">",REPLACE_CMD},
    {"1>",REPLACE_CMD},
    {"1>",APPEND_CMD},
    {"1>>",APPEND_CMD},
    {"2>",E_REPLACE_CMD},
    {"2>>",E_APPEND_CMD},
    {"cd",DEFAULT_CMD},
    {"print",DEFAULT_CMD},
    {"show",DEFAULT_CMD}
};



typedef struct{
    char* name;
    UT_hash_handle hh;
}hashmap;
hashmap *h=NULL;

void hmap(){
    for(int i=0;i<metaops_ind;i++){

        hashmap* entry=malloc(sizeof(*entry));
        if(!entry){
            perror("malloc");
            exit(1);
        }
        entry->name=strdup(metaops[i]);
        HASH_ADD_STR(h,name,entry);

    }
}

char* print_cmd(char *str){
    if (str){
        char c[1024];
        strcat(c,"\n");
        strcat(c,str);
        strcat(c,"\n");
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

    printf("in the exec cmd file loc %s\n",buff);
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
char* exec_pipe(int i, int j, char* arg_arr[], int prev_fd){
    pid_t pids[i + 1];


    for (j = 0; j <= i; j++) {
        int fd[2];

        if (j < i)
            pipe(fd);

        char* temp = strdup(arg_arr[j]);
        char* saveptr4;
        char* temp_args[100];
        char* token = strtok_r(temp, " ", &saveptr4);

        int k = 0;
        while (token != NULL) {
            temp_args[k++] = token;
            token = strtok_r(NULL, " ", &saveptr4);
        }
        temp_args[k] = NULL;

        pid_t x = fork();
        if (x == 0) {
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            if (j < i) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            execvp(temp_args[0], temp_args);
            _exit(1);
        }

        pids[j] = x;

        if (prev_fd != -1)
            close(prev_fd);

        if (j < i) {
            close(fd[1]);
            prev_fd = fd[0];
        }
    }

    for (j = 0; j <= i; j++)
        waitpid(pids[j], NULL, 0);

    return NULL;
}

enum cmd_type find_type(char* key) {
    for(int i=0;i<(sizeof(cmd_arr)/sizeof(cmd_arr[0]));i++){
        if(strcmp(cmd_arr[i].cmd,key)==0)
            return cmd_arr[i].type;
    }
    return NRML_CMD;
}


struct Node* expand_arg(char *arg, int i, int k) {

    char prev = '\0';
    char full_cmd[100] = {0};
    bool flag = false;

    Node *n = malloc(sizeof(Node));

    for (int j = i; j < k; j++) {
        full_cmd[strlen(full_cmd)]=arg[j];
        for(int m=0;m<metaops_ind;m++){
            int len=strlen(metaops[m]);


            if (j+len<=k && strncmp(&arg[j],metaops[m],len)==0) {
                n->type  = find_type(metaops[m]);
                n->left  = expand_arg(arg, i, j);
                n->right = expand_arg(arg, j+len, k);
                n->arg = NULL;
                flag = true;
                goto done;
            }

            }
    }


    done:
    if (!flag) {
        bool isdef=false;
        for(int j=0;j<builtin_len;j++){
            if(strcmp(arg,builtins[j])==0) {
                    isdef=true;
                    break;
            }
        }
        if(isdef)
            n->type = DEFAULT_CMD;
        else
            n->type = NRML_CMD;
        n->left = NULL;
        n->right = NULL;

        char *saveptr5;
        char *token = strtok_r(full_cmd, " ", &saveptr5);
        char **arg_s = malloc(100 * sizeof(char *));
        int arg_ind = 0;
        while (token) {
            arg_s[arg_ind++] = strdup(token);
            token = strtok_r(NULL, " ", &saveptr5);
        }
        arg_s[arg_ind] = NULL;
        n->arg = arg_s;
    }
    return n;

}
void exec_new(Node* n){
        execvp(n->arg[0],n->arg);
        perror("coudnt execute command");
        // exit(1);
}

void exec_redir(Node* n){
    if(n->type==REPLACE_CMD || n->type==E_REPLACE_CMD){
        int fd=open(n->right->arg[0],O_WRONLY|O_CREAT|O_TRUNC,0664);
        pid_t p=fork();
        if(p==0){
            if(n->type==REPLACE_CMD)
                dup2(fd,STDOUT_FILENO);
            else
                dup2(fd,STDERR_FILENO);
            exec_new(n->left);
            close(fd);
            exit(1);
        }
        wait(NULL);
  }
  if(n->type==APPEND_CMD || n->type==E_APPEND_CMD){
        int fd=open(n->right->arg[0],O_WRONLY|O_CREAT|O_APPEND,0664);
        pid_t p=fork();
        if(p==0){
            if(n->type==APPEND_CMD)
                dup2(fd,STDOUT_FILENO);
            else
                dup2(fd,STDERR_FILENO);
            exec_new(n->left);
            close(fd);
            exit(1);
        }
        wait(NULL);
  }

}

void exec_pip(Node* n){
    int fd[2];
    pipe(fd);
    pid_t left=fork();
    if(left==0){
        dup2(fd[1],STDOUT_FILENO);
        close(fd[0]);
        parse_arg(n->left);
        close(fd[1]);
        // perror("coudnt execute this");
        exit(0);
    }
    pid_t right=fork();
    if(right==0){
        dup2(fd[0],STDIN_FILENO);
        close(fd[1]);
        parse_arg(n->right);

        close(fd[0]);
        // perror("COudnt execute right cmd ");
        exit(1);
    }
    close(fd[0]);
    close(fd[1]);
    waitpid(left,NULL,0);
    waitpid(right,NULL,0);
}
void parse_arg(Node *n){
        
        if(n->type==NRML_CMD){
            pid_t p=fork();
            if(p==0){
                exec_new(n);
                exit(1);
            }
            waitpid(p,NULL,0);
        }
       else if(n->type==PIPE_CMD){
            exec_pip(n);
        }
        else if(n->type==DEFAULT_CMD){
            char temp[1024]={0};
            char** arg=n->arg;
            for(int i=0;arg[i]!=NULL;i++){
                strcat(temp,arg[i]);
                strcat(temp," ");
            }
            printf("%s\n",exec_main(temp,NULL));
        }
        else{
            exec_redir(n);
        }
}
char *find_full_cmd(char *buff, int space, int i, bool is_cmd) {
    char temp[100];
    int idx = 0;

    int start = (space > 0) ? space + 1 : 0;

for (idx = start; idx < i && buff[idx] != '\0'; idx++) {
    temp[idx - start] = buff[idx];
}
temp[idx - start] = '\0';

    char *res = malloc(4096);
    if (!res) return NULL;
    res[0] = '\0';

    char *env_path = getenv("PATH");
    if (!env_path) {
        perror("getting error in opening the path");
        return res;
    }

    if (is_cmd) {
        char *temp_path = strdup(env_path);
        char *saveptr6;
        char *token = strtok_r(temp_path, ":", &saveptr6);

        while (token) {
            DIR *d = opendir(token);
            if (!d) {
                token = strtok_r(NULL, ":", &saveptr6);
                continue;
            }

            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                char complete_path[100];
                snprintf(complete_path, sizeof(complete_path),
                         "%s/%s", token, entry->d_name);

                struct stat st;
                if (stat(complete_path, &st) == 0 &&
                    S_ISREG(st.st_mode) &&
                    access(complete_path, X_OK) == 0) {

                    if (strlen(temp) <= strlen(entry->d_name) &&
                        strncmp(temp, entry->d_name, strlen(temp)) == 0) {

                        strcat(res, entry->d_name);
                        strcat(res, "\n");
                    }
                }
            }
            closedir(d);
            token = strtok_r(NULL, ":", &saveptr6);
        }
        free(temp_path);

    } else {
        DIR *d = opendir(".");
        if (!d) {
            perror("we couldn't open the directory");
            return res;
        }

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strlen(temp) <= strlen(entry->d_name) &&
                strncmp(temp, entry->d_name, strlen(temp)) == 0) {

                strcat(res, entry->d_name);
                strcat(res, "\n");
            }
        }
        closedir(d);
    }

    return res;
}
int main(){
    hmap();
    setbuf(stdout,NULL);
    getcwd(path, sizeof(path));
    char buff[100];
    buff[0]='\0';
    int i=0;
    int spaces=-1;
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
            Node * n=expand_arg(buff,0,strlen(buff));
            parse_arg(n);
            enable_raw();
            buff[0]='\0';
            i=0;
            spaces=0;
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
        else if(c=='\t'){
                 char *arr = find_full_cmd(buff, spaces, i, spaces <= 0);

            int count = 0;
            for (int j = 0; arr[j] != '\0'; j++)
                if (arr[j] == '\n')
                    count++;

            if (count == 1) {
                char completion[100];
                int k = 0;

                for (int j = 0; arr[j] != '\0' && arr[j] != '\n'; j++)
                    completion[k++] = arr[j];
                completion[k] = '\0';

                int start = (spaces > 0) ? spaces + 1 : 0;
                buff[start] = '\0';
                strcat(buff, completion);
                i = strlen(buff);
            }
            else if (count > 1) {
                disable_raw();
                write(STDOUT_FILENO, "\n", 1);
                printf("%s", arr);
                enable_raw();
            }

            free(arr);

                   
        }
        else{
             buff[i]=c;
             if(c==' '){
                 spaces=i;
             }
             buff[i+1]='\0';
             i++;
        }
         write_line(buff);

         
        }
    return 0;
}

