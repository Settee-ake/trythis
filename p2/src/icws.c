#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include "pcsa_net.h"
#include "parse.h"
#include <pthread.h>
#include <time.h>
#include <poll.h>


#define MAXBUF 8192
#define THREAD_NUM 100
#define PERSISTENT 1
#define CLOSE 0

pthread_t th[THREAD_NUM];
char portdirect[MAXBUF];
char rootdirect[MAXBUF];
int timeout;
int numthread;

////////////// Thread implementing //////////////////
typedef struct Task {
    int confd;
   // char* rootdirect;
} Task;

Task taskQueue[256];
int taskCount = 0;

pthread_mutex_t mutexQueue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexTask = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condQueue = PTHREAD_COND_INITIALIZER;



////////////////////////////////////////////////////////////

//char optarg[MAXBUF];

int getOption(int argc, char** argv){

    int option_index = 0;
    int c; //indicating switch cases.....

    struct option long_options[] = {
        {"port", required_argument,0,'p'},
        {"root", required_argument,0,'r'},
        {"numThreads",required_argument,0,'n'},
        {"timeout",required_argument,0,'t'},
        {0,0,0,0}
    };

    while((c = getopt_long(argc,argv, "prnt", long_options, &option_index)) != -1){
        switch (c) {
            case 'p' : strcpy(portdirect,optarg); break;
            case 'r' : strcpy(rootdirect,optarg); break;
            case 'n' : numthread = atoi(optarg); break;
            case 't' : timeout = atoi(optarg); break;
            default:
                printf("invalid arguments\n");
                exit(0);
        }
    }

    return 1;
}

typedef struct sockaddr SA;

char* mimeCheck(char* req_obj){
    // find extension
    char * ext = strchr(req_obj, '.') + 1;
    char * mime;
    
    // check extension
    if ( strcmp(ext, "html") == 0 )
        mime = "text/html";
    else if ( strcmp(ext, "css") == 0 )
        mime = "text/css";
    else if ( strcmp(ext, "txt") == 0 )
        mime = "txt/plain";
    else if ( strcmp(ext, "jpg") == 0 )
        mime = "image/jpg";
     else if ( strcmp(ext, "gif") == 0 )
        mime = "image/gif";
    else if ( strcmp(ext, "js") == 0 )
        mime = "application/javascript";
    else if ( strcmp(ext, "jpeg") == 0 )
        mime = "image/jpeg";
    else{
        mime = "null";
    }

    return mime;
}


void respond_head(int connFd, char* rootFol, char* req_obj, char* connectionType){
     char loc[MAXBUF];                   
    char headr[MAXBUF];                

    strcpy(loc, rootFol);               
    if (strcmp(req_obj, "/") == 0){     
        req_obj = "/index.html";
    } 
    else if (req_obj[0] != '/'){     
        strcat(loc, "/");   
    }
    strcat(loc, req_obj);

    printf(" File location is: %s \n", loc);
    int fd = open( loc , O_RDONLY);

    printf("already open \n");

    char curr_time[MAXBUF];
    time_t t = time(NULL);
    struct tn* tm =localtime(&t);
    strcpy(curr_time, asctime(tm));

    if (fd < 0){
        sprintf(headr, 
                "HTTP/1.1 404 not found\r\n"
                "Server: Micro\r\n"
                "Connection: %s\r\n", connectionType);
        write_all(connFd, headr, strlen(headr));
        return;
    }

    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;
    if (filesize < 0){
        printf("Filesize Error\n");
        close(fd);
        return ;
    }
    // find extension
   char * mime = mimeCheck(req_obj);

    if ( strcmp(mime, "null")==0){
        char * msg = "File type not supported\n";
        write_all(connFd, msg , strlen(msg) );
        close(fd);
        return;
    }
    sprintf(headr, 
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: Micro\r\n"
            "Connection: %s\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n"
            "Last-Modified: %s\r\n\r\n"
            , curr_time, connectionType, filesize, mime, ctime(&st.st_mtime));

    write_all(connFd, headr, strlen(headr));
}

void respond_get(int connFd, char* rootFol, char* req_obj, char* connectionType) {

    
    char loc[MAXBUF];                   
    char headr[MAXBUF];                

    strcpy(loc, rootFol);               
    if (strcmp(req_obj, "/") == 0){     
        req_obj = "/index.html";
    } 
    else if (req_obj[0] != '/'){     
        strcat(loc, "/");   
    }
    strcat(loc, req_obj);
    printf(" File location is: %s \n", loc);
    int fd = open( loc , O_RDONLY);
    printf("already open \n");

     char curr_time[MAXBUF];
    time_t t = time(NULL);
    struct tn* tm =localtime(&t);
    strcpy(curr_time, asctime(tm));


    if (fd < 0){
        sprintf(headr, 
                "HTTP/1.1 404 not found\r\n"
                "Server: Micro\r\n"
                "Connection: %S\r\n",connectionType);
        write_all(connFd, headr, strlen(headr));
        return;
    }

    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;
    if (filesize < 0){
        printf("Filesize Error\n");
        close(fd);
        return ;
    }

    // find extension
    char * mime = mimeCheck(req_obj);

    if ( strcmp(mime, "null")==0){
        char * msg = "File type not supported\n";
        write_all(connFd, msg , strlen(msg) );
        close(fd);
        return;
    }
       sprintf(headr, 
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: ICWS\r\n"
            "Connection: %s\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n"
            "Last-Modified: %s\r\n\r\n"
            , curr_time,connectionType, filesize, mime, ctime(&st.st_mtime));

    write_all(connFd, headr, strlen(headr));


    char buf[filesize];
    ssize_t numRead;
    while ((numRead = read(fd, buf, MAXBUF)) > 0) {
        write_all(connFd, buf, numRead);
    }

    if ( (close(fd)) < 0 ){
        printf("Failed to close input file. Meh.\n");
    }    
}


int serve_http(int connFd, char* rootFol) {


    char headr[MAXBUF];    

    printf("connfd : %d\n",connFd);
    char buffer[MAXBUF];

    char line[MAXBUF];
    int readLine;


////////////// Setting for writing Date in a header//////////////////
     char curr_time[MAXBUF];
    time_t t = time(NULL);
    struct tn* tm =localtime(&t);
    strcpy(curr_time, asctime(tm));


///////////// Setting for timeout///////////////////////////////////
    struct pollfd fds[1];

     for(;;){

        fds[0].fd = connFd;

        fds[0].events = POLLIN;

        int t = timeout * 1000;

        int pret = poll(fds, 1, t);

        if(pret < 0){
            perror("polling fail for timeout\n");
            return CLOSE;
        } else if(pret == 0){
                   sprintf(headr ,
            "HTTP/1.1 408 Request Timeout Error\r\n"
            "Date: %s \r\n"
            "Server: ICWS\r\n"
            "Connection: CLOSE\r\n\r\n", curr_time);

            write_all(connFd, headr, strlen(headr));
            return CLOSE;
        } else{
            while ( (readLine = read(connFd, line, MAXBUF)) > 0 ){
                strcat(buffer, line);

                if (strstr(buffer, "\r\n\r\n") != NULL){
                    memset(line, '\0', MAXBUF);
                    break;
                }
                memset(line, '\0', MAXBUF);
            }

            break;
        }
    }
    

    printf("LOG : %s\n", buffer);

    pthread_mutex_lock(&mutexTask);

    Request *request = parse(buffer,MAXBUF,connFd);

    pthread_mutex_unlock(&mutexTask);

    ////////////////////////////////////////setting connection type//////////////////////////////////////////////////////////
    int connection;
    char* connection_str;
    connection = PERSISTENT;
    connection_str = "keep-alive";







///////////////////////////////in case of failling parse///////////////////////////////////////////////////////////////////////////////
    if (request == NULL || request->http_method == NULL || request->http_uri == NULL || request->http_version==NULL)
    {
        printf("LOG : Failed to parse the request\n");
        sprintf(headr, 
            "HTTP/1.1 400 Parsing Failed\r\n"
            "Date: %s\r\n"
            "Server: ICWS\r\n"
            "Connection: CLOSE\r\n\r\n", curr_time);

        write_all(connFd, headr, strlen(headr));
        connection = CLOSE;

        memset(buffer, 0, MAXBUF);
        memset(headr, 0, MAXBUF);
        return connection;
    }
    
  /////////////////////////////////////////checking the connection type /////////////////////////////////////////////////////////////

        char* head_name;
        char* head_val;

      for(int i = 0; i < request->header_count;i++){

        head_name = request->headers[i].header_name;

        head_val = request->headers[i].header_value;

        if(strcasecmp(head_name, "CONNECTION") == 0){

            if(strcasecmp(head_val, "CLOSE") == 0){

                connection = CLOSE;

                connection_str = "close";
            }
            break;
        }
    }
 

    ///////////////////////////////////////////Printing The Headers//////////////////////////////////////////////////////////////////////////

    printf("Http Method %s\n",request->http_method);
    printf("Http Version %s\n",request->http_version);
    printf("Http Uri %s\n",request->http_uri);


    for(int index = 0;index < request->header_count;index++){
    printf("Request Header\n");
    printf("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
    }
    
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    if (strcasecmp( request->http_version , "HTTP/1.1") != 0 && strcasecmp( request->http_version , "HTTP/1.0") != 0){ 
        printf("LOG: Incompatible HTTP version\n");
        printf("request->http_version: %s\n",request->http_version);
        sprintf(headr, 
            "HTTP/1.1 505 incompatable version\r\n"
            "Date: %s\r\n"
            "Server: ICWS\r\n"
            "Connection: CLOSE\r\n\r\n", curr_time);
        write_all(connFd, headr, strlen(headr));
        free(request->headers);
        free(request);
        memset(buffer, 0, MAXBUF);
        memset(headr, 0, MAXBUF);
        connection = CLOSE;
        return connection;
    }


    if (strcasecmp(request->http_method, "GET") == 0 &&
        strcasecmp(request->http_version, "HTTP/1.1") == 0) {
        respond_get(connFd, rootFol, request->http_uri, connection_str);
    }
    else if (strcasecmp(request->http_method, "HEAD") == 0 &&
        strcasecmp(request->http_version, "HTTP/1.1") == 0) {
        respond_head(connFd, rootFol, request->http_uri, connection_str);
    }  
    else {

         printf("LOG: Unknown request\n\n");

         sprintf(headr ,
         "HTTP/1.1 501 Method not supported\r\n"
         "Date: %s \r\n"
         "Server: ICWS\r\n"
         "Connection: %s\r\n\r\n", curr_time,connection_str);
         

         write_all(connFd, headr, strlen(headr));

    }
    
     free(request->headers); 
     free(request);


     memset(headr,'\0',MAXBUF);
     memset(buffer,'\0',MAXBUF);
     printf("done serving\n");
     return connection;
    

}

void executeTask(Task* task) {
    ////set before going into connection loop, we want to find out if it's persistent or not
    int connection = PERSISTENT;

    while(connection == PERSISTENT){
        connection = serve_http(task->confd, rootdirect);
    }

    close(task->confd);
}

void submitTask(Task task) {
    pthread_mutex_lock(&mutexQueue);
    taskQueue[taskCount] = task;
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue);
}

void* startThread(void* args) {
    while (1) {

        Task task;

        pthread_mutex_lock(&mutexQueue);
        while (taskCount == 0) {
            pthread_cond_wait(&condQueue, &mutexQueue);
        }

        task = taskQueue[0];
        int i;
        for (i = 0; i < taskCount - 1; i++) {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--;
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task);

        if(task.confd < 0){break;}
    }
}

int main(int argc, char* argv[]) {

    getOption(argc, argv);

///////////////////////////// Initializing THREADS ///////////////////////////////////////
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_mutex_init(&mutexTask, NULL);
    pthread_cond_init(&condQueue, NULL);
    int i;
    for (i = 0; i < numthread; i++) {

        int checker = pthread_create(&th[i], NULL, &startThread, NULL);

        if (checker != 0) {
            perror("Failed to create the thread");
        }
        //check for creating threads
        if(checker == 0){
            printf("Thread number %d already created \n",i);
        }
    

    }
    srand(time(NULL));

/////////////////////////////////////////////////////////////////////////////////////////////


  //  printf("timeout is ===== %d\n", numthread);
    int listenFd = open_listenfd(portdirect);

    for (;;) {

        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);
        

        int connFd = accept(listenFd, (SA *) &clientAddr, &clientLen);
        if (connFd < 0) { fprintf(stderr, "Failed to accept\n"); continue; }

        char hostBuf[MAXBUF], svcBuf[MAXBUF];
        if (getnameinfo((SA *) &clientAddr, clientLen, 
                        hostBuf, MAXBUF, svcBuf, MAXBUF, 0)==0) 
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");


        

        struct Task t;
        t.confd = connFd;


        submitTask(t);



        //This for testing without threads
        //serve_http(connFd, rootdirect);
        //close(connFd);

        
    }

    for (i = 0; i < numthread; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join the thread");
        }
    }
    pthread_mutex_destroy(&mutexQueue);
    pthread_mutex_destroy(&mutexTask);
    pthread_cond_destroy(&condQueue);

    return 0;
}


// references
// https://code-vault.net/lesson/w1h356t5vg:1610029047572
// https://www.youtube.com/watch?v=7i9z4CRYLAE
// https://www.youtube.com/watch?v=_n2hE2gyPxU
// https://stackoverflow.com/questions/423626/get-mime-type-from-filename-in-c
// https://stackoverflow.com/questions/1442116/how-to-get-the-date-and-time-values-in-a-c-program
// https://github.com/nathan78906/HTTPServer/blob/master/PersistentServer.c 
// https://www.youtube.com/watch?v=UP6B324Qh5k 
// https://www.codeproject.com/Articles/1275479/State-Machine-Design-in-C
// https://github.com/Pithikos/C-Thread-Pool