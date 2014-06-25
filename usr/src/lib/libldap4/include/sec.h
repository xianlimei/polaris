/*
 *
 * Copyright 10/03/00 Sun Microsystems, Inc. 
 * All Rights Reserved
 *
 *
 * Comments:   
 *
 */

#pragma ident	"@(#)sec.h	1.4	00/10/03 SMI"

#ifndef _SEC_H_
#define _SEC_H_

#include <sys/types.h>
#include <md5.h>

void hmac_md5(unsigned char *text, int text_len, unsigned char *key,
	int key_len, unsigned char *digest);

char *hexa_print(unsigned char *aString, int aLen);
char *hexa2str(char *anHexaStr, int *aResLen);

#endif /* _SEC_H_ */