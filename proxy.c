/*
 * COMP 321 Project 6: Web Proxy
 *
 * This program implements a multithreaded HTTP proxy.
 *
 * Vidisha Ganesh vg19
 * Jolisa Brown jmb26
 */ 

#include <assert.h>

#include "csapp.h"

static void	client_error(int fd, const char *cause, int err_num, 
		    const char *short_msg, const char *long_msg);
static char *create_log_entry(const struct sockaddr_in *sockaddr,
		    const char *uri, int size);
static int	parse_uri(const char *uri, char **hostnamep, char **portp,
		    char **pathnamep);
int parse_uri_static(char *uri, char *filename, char *cgiargs); 
void doit(int fd);
void read_requesthdrs(rio_t *rp);
//void serve_static(int fd, char *filename, int filesize);
//void serve_dynamic(int fd, char *filename, char *cgiargs);
//void get_filetype(char *filename, char *filetype);


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
    //for now, putting in what the tiny shell had, and we'll make adjustments
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

	/* Check the arguments. */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
	}

	listenfd = Open_listenfd(argv[1]);
	//TODO: add threads to the following loop using threads as specified in tiny.c
	while (1) {
	    clientlen = sizeof(clientaddr);
	    //accepts connection request
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
	    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
	    printf("Accepted connection from (%s, %s)\n", hostname, port);
	    doit(connfd); //performs the transaction
	    Close(connfd); //closes end of connection
	}// until client closed!

	/* Return success. */
	return (0);
}


/*
 * parse_uri_static - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
int parse_uri_static(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
	strcpy(cgiargs, "");                            
	strcpy(filename, ".");                          
	strcat(filename, uri);                          
	if (uri[strlen(uri)-1] == '/')                  
	    strcat(filename, "home.html");              
	return 1;
    }
    else {  /* Dynamic content */                       
	ptr = index(uri, '?');                          
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	}
	else 
	    strcpy(cgiargs, "");                        
	strcpy(filename, ".");                          
	strcat(filename, uri);                          
	return 0;
    }
}
/*
 * doit - handle one HTTP request/response transaction
 */

void doit(int fd)
{
     
     int serverfd;
     struct stat sbuf;
     struct sockaddr_in serveraddr;
	 struct addrinfo *ai;
     //char buf[MAXLINE];
     char *buf = NULL;
     char *headbuf = (char*) calloc(MAXLINE, sizeof(char));
     char *hostnamep, *portp, *pathnamep;
     char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
     char filename[MAXLINE];
     //char cgiargs[MAXLINE];
     rio_t rio;


     
     /* Verify that this is a get request*/
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        client_error(fd, method, 501, "Not implemented",
        "Tiny does not implement this method");
         return;
    }
     
    //open connection to server
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    parse_uri(uri, &hostnamep, &portp, &pathnamep);
    //get server IP address
    getaddrinfo(hostnamep, NULL, NULL, &ai);
     /*
	 * Set the address of serveraddr to be server's IP address and port.
	 * Be careful to ensure that the IP address and port are in network
	 * byte order.
	 */
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	// FILL THIS IN.
	serveraddr.sin_addr = ((struct sockaddr_in *)(ai->ai_addr))->sin_addr;
	/* should we be bit flipping this value here or naw??*/
	serveraddr.sin_port = htons(*portp);

	//serveraddr.sin_port = htons(portp);

	// Establish a connection to the server with connect().
	// FILL THIS IN.
	connect(serverfd, (const struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in));
     

     //we want to 
     printf("Request headers:\n");
     int count = 0;
     /* Read request line and headers */
     Rio_readinitb(&rio, fd);


     /* should these be \r\n or \n...\n might need to be the check for the end of the headers list*/	

     while(strcmp(buf, "\r\n")) {
     	//change size of buff to allow full line to be written in ...should we change buff here to become a pointer??
     	buf = (char*) realloc(buf, (count + 1) * MAXLINE);
        Rio_readlineb(&rio, buf + (count * MAXLINE), MAXLINE);
        count += 1;
    }

    /* this should actually be writing to the server fd, not the client fd, we need to open a connection to the server here like is done in echoclient*/
    rio_writen(serverfd, buf, strlen(buf));
    printf("%s", buf);
    
    
    /* Read headers into head buf*/
     
     //THIS LOOP IS WRITTEN POORLY
    //headbuf = (char*) calloc(MAXLINE, sizeof(char));
     while(strcmp(headbuf, "\r\n")) {
     	//change size of buff to allow full line to be written in ...should we change buff here to become a pointer??
     	
     	/*  remove  any inappropriate connection types*/
     	//CORRECT way to write these connection headers???
     	if(strstr(headbuf, "Connection: keep-alive") != NULL) {
     		//write over the contents written here, do not extend buffer size further
     		continue;
    	
		}
		if(strstr(headbuf, "Connection: connection") != NULL) {
     		//write over the contents written here, do not extend buffer size further
     		continue;
    	
		}
		if(strstr(headbuf, "Connection: proxy-connection") != NULL) {
     		//write over the contents written here, do not extend buffer size further
     		continue;
    	
		}


        Rio_readlineb(&rio, headbuf, MAXLINE);


        printf("%s", headbuf);
        /* this should actually be writing to the server fd, not the client fd, we need to open a connection to the server here like is done in echoclient*/
        /* later tho we'll want to write back to the client fd*/
        rio_writen(serverfd, headbuf, strlen(buf));
        //write this information to the server
        

        

    }

    //concatenate buf and headbuf...is this neccesary if we write the info as we go
    //strcat(buf, headbuf);
    


     
     //printf("%s", buf);
     //would we check here to make sure that the method is not keep-alive? 

     //make headbuf, concatenate to buf, rio_init and write both to the server
     




     

     read_requesthdrs(&rio);

     /* Parse URI from GET request */
     

     if (stat(filename, &sbuf) < 0) {
        client_error(fd, filename, 404, "File Not found",
        "Tiny couldn’t find this file");
        return;
     }

     
}// end doit

/*need to add in docs*/
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }

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

	if (strncasecmp(uri, "http://", 7) != 0)
		return (-1);

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


	//put newline at the end of log_str
	//str_cat(log_str, "\n");
	//Free();

    //TODO: free the memory used to store the string
    //TODO: put newline at the end of the returned string
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
