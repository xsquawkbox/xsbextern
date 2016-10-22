/*
 * http_lib.c : http-tiny implementation
 *
 * Copyright (c) 1996 Observatoire de Paris - Meudon - France
 * Copyright (c) 1998 Laurent Demailly - http://www.demailly.com/~dl/
 * Copyright (c) 2016 Christopher Collins
 *
 * see LICENSE for terms, conditions and DISCLAIMER OF ALL WARRANTIES
 *
 * Originally written by L. Demailly.
 *
 * Changes: 
 * 2016-Oct-22
 *  + Major refactor to make reentrant, and reduce continous passing of data between
 *    caller and callee.
 *  + Rewriting of some of the internal documentation to reflect better upon the HTTP 
 *    standard
 *  + All function definitions updated from K&R style to ANSI
 *  + Fixed various easily-fixable buffer overruns.
 *  + Winsock support
 *  + IPv6 support
 *  + Fixed some header parsing bugs.
 *  + Refactored some code to make it more readable
 *  + half-rewrote the URI parser so it doesn't try to modify the passed URI string.
 *
 * Original hsitory continues below.
 *
 * $Id: http_lib.c,v 3.5 1998/09/23 06:19:15 dl Exp $ 
 *
 * Description : Use http protocol, connects to server to echange data
 *
 * $Log: http_lib.c,v $
 * Revision 3.5  1998/09/23 06:19:15  dl
 * portability and http 1.x (1.1 and later) compatibility
 *
 * Revision 3.4  1998/09/23 05:44:27  dl
 * added support for HTTP/1.x answers
 *
 * Revision 3.3  1996/04/25 19:07:22  dl
 * using intermediate variable for htons (port) so it does not yell
 * on freebsd  (thx pp for report)
 *
 * Revision 3.2  1996/04/24  13:56:08  dl
 * added proxy support through http_proxy_server & http_proxy_port
 * some httpd *needs* cr+lf so provide them
 * simplification + cleanup
 *
 * Revision 3.1  1996/04/18  13:53:13  dl
 * http-tiny release 1.0
 *
 *
 */

#define VERBOSE

/* http_lib - Http data exchanges mini library.
 */

#ifdef _WIN32
/* Windows Systems */
#include <winsock2.h>
#include <ws2tcpip.h>
#define strncasecmp(a,b,n) _strnicmp(a,b,n)

#else
/* POSIX Systems */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

typedef int SOCKET;
#define closesocket(x) close(x)

#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "http_lib.h"

/*
 * read a line from file descriptor
 * returns the number of bytes read. negative if a read error occured
 * before the end of line or the max.
 * cariage returns (CR) are ignored.
*/
static int http_read_line (
	SOCKET fd, /* file descriptor to read from */
	char *buffer, /* placeholder for data */
	int max) /* max number of bytes to read */
{ 
/* not efficient on long lines (multiple unbuffered 1 char reads) */
	int n=0;

	while (n<max) {
		if (recv(fd,buffer,1, 0)!=1) {
			n= -n;
			break;
		}
		n++;
		if (*buffer=='\015') {
		/* ignore CR */
			continue;
		}
		if (*buffer=='\012') {
		/* LF is the separator */      
			break;    
		}
		buffer++;
	}
	*buffer='\0';

	return n;
}


/*
 * read data from file descriptor
 * retries reading until the number of bytes requested is read.
 * returns the number of bytes read. negative if a read error (EOF) occured
 * before the requested length.
*/
static int http_read_buffer (
	SOCKET fd,      /* file descriptor to read from */
	char *buffer,   /* placeholder for data */
	int length)     /* number of bytes to read */
{
	int n,r;

	for (n=0; n<length; n+=r) {
		r=recv(fd,buffer,length-n,0);
		if (r<=0) {
			return -n;
		}
		buffer+=r;
	}

	return n;
}

/* beware that filename+type+rest of header must not exceed MAXBUF */
/* so we limit filename to 256 and type to 64 chars in put & get */
#ifndef MAXBUF
#define MAXBUF 512
#endif /* #ifndef MAXBUF */

/* Generalised HTTP Query
 *
 * Sends a HTTP Verb, with optional additional headers and body to the HTTP server.
 *
 * If keep_open is set, then pfd must be set to a SOCKET to recieve the open socket
 * to recieve the body from the server on.
 *
 * @param req A populated http_req structure
 * @param method HTTP Method verb
 * @param additional_header additional headers transmitted before body.
 * @param keep_open If true, keep the socket open (and return via pfd), otherwise close the socket once the request is complete.
 * @param body Request body to send. If NULL, an empty body is used
 * @param length length of the body.
 * @param pfd Pointer to a SOCKET to populate with the open socket if keep_open is set
*/
static http_retcode 
http_query(struct http_req *req, char *method, char *additional_header,
	BOOL keep_open, char *body, size_t length, SOCKET *pfd)
{
	SOCKET			s;
	struct sockaddr_storage	server;
	char			header[MAXBUF];
	size_t 			hlg;
	http_retcode 		ret;
	struct addrinfo 	addrhints;
	struct addrinfo 	*addrinfo = NULL;
	struct addrinfo 	*thisaddr = NULL;
	int  			proxy = (req->proxy_server!=NULL && req->proxy_port!=0);
	int  			port = (proxy?req->proxy_port:req->port);

	/* if we're being asked to keep the socket open, the caller MUST
	 * be prepared to pick up the socket, otherwise we'll leak sockets/fds
	*/
	if (keep_open) {
		assert(pfd != NULL);
	}

	memset(&addrhints, 0, sizeof(addrhints));
	addrhints.ai_flags = AI_ADDRCONFIG;
	/* use getaddrinfo to do the lookup */
	if (getaddrinfo(proxy?req->proxy_server:req->server, NULL, &addrhints, &addrinfo)) {
		return ERRHOST;
	}
	
	for (thisaddr = addrinfo; thisaddr != NULL; thisaddr = thisaddr->ai_next) {
		if (thisaddr->ai_family != AF_INET && thisaddr->ai_family != AF_INET6) {
			continue;
		}
		break;
	}
	if (thisaddr == NULL) {
		ret = ERRHOST;
		goto earlybail;
	}
	memcpy(&server, addrinfo->ai_addr, addrinfo->ai_addrlen);
	switch (thisaddr->ai_family) {
		struct sockaddr_in	*s_in4;
		struct sockaddr_in6	*s_in6;
	case AF_INET:
		s_in4 = (struct sockaddr_in *)&server;
		s_in4->sin_port = htons(port);
		break;
	case AF_INET6:
		s_in6 = (struct sockaddr_in6 *)&server;
		s_in6->sin6_port = htons(port);
		break;
	}

	/* create socket */
	if ((s = socket(thisaddr->ai_family, SOCK_STREAM, 0)) < 0) {
		ret = ERRSOCK;
		goto earlybail;
	}

	/* connect to server */
	if (connect(s, (struct sockaddr *)&server, thisaddr->ai_addrlen) < 0) {
		ret = ERRCONN;
		goto bail;
	}

	/* create header */
	if (proxy) {
		snprintf(header, MAXBUF,
			"%s http://%.128s:%d/%.256s HTTP/1.0\015\012User-Agent: %s\015\012%s\015\012",
			method, req->server, req->port, req->pathname, req->user_agent, additional_header);
	} else {
		snprintf(header, MAXBUF,
			"%s /%.256s HTTP/1.0\015\012User-Agent: %s\015\012%s\015\012",
			method, req->pathname, req->user_agent, additional_header);
	}
	header[MAXBUF-1] = '\0';

	hlg=strlen(header);

	if (send(s, header, hlg, 0) != hlg) {
		/* short write sending header */
		ret = ERRWRHD;
		goto bail;
	}

	if ((length>0 && body != NULL) && (send(s, body, length, 0) != length)) {
		/* short write sending data */
		ret = ERRWRDT;
		goto bail;
	} 

	/* everything went OK - read result & check */
	ret = http_read_line(s, header, MAXBUF-1);
	if (ret<=0) {
		/* something blew up reading from the socket */
		ret = ERRRDHD;
	} else if (sscanf(header, "HTTP/1.%*d %03d", (int*)&ret) != 1) {
		ret = ERRPAHD;
	} else if (keep_open) {
		*pfd = s;
	}
bail:
	/* close socket */
	if (!keep_open || ret < 0) {
		closesocket(s);
	}
earlybail:
	if (NULL != addrinfo) {
		freeaddrinfo(addrinfo);
	}
	return ret;
}


/*
 * Put data on the server
 *
 * This function sends data to the http data server.
 * The data will be stored under the ressource name filename.
 * returns a negative error code or a positive code from the server
 *
 * limitations: filename is truncated to first 256 characters 
 *              and type to 64.
 *
 * @param req req structure for the request to perform
 * @param data pointer to the data to send
 * @param length length of the data to send
 * @param overwrite lag to request to overwrite the ressource if it already existed
 * @param type type of the data, if NULL default type is used
*/
http_retcode 
http_put(struct http_req *req, char *data, size_t length, int overwrite, char *type)
{
	char header[MAXBUF];

	if (type) {
		snprintf(header, MAXBUF, "Content-length: %d\015\012Content-type: %.64s\015\012%s",
			(int)length, type, overwrite ? "Control: overwrite=1\015\012" : "");
	} else {
		snprintf(header, MAXBUF, "Content-length: %d\015\012%s", (int)length,
			overwrite ? "Control: overwrite=1\015\012" : "");
	}
	header[MAXBUF-1] = '\0';

	return http_query(req, "PUT", header, FALSE, data, length, NULL);
}

/*
 * Get data from the server
 *
 * This function gets data from the http data server.
 * The data is read from the ressource named filename.
 * Address of new new allocated memory block is filled in pdata
 * whose length is returned via plength.
 * 
 * returns a negative error code or a positive code from the server
 * 
 * limitations: filename is truncated to first 256 characters
 *
 * @param filename Name/URI of the ressource to read
 * @param pdata address of a pointer variable which will be set to point 
 * 	toward allocated memory containing read data.
 * @param plength address of integer variable which will be set to
 *	length of the read data 
 * @param typebuf allocated buffer where the read data type is returned.  
 * 	If NULL, the type is not returned
*/

http_retcode 
http_get(struct http_req *req, char **pdata, size_t *plength, char *typebuf)
{
	http_retcode	ret;
	char 		header[MAXBUF];
	char 		*pc;
	SOCKET 		fd;
	int  		n, length=-1;

	if (NULL == pdata) {
		return ERRNULL;
	}
	*pdata = NULL;

	if (NULL != plength) {
		*plength = 0;
	}
	if (NULL != typebuf) {
		*typebuf = '\0';
	}

	ret = http_query(req, "GET", "", TRUE, NULL, 0, &fd);

	if (ret != 200) {
		if (ret >= 0) {
			closesocket(fd);
		}
		return ret;
	}

	while (1) {
		n = http_read_line(fd,header,MAXBUF-1);

		if (n<=0) {
			closesocket(fd);
			return ERRRDHD;
		}

		if (n>0 && (*header == '\0')) {
			/* empty line ? (=> end of header) */
			break;
		}
	
		/* try to parse some keywords : */
		/* convert to lower case 'till a : is found or end of string */
		for (pc = header; (*pc != ':' && *pc != '\0'); pc++) {
			*pc = tolower(*pc);
		}
		sscanf(header,"content-length: %d",&length);
		if (typebuf) {
			sscanf(header,"content-type: %s",typebuf);
		}
	}

	if (length <= 0) {
		closesocket(fd);
		return ERRNOLG;
	}
	
	if (NULL != plength) {
		*plength=length;
	}
	
	*pdata = malloc(length);
	if (NULL == *pdata) {
		closesocket(fd);
		return ERRMEM;
	}

	n = http_read_buffer(fd, *pdata, length);
	closesocket(fd);
	if (n!=length) {
		return ERRRDDT;
	}
	return ret;
     }


/*
 * Request the header
 *
 * This function outputs the header of thehttp data server.
 * The header is from the ressource named filename.
 * The length and type of data is eventually returned (like for http_get(3))
 *
 * returns a negative error code or a positive code from the server
 * 
 * limitations: filename is truncated to first 256 characters
 *
 * @param filename name of the ressource to read
 * @param plength address of integer variable which will be set to
 *	length of the data
 * @param typebuf allocated buffer where the data type is returned.
 *	If NULL, the type is not returned 
*/

http_retcode 
http_head(struct http_req *req, size_t *plength, char *typebuf)
{
	/* mostly copied from http_get : */
	http_retcode ret;

	char header[MAXBUF];
	char *pc;
	SOCKET fd;
	int  n,length=-1;

	if (NULL != plength) {
		*plength = 0;
	}

	if (NULL != typebuf) {
		*typebuf='\0';
	}

	ret = http_query(req, "HEAD", "", TRUE, NULL, 0, &fd);
	if (ret != 200) {
		if (ret >= 0) {
			closesocket(fd);
		}
		return ret;
	}
	while (1) {
		n=http_read_line(fd,header,MAXBUF-1);
		if (n<=0) {
			closesocket(fd);
			return ERRRDHD;
		}

		/* empty line ? (=> end of header) */
		if ( n>0 && (*header)=='\0') break;

      		/* try to parse some keywords : */
      		/* convert to lower case 'till a : is found or end of string */
		for (pc=header; (*pc!=':' && *pc) ; pc++) {
			*pc=tolower(*pc);
		}
		sscanf(header, "content-length: %d", &length);
		if (typebuf) {
			sscanf(header, "content-type: %s", typebuf);
		}
	}
	if (plength) {
		*plength=length;	
	}
	closesocket(fd);
	return ret;
}

/*
 * Delete data on the server
 *
 * This function request a DELETE on the http data server.
 *
 * returns a negative error code or a positive code from the server
 *
 * limitations: filename is truncated to first 256 characters 
 *
 * @param filename name of the ressource to create
*/

 http_retcode http_delete(struct http_req *req) 
 {
	return http_query(req, "DELETE", "", FALSE, NULL, 0, NULL);
 }

/* parses an url : setting the http_server and http_port global variables
 * and returning the filename to pass to http_get/put/...
 * returns a negative error code or 0 if sucessfully parsed.
 *
 * @param req a (used or zero-initialied) http_req to populate with the request data
 * @param url the URL
 * @return 0 for success, <0 for failure (indicating the cause)
*/
int
http_parse_url(struct http_req *req, const char *url)
{
	char *ourUrl = strdup(url);
	int offs;

	assert(req != NULL);

	req->port = 80;
	if (req->server) {
		free(req->server);
		req->server=NULL;
	}
	if (req->pathname) {
		free(req->pathname);
		req->pathname = NULL;
	}
	/* Filter URLs for http only at this stage. */
	if (strncasecmp("http://",ourUrl,7)) {
		return ERRURLH;
	}

	ourUrl += 7;

	/* scan forward from the end of the protocol to the port or start of the path */	
	offs = strcspn(ourUrl, ":/");

	req->server = malloc(offs+1);
	assert(req->server != NULL);
	strncpy(req->server, ourUrl, offs);
	req->server[offs] = '\0';

	ourUrl += offs + 1;
	if (ourUrl[-1] == ':') {
		/* ':' delimiter.  port number follows */
		if (sscanf(ourUrl,"%d", &req->port)!=1) {
			return ERRURLP;
		}		
		offs = strcspn(ourUrl, "/");
		ourUrl += offs + 1;
	}
	req->pathname = strdup(ourUrl);
	
	return 0;
}

/* release any resources held by the http_req structure
*/
void		
http_freereq(struct http_req *req)
{
	if (req->server != NULL) {
		free(req->server);
		req->server = NULL;
	}
	if (req->proxy_server != NULL) {
		free(req->proxy_server);
		req->proxy_server = NULL;
	}
	if (req->pathname) {
		free(req->pathname);
		req->pathname = NULL;
	}
}
