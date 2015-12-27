#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <resolv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "zlib.h"

struct WriteMsg {
    char *str;
    uLong len;
};

void tear_down(void)
{

}

void error(char *descr)
{
    fprintf(stderr, "%s\n", descr);
    tear_down();
    exit(EXIT_FAILURE);
}

struct WriteMsg *encode_str(char* str)
{
    char *out;
    uint32_t len;
    int i = 0;
    struct WriteMsg *ret = malloc(sizeof(struct WriteMsg));

    uLong ucompSize = strlen(str) + 1;
    uLong compSize = compressBound(ucompSize);
    uLong targetSize = compSize + 5;

    printf("%lu/%lu/%lu len\n", ucompSize, compSize, targetSize);

    if (ret == NULL)
        error("Ran out of memory.");

    out = malloc(sizeof(char) * (compSize + 5));

    if (out == NULL)
        error("Ran out of memory.");

    compress((Bytef *)out + 5, &compSize, (Bytef *)str, ucompSize);

    /* the header */
    len = htonl(ucompSize);
    out[0] = 'D';
    out[1] = (len >> 0);
    out[2] = (len >> 8);
    out[3] = (len >> 16);
    out[4] = (len >> 24);

    /* debug */
    printf("out:\n");
    printf("[ ");
    for (i = 0; i < compSize; i++)
        printf("%d, ", out[i]);
    printf(" ]\n");

    /* header is 5 bytes, no NUL */
    ret->str = malloc(sizeof(char) * compSize + 5);

    if (ret->str == NULL)
        error("Ran out of memory.");

    strncpy(ret->str, out, targetSize);
    ret->len = targetSize;

    return ret;
}

char *decode_str(char* str, int len)
{
    char *out;
    uLong ucompSize = strlen(str);
    uLong compSize = len;
    out = malloc(sizeof(char) * len);

    if (out == NULL)
        error("Ran out of memory.");

    uncompress((Bytef *)out, &ucompSize, (Bytef *)str, compSize);
    return out;
}

int main(int argc, char** argv)
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[5000];

    const SSL_METHOD *method;
    SSL_CTX *ctx;
    SSL *ssl;

    struct WriteMsg *auth = encode_str("[[1,'daemon.login',[],{}]]");
    struct WriteMsg *get_torrents = encode_str("[[2,'daemon.get_torrents_status',[],{}]]");

    /* init openssl */
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    SSL_load_error_strings();

    method = SSLv23_client_method();
    assert(method != NULL);
    ctx = SSL_CTX_new(method);
    assert(ctx != NULL);
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

    /* encode_str("[[1,'info',[],{}]] "); */

    portno = 58846;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        error("No socket opened.");

    server = gethostbyname("127.0.0.1");

    if (server == NULL)
        fprintf(stderr, "No host.");

    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = *(long *)(server->h_addr);


    if (connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    ssl = SSL_new(ctx);
    assert(ssl != NULL);
    SSL_set_fd(ssl, sockfd);
    SSL_connect(ssl);

    bzero(buffer, 5000);

    /* authorize */
    printf("to write: %lu\n", auth->len);
    n = SSL_write(ssl, &auth->str, auth->len);
    //n = write(sockfd, &auth->str, auth->len); /* <- this works … kinda */
    if (n < 0)
        error("ERROR writing to socket.");
    printf("wrote %d bytes\n", n);

    n = SSL_read(ssl, buffer, 255);
    printf("got %d bytes\n", n);

    if (n < 0)
        error("ERROR reading");

    printf("%d: `%s`\n", n, buffer);
    printf("%s\n", decode_str(buffer, n));

    /* get torrent list */
    bzero(buffer, 5000);
    n = SSL_write(ssl, &get_torrents->str, get_torrents->len);

    if (n < 0)
        error("ERROR writing");

    n = SSL_read(ssl, buffer, 4999);

    if (n < 0)
        error("ERROR reading");

    printf("%d: %s\n", n, buffer);
    printf("%s\n", decode_str(buffer, n));

    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);

    return 0;
}
