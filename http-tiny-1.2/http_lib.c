/*
 *  Http put/get mini lib
 *  written by L. Demailly
 *  updated by C. Collins and made more portable
 *  (c) 1998 Laurent Demailly - http://www.demailly.com/~dl/
 *  (c) 1996 Observatoire de Paris - Meudon - France
 *  see LICENSE for terms, conditions and DISCLAIMER OF ALL WARRANTIES
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

/* pointer to a mallocated string containing server name or NULL */
char *http_server=NULL ;
/* server port number */
int  http_port=5757;
/* pointer to proxy server name or NULL */
char *http_proxy_server=NULL;
/* proxy server port number or 0 */
int http_proxy_port=0;
/* user agent id string */

static char *http_user_agent="XSB/2.0";

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


typedef enum querymode_e {
	CLOSE,          /* Close the socket after the query (for put) */
	KEEP_OPEN       /* Keep it open */
} querymode;

/* beware that filename+type+rest of header must not exceed MAXBUF */
/* so we limit filename to 256 and type to 64 chars in put & get */
#ifndef MAXBUF
#define MAXBUF 512
#endif /* #ifndef MAXBUF */

/*
 * Pseudo general http query
 *
 * send a command and additional headers to the http server.
 * optionally through the proxy (if http_proxy_server and http_proxy_port are
 * set).
 *
 * Limitations: the url is truncated to first 256 chars and
 * the server name to 128 in case of proxy request.
 *
 * @param command HTTP Method to use
 * @param url URL/filename queried 
 * @param additional_header additional headers transmitted before body.
 * @param mode flag to indicate if http_query should close the socket or not.
 * @param data Body to send. If NULL, an empty body is used
 * @param length size of data.
 * @param pfd Pointer to a SOCKET to populate with the open socket if mode is KEEP_OPEN
*/
static http_retcode 
http_query(char *command, char *url, char *additional_header,
	querymode mode, char *data, size_t length, SOCKET *pfd)
{
	SOCKET     s;
	struct hostent        *hp;
	struct sockaddr_in    server;
	char header[MAXBUF];
	size_t hlg;
	http_retcode ret;
	int  proxy = (http_proxy_server!=NULL && http_proxy_port!=0);
	int  port = (proxy?http_proxy_port:http_port);

	/* if we're being asked to keep the socket open, the caller MUST
	 * be prepared to pick up the socket, otherwise we'll leak sockets/fds
	*/
	if (mode == KEEP_OPEN) {
		assert(pfd != NULL);
	}

	/* get host info by name :*/
	hp = gethostbyname(proxy?http_proxy_server:http_server);
	if (hp == NULL) {
		return ERRHOST;
	}
	memset(&server, 0, sizeof(server));
	memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
	server.sin_family = hp->h_addrtype;
	server.sin_port = htons(port);

	/* create socket */
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return ERRSOCK;
	}

	//FIXME: eh, WHY?!  disable for now.
	//setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, 0, 0);

	/* connect to server */
	if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
		ret = ERRCONN;
		goto bail;
	}

	/* create header */
	if (proxy) {
		sprintf(header,
			"%s http://%.128s:%d/%.256s HTTP/1.0\015\012User-Agent: %s\015\012%s\015\012",
			command, http_server, http_port, url, http_user_agent, additional_header);
	} else {
		sprintf(header,
			"%s /%.256s HTTP/1.0\015\012User-Agent: %s\015\012%s\015\012",
			command, url, http_user_agent, additional_header);
	}

	hlg=strlen(header);

	if (send(s, header, hlg, 0) != hlg) {
		/* short write sending header */
		ret = ERRWRHD;
		goto bail;
	}

	if ((length && data) && (send(s, data, length, 0) != length)) {
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
	} else if (mode==KEEP_OPEN) {
		*pfd = s;
	}
bail:
	/* close socket */
	if (mode == CLOSE || ret < 0) {
		closesocket(s);
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
 * @param filename name of the ressource to create
 * @param data pointer to the data to send
 * @param length length of the data to send
 * @param overwrite lag to request to overwrite the ressource if it already existed
 * @param type type of the data, if NULL default type is used
*/
http_retcode 
http_put(char *filename, char *data, size_t length, int overwrite, char *type)
{
	char header[MAXBUF];

	if (type) {
		sprintf(header, "Content-length: %d\015\012Content-type: %.64s\015\012%s",
			(int)length, type, overwrite ? "Control: overwrite=1\015\012" : "");
	} else {
		sprintf(header, "Content-length: %d\015\012%s", (int)length,
			overwrite ? "Control: overwrite=1\015\012" : "");
	}
	return http_query("PUT", filename, header, CLOSE, data, length, NULL);
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
http_get(char *filename, char **pdata, size_t *plength, char *typebuf)
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

	ret = http_query("GET", filename, "", KEEP_OPEN, NULL, 0, &fd);

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
http_head(char *filename, size_t *plength, char *typebuf)
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

	ret = http_query("HEAD", filename, "", KEEP_OPEN, NULL, 0, &fd);
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

 http_retcode http_delete(char *filename) 
 {
	return http_query("DELETE", filename, "", CLOSE, NULL, 0, NULL);
 }

/* parses an url : setting the http_server and http_port global variables
 * and returning the filename to pass to http_get/put/...
 * returns a negative error code or 0 if sucessfully parsed.
 *
 * @param url writeable copy of an url
 * @param pfilename address of a pointer that will be filled with allocated filename
 * 	the pointer must be equal to NULL before calling or it will be automatically 
 *	freed (free(3))
*/
http_retcode 
http_parse_url(char *url, char **pfilename)
{
	char *pc,c;

	http_port=80;
	if (http_server) {
		free(http_server);
		http_server=NULL;
	}
	if (*pfilename) {
		free(*pfilename);
		*pfilename=NULL;
	}

	if (strncasecmp("http://",url,7)) {
		return ERRURLH;
	}
	url+=7;
	for (pc=url,c=*pc; (c && c!=':' && c!='/');) c=*pc++;
		*(pc-1)=0;
	if (c==':') {
		if (sscanf(pc,"%d",&http_port)!=1) {
			return ERRURLP;
		}
		for (pc++; (*pc && *pc!='/') ; pc++) ;
			if (*pc) pc++;
	}

	http_server = strdup(url);
	*pfilename = strdup ( c ? pc : "") ;

	return OK0;
}

