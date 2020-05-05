/*
 * COMP 321 Project 6: Web Proxy
 *
 * This program implements a multithreaded HTTP proxy.
 *
 * Vidisha Ganesh vg19
 * Jolisa Brown jmb26
 */

#include <assert.h>

#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

//#include "cis307.h"
#include "csapp.h"

FILE *proxy_log;

static void	client_error(int fd, const char *cause, int err_num, 
		    const char *short_msg, const char *long_msg);
static char *create_log_entry(const struct sockaddr_in *sockaddr,
		    const char *uri, int size);
static int	parse_uri(const char *uri, char **hostnamep, char **portp,
		    char **pathnamep);
int parse_uri_static(char *uri, char *filename, char *cgiargs); 
void doit(int fd);
void read_requesthdrs(rio_t *rp, int clientfd);
void *connection_func();
void replaceOccurences(char *buf, const char *prevChar, const char *newChar);
static int	open_client(char *hostname, int port);
//void serve_static(int fd, char *filename, int filesize);
//void serve_dynamic(int fd, char *filename, char *cgiargs);
//void get_filetype(char *filename, char *filetype);
struct sockaddr_in serveraddr;
struct client_info {
    //struct socklen_t clientlen;
    int connfd;
};
int nthreads = 5; /* number of threads created. */ 
struct client_info connection_array[1000]; /* Echo buffer. */
int thread_cnt;             /* Item count. */
int connection_index;
pthread_mutex_t mutex; /* pthread mutex. */
pthread_mutex_t thread_lock; /* pthread mutex. */ 
pthread_cond_t cond_thread_available;   /* pthread cond variable to wait on non-empty buffer. */

pthread_cond_t cond_connection_available;   /* pthread cond variable to wait on non-empty buffer. */
//what else do we need to add?
/*
doit
*/
/* 
 * Requires:
 *   <to be filled in by the student(s)> 
 *
 * Effects:
 *   <to be filled in by the student(s)> 
 */
int
main(int argc, char **argv)
{
    printf("STARTING MAIN PROCESS NOW\n");
    //for now, putting in what the tiny shell had, and we'll make adjustments
    int listenfd, connfd;
    int i;
    long myid[nthreads];
    pthread_t tid[nthreads];
    
    socklen_t clientlen;
    struct client_info client_conn;
    struct sockaddr_storage clientaddr;
    connection_index = -1;

    char *log_name = "proxy.log";

	/* Check the arguments. */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
	}

	//open log file
	proxy_log = fopen(log_name, "w");
	if(proxy_log == NULL) {
	    fprintf(stderr, "Can't open: %s. \n", log_name);
	    return(1);
	}


	listenfd = Open_listenfd(argv[1]);
	//TODO: add threads to the following loop using threads as specified in tiny.c

    Pthread_mutex_init(&mutex, NULL);

    Pthread_mutex_init(&thread_lock, NULL);
    /* INITIALIZE THE CONDITION VARIABLES HERE. */
    pthread_cond_init(&cond_thread_available, NULL);
    pthread_cond_init(&cond_connection_available, NULL);


    for (i = 0; i < nthreads; i++) {
        myid[i] = i;
        Pthread_create(&tid[i], NULL, connection_func, &myid[i]);

    }

    for (i = 0; i < nthreads; i++) {
        Pthread_join(tid[i], NULL);
    }


	while (1) {


        clientlen = sizeof(clientaddr);

        //accepts connection request
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd != 0) {
            Pthread_mutex_lock(&mutex);
            client_conn.connfd = connfd;
            //client_conn.clientlen = clientlen;
            connection_array[connection_index + 1] = client_conn;
            connection_index += 1;
            //enque connection struct
            
            Pthread_mutex_unlock(&mutex);
        }
        connfd = 0;
       
        
        
	    
	}// until client closed!

    /* Clean up. */
    Pthread_mutex_destroy(&mutex);
    /* DESTROY THE CONDITION VARIABLES HERE. */
    pthread_cond_destroy(&cond_thread_available);
    pthread_cond_destroy(&cond_connection_available);
	/* Return success. */
	fflush(proxy_log);
	fclose(proxy_log);
	return (0);
}

/* Each thread executes this function*/

void * 
connection_func() {

    while (1) {
        /*WAIT ON APPROPRIATE CONDITION VARIABLE. */
        
        pthread_cond_wait(&cond_thread_available, &mutex);
        pthread_cond_wait(&cond_connection_available, &mutex);
        //char hostname[MAXLINE], port[MAXLINE];
        struct client_info client_conn;
        //struct socklen_t clientlen;
        //struct sockaddr_storage clientaddr;
        int connfd;
        /* Acquire mutex lock. */
        Pthread_mutex_lock(&mutex);
        Pthread_mutex_lock(&thread_lock);
        thread_cnt -= 1;
        if (thread_cnt == 0) {
            /* signal no threads currently available*/
            pthread_cond_signal(&cond_thread_available);
            client_conn = connection_array[connection_index];
        }
        Pthread_mutex_unlock(&thread_lock);

        /* Release mutex lock. */
        Pthread_mutex_unlock(&mutex);
        //clientlen = sizeof(clientaddr);
        /* need to include this in struct???? Or always the same value*/
        //clientlen = client_conn.clientlen;
        //accepts connection request
        connfd = client_conn.connfd;
        //Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        //printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd); //performs the transaction
        Close(connfd); //closes end of connection
        /* release thread */
        Pthread_mutex_lock(&thread_lock);
        if (thread_cnt == 0) {
            /* signal thread is now available*/
            thread_cnt += 1;
            pthread_cond_signal(&cond_thread_available);
        } else {
            thread_cnt += 1;
        }
        
        Pthread_mutex_unlock(&thread_lock);

    }

}



/*
 * doit - handle one HTTP request/response transaction
 */

void doit(int fd)
{
     printf("STARTING DOIT FUNCTION!!!\n");
     
//     int clientfd;
     //struct stat sbuf;
     //struct sockaddr_in serveraddr;
	 //struct addrinfo *ai;
     //char buf[MAXLINE];
     //need to check sizes of possible bufs - might actually need to realloc
//     char *buf = calloc(MAXLINE, sizeof(char));
//     char *temp_buf= calloc(MAXLINE, sizeof(char));
     //char *headbuf = (char*) calloc(MAXLINE, sizeof(char));
//     char *hostnamep, *portp, *pathnamep;
//     char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
     //char filename[MAXLINE];
     //char cgiargs[MAXLINE];
//     rio_t rio;




     int serverfd;
     char *buf = calloc(MAXLINE, sizeof(char));
     char *log_data = calloc(MAXLINE, sizeof(char));
     char *temp_buf= calloc(MAXLINE, sizeof(char));
     char *hostnamep, *portp, *pathnamep;
     char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
     char *client_buf = calloc(MAXLINE, sizeof(char)); // [MAXLINE];
     rio_t rio, rio_server;

     //initialize memory to 0, cleans out whatever was there previously
     memset(method, 0, sizeof(method));
     memset(uri, 0, sizeof(uri));
     memset(version, 0, sizeof(version));
     
    printf("Finished cleaning memory!!!\n");

     /* First, we have to get the FIRST request line and parse it */

     Rio_readinitb(&rio, fd); //init reader

     if(!Rio_readlineb(&rio, buf, MAXLINE)) {
        printf("No request to read! ERROR!\n");
        return;
     }
     printf("The first line of request from client is: %s\n", buf);

     //TODO: fix the following implementation
     /*Version that accounts for lines longer than MAXLINE*/
     //goes through the first line and gets the line while there are still characters to get
//     int count = 1;
//     while(!strstr(buf, "\r\n")) {
//     	//buf = (char*) realloc(buf, ((count + 1) * MAX));
//     	//buf_extended = (char*) realloc(buf_extended, (count * MAX));
//     	Rio_readlineb(&rio, ((temp_buf )), MAXLINE);
//     	strcat(buf, temp_buf);
//        //Rio_readlineb(&rio, ((buf + count * MAX)), MAX);
//        printf("buf is : %s and is size %d\n", buf, (int) strlen(buf));
//        //printf("buf_extended is : %s and is size %d\n", buf_extended, (int) strlen(buf_extended));
//        count += 1;
//        //printf("Count is %d\n", count);
//     }


     /* Verify that this is a get request*/
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        client_error(fd, method, 501, "Not implemented",
        "This proxy server does not implement this method");
         return;
    }

    if(strstr(version, "1.1") == NULL && strstr(version, "1.0") == NULL) {
        client_error(fd, version, 500, "Not supported",
        "This proxy server does not support this version");
        return;
    }

    /* TODO:check 1.0 or 1.1*/



     /* FREEE STRINGS after completed use */
     	
    printf("About to parse uri:\n");
    printf("Uri is: %s\n", uri);

    parse_uri(uri, &hostnamep, &portp, &pathnamep);
    printf("Hostname is: %s\n", hostnamep);
    printf("Port is: %s and in int form: %d\n", portp, atoi(portp));
    printf("Pathname is: %s\n", pathnamep);

    serverfd = open_client(hostnamep, atoi(portp));
    /* error check for client file descriptor*/
    if (serverfd == -1) {
		unix_error("open_clientfd Unix error");
	} else if (serverfd == -2) {
		dns_error("open_clientfd DNS error");
	}
    Rio_readinitb(&rio_server, serverfd);

    // writes first line of request to the origin server
    Rio_writen(serverfd, buf, strlen(buf));

    printf("Buf is %s \n", buf);

    

    /* edited to check for headers we don't want to be sent, will send to origin server */
    read_requesthdrs(&rio, serverfd);

    /*  add connection closed to buff and send*/

    if (strstr(version, "1.1") != NULL) { // it's version 1.1
        Rio_writen(serverfd, "Connection: closed\r\n", strlen("Connection: closed\r\n"));
        printf("Connection closed header sent.\n");
    }

    /* Write empty line to server to signal end of headers*/
    Rio_writen(serverfd, "\r\n", strlen("\r\n"));

    /*Should have sent everything we needed to send (request) from proxy to origin server*/
    printf("finished FINALLY writing headers 2 and sent a new empty line\n");


    // TODO: proxy read message from origin server and writes back to client
    int length = 1;
    printf("Writing to client soon\n");
    //length = rio_readlineb(&rio_client, client_buf, MAXLINE);
    printf("Writing to client now\n");
    //printf("Client buf is %s \n", client_buf);
    int size = 0;
    while((length = rio_readlineb(&rio_server, client_buf, MAXLINE)) > 0) {
    	printf("Entered loop for writing to server\n");
        printf("Sending the message: %s\n", client_buf);
        Rio_writen(fd, client_buf, length);
        size += length;

    }
    log_data = create_log_entry(&serveraddr, uri, size);

    //fprintf(proxy_log, "%s\n", log_data);
    fprintf(proxy_log, log_data);
   // fputs(log_data, proxy_log);
    fflush(proxy_log);


    printf("size of log data is %d \n", size);
    printf("Log data is : %s\n", log_data);
   // Rio_readlineb(&rio_client, client_buf, strlen(client_buf));

    // TODO: send message back to client
   // Rio_writen(fd, client_buf, strlen(client_buf));
    // TODO: put stuff in the log
    /*. CLOSE. CLIENT FD*/
    Close(serverfd);

    Free(buf);
    Free(temp_buf);

    /* after everything is functional */
    // TODO: fix the memory allocation in buffer - do realloc and store all the headers in one buf

    //we're doing the same thing twice, and it's interfereing
    //in the doit, we're populating the buffer, then do it again in readrequest headers
     
} // end doit


/*
 * Requires:
 *   hostname points to a string representing a host name, and port in an
 *   integer representing a TCP port number.
 *
 * Effects:
 *   Opens a TCP connection to the server at <hostname, port> and returns a
 *   file descriptor ready for reading and writing.  Returns -1 and sets
 *   errno on a Unix error.  Returns -2 on a DNS (getaddrinfo) error.
 */
static int
open_client(char *hostname, int port)
{
    printf("Starting to open client now!\n");
	struct sockaddr_in serveraddr;
	struct addrinfo *ai;
	int clientfd;

	// Set clientfd to a newly created stream socket.
	// REPLACE THIS.
	clientfd = socket(AF_INET, SOCK_STREAM, 0);

	printf("OPENED the SOCKET\n");

	// Use getaddrinfo() to get the server's IP address.
	getaddrinfo(hostname, NULL, NULL, &ai);
    printf("got the ADDRESS info\n");
	/*
	 * Set the address of serveraddr to be server's IP address and port.
	 * Be careful to ensure that the IP address and port are in network
	 * byte order.
	 */
	memset(&serveraddr, 0, sizeof(serveraddr));
	printf("SET THE MEMORY ASIDE FOR SERVERADDR\n");
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr = ((struct sockaddr_in *)(ai->ai_addr))->sin_addr;
	serveraddr.sin_port = htons(port);
	printf("The port is %d\n", serveraddr.sin_port);

    printf("FINISHED ASSIGNING SERVERADDRS struct thingys\n");
	// Establish a connection to the server with connect().
	connect(clientfd, (const struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in));
	printf("CONNECTED TO CLIENT File Descriptor\n");

	return (clientfd);

}



/*
 * read_requesthdrs - read and parse HTTP request headers
 */
void read_requesthdrs(rio_t *rp, int clientfd)
{
    int size = 7;
    char *temp_buf = calloc(size, sizeof(char));
    char * check_buf = calloc(size, sizeof(char));
    int buf_length;
//    char buf[MAXLINE];
//    char single_header[MAXLINE];
    char *buf = calloc(MAXLINE, sizeof(char));
    printf("ORIGINAL buf size is %d \n", (int) sizeof(buf));
    printf("ORIGINAL temp_buf size is %d \n", (int) sizeof(temp_buf));
    //char *single_header = calloc(MAXLINE, sizeof(char));
    char *new_ptr = calloc(MAXLINE, sizeof(char));


    rio_readnb(rp, temp_buf, size);
    strcat(buf, temp_buf);
    /* create initial line of headers*/
    
    while(strcmp(check_buf, "\r\n\r\n") != 0) {
        Rio_readnb(rp, temp_buf, size);
        if((strlen(buf) + strlen(temp_buf) * sizeof(char)) > sizeof(buf)) {
                printf("0: str_len buf is : %d , str_len temp_buf is : %d \n", (int) strlen(buf), (int) strlen(temp_buf));
                printf("1: temp_buf is %s\n", temp_buf);
                printf("2: buf is %s\n", buf);
                new_ptr = (char *)realloc(buf, sizeof(buf) + MAXLINE);
                buf= new_ptr;
                strcat(buf, temp_buf);
            } else {
               printf("3: temp_buf is %s\n", temp_buf);
               printf("4: buf is %s\n", buf);
               strcat(buf, temp_buf);
            }


        buf_length = strlen(buf);
        printf("buf_length is %d\n", buf_length);
        check_buf = &buf[buf_length - 4];
        printf("check_buf is %s\n", check_buf);
        
    }
    replaceOccurences(buf, "</script>", "");
    char * oo = calloc(10, sizeof(char));
    oo = "Hellllooo";

    //char oo[10] = "Hellllooo";
    printf("This is oo %s\n", oo);

    //replaceOccurences(oo, "ooo", "");
    //printf("This is oo %s\n", oo);
    //printf("the buf before is %s\n", buf);
    replaceOccurences(buf, "Connection: proxy-connection", "");
    replaceOccurences(buf, "Connection: connection", "");
    replaceOccurences(buf, "Connection: keep-alive", "");
    printf("the buf after is %s \n", buf);
    rio_writen(clientfd, buf, strlen(temp_buf));
    printf("finished writing headers\n");
    return;
}


/**
 * Replace all occurrences of a given a word in string.
 */
void replaceOccurences(char *buf, const char *prevChar, const char *newChar)
{
    char *pos, temp[sizeof(buf)];
    int index = 0;
    int prevlen;

    prevlen = strlen(prevChar);


    /*
     * Repeat till all occurrences are replaced. 
     */
    while ((pos = strstr(buf, prevChar)) != NULL)
    {
        // Bakup current line
        strcpy(temp, buf);

        // Index of current found word
        index = pos - buf;

        // Terminate str after word found index
        buf[index] = '\0';

        // Concatenate str with new word 
        strcat(buf, newChar);
        
        // Concatenate str with remaining words after 
        // oldword found index.
        strcat(buf, temp + index + prevlen);
    }
}





/*
 * Requires:
 *   The parameter "uri" must point to a properly NUL-terminated string.
 *
 * Effects:
 *   Given a URI from an HTTP proxy GET request (i.e., a URL), extract the
 *   host name, port, and path name.  Create strings containing the host name,
 *   port, and path name, and return them through the parameters "hostnamep",
 *   "portp", "pathnamep", respectively.  (The caller must free the memory
 *   storing these strings.)  Return -1 if there are any problems and 0
 *   otherwise.
 */
static int
parse_uri(const char *uri, char **hostnamep, char **portp, char **pathnamep)
{
	const char *pathname_begin, *port_begin, *port_end;
	printf("in parse uri\n");
	printf("the uri is: %s\n", uri);
	if (strncasecmp(uri, "http://", 7) != 0) {
	    printf("invalid http starting for uri!!!\n");
	    return (-1);
	}

	printf("in parse uri2\n");
	printf("The uri is: %s\n", uri);
	printf("Won't print\n");
	/* Extract the host name. */
	const char *host_begin = uri + 7;
	const char *host_end = strpbrk(host_begin, ":/ \r\n");
	if (host_end == NULL)
		host_end = host_begin + strlen(host_begin);
	int len = host_end - host_begin;
	char *hostname = Malloc(len + 1);
	strncpy(hostname, host_begin, len);
	hostname[len] = '\0';
	*hostnamep = hostname;

	/* Look for a port number.  If none is found, use port 80. */
	if (*host_end == ':') {
		port_begin = host_end + 1;
		port_end = strpbrk(port_begin, "/ \r\n");
		if (port_end == NULL)
			port_end = port_begin + strlen(port_begin);
		len = port_end - port_begin;
	} else {
		port_begin = "80";
		port_end = host_end;
		len = 2;
	}
	char *port = Malloc(len + 1);
	strncpy(port, port_begin, len);
	port[len] = '\0';
	*portp = port;

	/* Extract the path. */
	if (*port_end == '/') {
		pathname_begin = port_end;
		const char *pathname_end = strpbrk(pathname_begin, " \r\n");
		if (pathname_end == NULL)
			pathname_end = pathname_begin + strlen(pathname_begin);
		len = pathname_end - pathname_begin;
	} else {
		pathname_begin = "/";
		len = 1;
	}
	char *pathname = Malloc(len + 1);
	strncpy(pathname, pathname_begin, len);
	pathname[len] = '\0';
	*pathnamep = pathname;
	
	printf("The hostname is %s\n", *hostnamep);
	printf("The port is %s\n", *portp);

	return (0);
}

/*
 * Requires:
 *   The parameter "sockaddr" must point to a valid sockaddr_in structure.  The
 *   parameter "uri" must point to a properly NUL-terminated string.
 *
 * Effects:
 *   Returns a string containing a properly formatted log entry.  This log
 *   entry is based upon the socket address of the requesting client
 *   ("sockaddr"), the URI from the request ("uri"), and the size in bytes of
 *   the response from the server ("size").
 */
static char *
create_log_entry(const struct sockaddr_in *sockaddr, const char *uri, int size)
{
	struct tm result;

	/*
	 * Create a large enough array of characters to store a log entry.
	 * Although the length of the URI can exceed MAXLINE, the combined
	 * lengths of the other fields and separators cannot.
	 */
	const size_t log_maxlen = MAXLINE + strlen(uri);
	char *const log_str = Malloc(log_maxlen + 1);

	/* Get a formatted time string. */
	time_t now = time(NULL);
	int log_strlen = strftime(log_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z: ",
	    localtime_r(&now, &result));

	/*
	 * Convert the IP address in network byte order to dotted decimal
	 * form.
	 */
	Inet_ntop(AF_INET, &sockaddr->sin_addr, &log_str[log_strlen],
	    INET_ADDRSTRLEN);
	log_strlen += strlen(&log_str[log_strlen]);

	/*
	 * Assert that the time and IP address fields occupy less than half of
	 * the space that is reserved for the non-URI fields.
	 */
	assert(log_strlen < MAXLINE / 2);

	/*
	 * Add the URI and response size onto the end of the log entry.
	 */
	snprintf(&log_str[log_strlen], log_maxlen - log_strlen, " %s %d", uri,
	    size);


	//put newline at the end of log_str
	//str_cat(log_str, "\n");
	//Free();

    //TODO: free the memory used to store the string
    //TODO: put newline at the end of the returned string - DONE
    strcat(log_str, "\n");
	return (log_str);
}

/*
 * Requires:
 *   The parameter "fd" must be an open socket that is connected to the client.
 *   The parameters "cause", "short_msg", and "long_msg" must point to properly 
 *   NUL-terminated strings that describe the reason why the HTTP transaction
 *   failed.  The string "short_msg" may not exceed 32 characters in length,
 *   and the string "long_msg" may not exceed 80 characters in length.
 *
 * Effects:
 *   Constructs an HTML page describing the reason why the HTTP transaction
 *   failed, and writes an HTTP/1.0 response containing that page as the
 *   content.  The cause appearing in the HTML page is truncated if the
 *   string "cause" exceeds 2048 characters in length.
 */
static void
client_error(int fd, const char *cause, int err_num, const char *short_msg,
    const char *long_msg)
{
	char body[MAXBUF], headers[MAXBUF], truncated_cause[2049];

	assert(strlen(short_msg) <= 32);
	assert(strlen(long_msg) <= 80);
	/* Ensure that "body" is much larger than "truncated_cause". */
	assert(sizeof(truncated_cause) < MAXBUF / 2);

	/*
	 * Create a truncated "cause" string so that the response body will not
	 * exceed MAXBUF.
	 */
	strncpy(truncated_cause, cause, sizeof(truncated_cause) - 1);
	truncated_cause[sizeof(truncated_cause) - 1] = '\0';

	/* Build the HTTP response body. */
	snprintf(body, MAXBUF,
	    "<html><title>Proxy Error</title><body bgcolor=""ffffff"">\r\n"
	    "%d: %s\r\n"
	    "<p>%s: %s\r\n"
	    "<hr><em>The COMP 321 Web proxy</em>\r\n",
	    err_num, short_msg, long_msg, truncated_cause);

	/* Build the HTTP response headers. */
	snprintf(headers, MAXBUF,
	    "HTTP/1.0 %d %s\r\n"
	    "Content-type: text/html\r\n"
	    "Content-length: %d\r\n"
	    "\r\n",
	    err_num, short_msg, (int)strlen(body));

	/* Write the HTTP response. */
	if (rio_writen(fd, headers, strlen(headers)) != -1)
		rio_writen(fd, body, strlen(body));
}

// Prevent "unused function" and "unused variable" warnings.
static const void *dummy_ref[] = { client_error, create_log_entry, dummy_ref,
    parse_uri };

    //TODO: Remove this extra commented code before submitting

    //THIS CODE CAME FROM DOIT ++++++++++++++++++++++++++++++++++
     //Rio_readlineb(&rio, buf , MAX);

     //printf("Maxline is %d\n", MAXLINE);
     /*while(strcmp(buf, "\r\n")) {
     	buf_extended = (char*) realloc(buf_extended, (count * MAX));
     	//Rio_readlineb(&rio, ((buf )), MAX);
        Rio_readlineb(&rio, ((buf + count * MAX)), MAX);
        strcat(buf_extended, buf);
        printf("bufis : %s and is size %d\n", buf, (int) strlen(buf));
        printf("buf_extended is : %s and is size %d\n", buf_extended, (int) strlen(buf_extended));
        count += 1;
        printf("Count is %d\n", count);
    }*/
    //DOIT++++++++++++++++++++++++++++++++++++++++++++++++++++++