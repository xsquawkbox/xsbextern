
/*
 * http_lib.h : http-tiny declarations
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
 * 2016-Oct-22 - CC
 *  + Protype changes to reflect major refactor (see http_lib.c)
 *  + Fixed to make safe to include directly into C++ code.
 *  + Documentation Updates
 *
 * Original history below:
 *
 * $Id: http_lib.h,v 1.4 1998/09/23 06:14:15 dl Exp $
 *
*/

#ifndef _HTTP_LIB_H
#define _HTTP_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

struct http_req {
	char 		*server;
	int		port;

	char		*proxy_server;
	int		proxy_port;

	char		*user_agent;

	char		*pathname;
};

/* return type */
typedef enum http_recode_e {
	/* Client side errors */
	ERRHOST=-1, /* No such host */
	ERRSOCK=-2, /* Can't create socket */
	ERRCONN=-3, /* Can't connect to host */
	ERRWRHD=-4, /* Write error on socket while writing header */
	ERRWRDT=-5, /* Write error on socket while writing data */
	ERRRDHD=-6, /* Read error on socket while reading result */
	ERRPAHD=-7, /* Invalid answer from data server */
	ERRNULL=-8, /* Null data pointer */
	ERRNOLG=-9, /* No/Bad length in header */
	ERRMEM=-10, /* Can't allocate memory */
	ERRRDDT=-11,/* Read error while reading data */
	ERRURLH=-12,/* Invalid url - must start with 'http://' */
	ERRURLP=-13,/* Invalid port in url */

	/* Return code by the server */
	ERR400=400, /* Invalid query */
	ERR403=403, /* Forbidden */
	ERR408=408, /* Request timeout */
	ERR500=500, /* Server error */
	ERR501=501, /* Not implemented */
	ERR503=503, /* Service overloaded */

	/* Succesful results */
	OK0 = 0,   /* successfull parse */
	OK201=201, /* Ressource succesfully created */
	OK200=200  /* Ressource succesfully read */
} http_retcode;

/* prototypes */
http_retcode	http_put(struct http_req *req, char *data, size_t length, int overwrite, char *type);
http_retcode	http_get(struct http_req *req, char **pdata, size_t *plength, char *typebuf);
int 		http_parse_url(struct http_req *req, const char *url);
http_retcode 	http_delete(struct http_req *req);
http_retcode 	http_head(struct http_req *req, size_t *plength, char *typebuf);
void		http_freereq(struct http_req *req);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _HTTP_LIB_H */