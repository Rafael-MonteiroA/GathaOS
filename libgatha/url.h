#ifndef GATHA_URL_H
#define GATHA_URL_H
#define G_URL_MAX 1024
/* Original bounded HTTP(S) URI resolver. Output may alias either input. */
int g_url(const char *base,const char *reference,char out[G_URL_MAX]);
int g_url_request(const char *url,char out[G_URL_MAX]);
#endif
