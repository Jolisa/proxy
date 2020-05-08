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

#include "csapp.h"

FILE *proxy_log;

struct client_info {
    struct sockaddr_storage clientaddr;
    int connfd;
};

//shared buffer
struct client_info connection_array[20];

struct sockaddr_in serveraddr;
struct addrinfo *ai;

int connection_index;
int max_num_connections = 20;
int nthreads = 15; /* number of threads created. */
int request_num = -1;
int thread_cnt;             /* Item count. */

pthread_cond_t cond_connection_array_full;
pthread_cond_t cond_connection_available;

pthread_mutex_t mutex;

static void	client_error(int fd, const char *cause, int err_num, 
		    const char *short_msg, const char *long_msg);
static char *create_log_entry(const struct sockaddr_in *sockaddr,
		    const char *uri, int size);
static int	parse_uri(const char *uri, char **hostnamep, char **portp,
		    char **pathnamep);
static int	open_client(char *hostname, int port);
int parse_uri_static(char *uri, char *filename, char *cgiargs);
void doit(int fd, struct sockaddr_storage clientaddr);
void read_requesthdrs(rio_t *rp, int clientfd, char *buf, char *version);
void * connection_func(void *arg);

/* 
 * Requires:
*   argv[1] is a string representing a host name, and argv[2] is a string
*   representing a TCP port number (in decimal).
 *
 * Effects:
 *   Opens a connection to the specified server. Then, adds the connection file
 *   descriptor to a shared buffer. Worker threads will access these connection
 *   requests as they become available to complete the transactions as concurrently
 *   as possible.
 */
int
main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
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

	/* OPEN LOG FILE */
	proxy_log = fopen(log_name, "w");
	if(proxy_log == NULL) {
	    fprintf(stderr, "Can't open: %s. \n", log_name);
	    return(1);
	}


	listenfd = Open_listenfd(argv[1]);

    /* INITIALIZE MUTEX LOCK */
    Pthread_mutex_init(&mutex, NULL);

    /* INITIALIZE THE CONDITION VARIABLES HERE. */
    pthread_cond_init(&cond_connection_available, NULL);
    pthread_cond_init(&cond_connection_array_full, NULL);


    for (i = 0; i < nthreads; i++) {
        myid[i] = i;
        Pthread_create(&tid[i], NULL, connection_func, &myid[i]);
    }

    /* Producer */
	while (1) {

        clientlen = sizeof(clientaddr); //key
        connfd = 0;

        //accepts connection request
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        if (connfd > 0) {

            Pthread_mutex_lock(&mutex);

            client_conn.connfd = connfd;
            client_conn.clientaddr = clientaddr;

            /* Wait until there is space in the shared buffer */
            while(connection_index == (max_num_connections - 1)) {
                Pthread_cond_wait(&cond_connection_array_full, &mutex);
            }

            /* Adds the client_info struct to shared buffer */
            connection_array[connection_index + 1] = client_conn;
            connection_index += 1;


            if(connection_index == 0) {
                Pthread_cond_signal(&cond_connection_available);
            }

            for (i = 0; i< connection_index; i++) {
                printf("Inside if statement: Connection array at i %d is: \n",
                connection_array[i].connfd);
            }    

            Pthread_mutex_unlock(&mutex);
        }
	}

    /* Clean up. */
    Pthread_mutex_destroy(&mutex);
    /* DESTROY THE CONDITION VARIABLES HERE. */
    pthread_cond_destroy(&cond_connection_array_full);
    pthread_cond_destroy(&cond_connection_available);
	fflush(proxy_log);
	fclose(proxy_log);
	return (0);
}

/*
 * Requires:
 *   Nothing
 *
 * Effects:
 *   Assigns a connection file descriptor to a thread, and thread performs
 *   the connection request/response transaction using doit function.
 */
void * 
connection_func(void *arg) {

    arg = (void *)arg;

    while (1) {

        Pthread_mutex_lock(&mutex);

        /* Wait until there is a connection available*/
        while (connection_index < 0) {
            Pthread_cond_wait(&cond_connection_available, &mutex);
        }


        struct client_info client_conn;
        client_conn = connection_array[connection_index];

        connection_index--;
        request_num++;

        if ((connection_index + 1) == (max_num_connections - 1)) {
            Pthread_cond_signal(&cond_connection_array_full);
        }

        /* Release mutex lock. */
        Pthread_mutex_unlock(&mutex);

        doit(client_conn.connfd, client_conn.clientaddr); //performs the transaction
        Close(client_conn.connfd); //closes end of connection
    }
}

/*
 * Requires:
 *   clientaddr - a valid struct containing info about the client location
 *   fd - the file descriptor for the client to proxy
 *
 * Effects:
 *   Handles one HTTP request/response transaction between client, proxy,
 *   and the end server.
 */
void doit(int fd, struct sockaddr_storage clientaddr)
{
     int serverfd, rio_r, rio_w;
     char *hostnamep, *portp, *pathnamep, *log_data;
     char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
     char buf[3*MAXLINE];
     char temp_buf[MAXLINE];
     char client_buf[MAXLINE];
     rio_t rio, rio_server;

     //initialize memory to 0, cleans out whatever was there previously
     memset(method, 0, sizeof(method));
     memset(uri, 0, sizeof(uri));
     memset(version, 0, sizeof(version));
     memset(buf, 0, sizeof(buf));
     memset(temp_buf, 0, sizeof(temp_buf));
     memset(client_buf, 0, sizeof(client_buf));

     /* First, we have to get the FIRST request line and parse it */

     rio_readinitb(&rio, fd); //init reader

     if((rio_r = rio_readlineb(&rio, temp_buf, MAXLINE)) == -1 && errno == ECONNRESET) {
        printf("No request to read! ERROR!\n");
        freeaddrinfo(ai);
        Free(hostnamep);
        Free(portp);
        Free(pathnamep);
        return;
        
     }

     /* Verify that this is a get request*/
    sscanf(temp_buf, "%s %s %s", method, uri, version);
     if (strcasecmp(method, "GET")) {
        client_error(fd, method, 501, "Not implemented\n",
        "This proxy server does not implement this method\n");
        printf("\n*** End of Request ***\n");
        printf("Request %d: Received non-GET request\n", request_num);
         return;
     }

    if(strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0) {
        client_error(fd, version, 500, "Not supported",
        "This proxy server does not support this version");
        return;
    }

    parse_uri(uri, &hostnamep, &portp, &pathnamep);

    serverfd = open_client(hostnamep, atoi(portp));
    /* error check for client file descriptor*/
    if (serverfd == -1) {
		unix_error("open_clientfd Unix error");
	} else if (serverfd == -2) {
		dns_error("open_clientfd DNS error");
	}
    rio_readinitb(&rio_server, serverfd);

    char *ip_address = malloc(MAXLINE*sizeof(char));
    Inet_ntop(AF_INET, &((const struct sockaddr_in *)&clientaddr)->sin_addr, ip_address,
        INET_ADDRSTRLEN);
    printf("Request %d: Recieved request from %s:\n", request_num, ip_address);
    printf(temp_buf);

    /* formats first line of request */
    strcpy(buf, "GET ");
    strcat(buf, pathnamep);
    strcat(buf, " ");
    strcat(buf, version);
    strcat(buf, "\r\n");

    /* edited to check for headers we don't want to be sent, will send to origin server */
    read_requesthdrs(&rio, serverfd, buf, version);

    /* Write empty line to server to signal end of headers*/
    rio_w = rio_writen(serverfd, "\r\n", strlen("\r\n"));

    if (rio_w == -1 && errno == EPIPE) {
        Close(serverfd);
        freeaddrinfo(ai);
        Free(hostnamep);
        Free(portp);
        Free(pathnamep);
        Free(ip_address);
        return;
    }

    int length = 1;
    int size = 0;

    printf("\n*** End of Request ***\n");
    while((length = rio_readnb(&rio_server, client_buf,  MAXLINE)) > 0) {

        printf("Request %d: Forwarded %d bytes from end server to client\n",
        request_num, length);
        rio_w = rio_writen(fd, client_buf, length);

        if (rio_w == -1 && errno == EPIPE) {
        Close(serverfd);
        freeaddrinfo(ai);
        Free(hostnamep);
        Free(portp);
        Free(pathnamep);
        Free(ip_address);
        return;
    }
        size += length;
    }

    log_data = create_log_entry((const struct sockaddr_in *)&clientaddr, uri, size);

    fprintf(proxy_log, log_data);
    fflush(proxy_log);

    Close(serverfd);
    freeaddrinfo(ai);
    Free(hostnamep);
    Free(portp);
    Free(pathnamep);
    Free(log_data);
    Free(ip_address);
    return;
}




/*
 * Requires:
 *   rp - a pointer to a valid read/write buffer
 *   serverfd - the file descriptor for the end server
 *   version - a pointer to the version string
 *
 * Effects:
 *   Collects all the request headers from the input request and forwards
 *   them to the end server. If connection is reset, process terminates.
 */
void read_requesthdrs(rio_t *rp, int serverfd, char *buf, char *version)
{
    int rio_r;
    int rio_w;
    char *new_ptr = calloc((4 * MAXLINE), sizeof(char));

    //new_ptr = (char *)realloc(buf, sizeof(buf) + (4 * MAXLINE));
    memmove(new_ptr, buf, strlen(buf));
    buf= new_ptr;

    /*An edited version for testing purposes*/
    char temp_buf[MAXLINE * 3];

    rio_r = rio_readlineb(rp, temp_buf, MAXLINE * 3);
    /* check whether client connection has been closed*/
    if (rio_r == -1 && errno == ECONNRESET) {
        Free(buf);
        return;
    } else {
        printf(temp_buf);
    }

    /* make case insensitive string comparison*/
    if(strncasecmp(temp_buf, "proxy-connection:", 17) != 0 &&
    strncasecmp(temp_buf, "connection:", 11) != 0 &&
    strncasecmp(temp_buf, "keep-alive:", 11) != 0 &&
    strcmp(temp_buf, "\r\n")) {
        if((strlen(buf) + strlen(temp_buf) *sizeof(char)) > sizeof(buf)) {
            new_ptr = (char *)Realloc(buf, strlen(buf) + (4 * MAXLINE));
            buf= new_ptr;
            strcat(buf, temp_buf);
        } else {
            strcat(buf, temp_buf);
        }
    }
    while(strcmp(temp_buf, "\r\n") != 0) {
        if((rio_r = rio_readlineb(rp, temp_buf, (3 * MAXLINE))) == -1 &&
            errno == ECONNRESET) {
            Free(buf);
            return;
        } else {
            printf(temp_buf);
            if(strncasecmp(temp_buf, "proxy-connection:", 17) != 0 &&
                strncasecmp(temp_buf, "connection:", 11) != 0 &&
                strncasecmp(temp_buf, "keep-alive:", 11) != 0 &&
                strcmp(temp_buf, "\r\n")) {
                    if((strlen(buf) + strlen(temp_buf) *sizeof(char)) > sizeof(buf)) {
                        new_ptr = (char *)Realloc(buf, strlen(buf) + (4 * MAXLINE));
                        buf= new_ptr;
                        strcat(buf, temp_buf);
                    } else {
                        strcat(buf, temp_buf);
                    }
            }
        }
    }

    /*  add connection closed to buff and send*/
    if (strstr(version, "1.1") != NULL) { // it's version 1.1
        if((strlen(buf) + strlen("Connection: close\r\n") * sizeof(char)) > sizeof(buf)) {
                new_ptr = (char *)Realloc(buf, strlen(buf) + (4 * MAXLINE));
                buf= new_ptr;
                strcat(buf, "Connection: close\r\n");
            } else {
                strcat(buf, "Connection: close");
            }
    }

    printf("*** End of Request ***\n");
    printf("Request %d: Forwarding request to end server:\n", request_num);
    printf(buf);

    rio_w = rio_writen(serverfd, buf, strlen(buf));

    /* check whether write failed*/
    if (rio_w == -1 && errno == EPIPE) {
        printf("Request %d: Write to end server failed\n", request_num);
    }

    Free(buf);
    return;
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

	if (strncasecmp(uri, "http://", 7) != 0) {
	    printf("invalid http starting for uri!!!\n");
	    return (-1);
	}

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

    struct sockaddr_in serveraddr;
    int clientfd;

    clientfd = socket(AF_INET, SOCK_STREAM, 0);

    // Use getaddrinfo() to get the server's IP address.
    getaddrinfo(hostname, NULL, NULL, &ai);

    /*
     * Set the address of serveraddr to be server's IP address and port.
     * Be careful to ensure that the IP address and port are in network
     * byte order.
     */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr = ((struct sockaddr_in *)(ai->ai_addr))->sin_addr;
    serveraddr.sin_port = htons(port);

    // Establish a connection to the server with connect().
    connect(clientfd, (const struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in));

    return (clientfd);

}

// Prevent "unused function" and "unused variable" warnings.
static const void *dummy_ref[] = { client_error, create_log_entry, dummy_ref,
    parse_uri };
