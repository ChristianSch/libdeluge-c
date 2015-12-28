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
#include <openssl/bio.h>
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
    struct WriteMsg *ret;

    uLong ucompSize = strlen(str);
    uLong compSize = compressBound(ucompSize);
	/* 5 bytes header + string */
    uLong targetSize = compSize + 5;

    if ((ret = malloc(sizeof(struct WriteMsg))) == NULL)
        error("Ran out of memory");

    if ((out = malloc(sizeof(char) * targetSize)) == NULL)
        error("Ran out of memory");

	/* the header. 'D' is the first byte. the last 4 byte represent
     * the emssage size *prior* to deflating.
     */
    len = htonl(ucompSize);
    out[0] = 'D';
    out[1] = (len >> 0);
    out[2] = (len >> 8);
    out[3] = (len >> 16);
    out[4] = (len >> 24);

    if (compress((Bytef *)out + 5, &compSize, (Bytef *)str, ucompSize) != Z_OK)
        error("Could not deflate message");

    if ((ret->str = malloc(sizeof(char) * targetSize)) == NULL)
        error("Ran out of memory");

	bzero(ret->str, targetSize);

    memcpy(ret->str, out, targetSize);
    ret->len = targetSize;

    return ret;
}

char *decode_str(char* str, int n)
{
	uLong len;
    char *out;

	/* check the header */
	assert('D' == str[0]);

	/* FIXME: make me portable; assuming network byte order here */
	len = (str[1] << 24) | (str[2] << 16) | (str[3] << 8) | str[4];

    if ((out = malloc(sizeof(char) * (len + 1))) == NULL)
        error("Ran out of memory.");

	bzero(out, len + 1);
    if (uncompress((Bytef *)out, &len, (Bytef *)str + 5, n) != Z_OK)
        error("Could not inflate message");

    return out;
}

int main(int argc, char** argv)
{
    BIO *sbio;
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[5000];
    SSL_CTX *ctx;
    SSL *ssl;

    struct WriteMsg *auth = encode_str("[[1,'daemon.login',[],{}]]");
    struct WriteMsg *get_torrents = encode_str("[[2,'daemon.get_torrents_status',[],{}]]");

    /* init openssl */
    SSL_library_init();
    SSL_load_error_strings();

    /* setting up the openssl context */
    ctx = SSL_CTX_new(SSLv23_client_method());
    assert(ctx != NULL);

    /* NOTE: here we *should* look all around the OS for the file instead of assuming
     * that OpenSSL was installed via homebrew. And this even validates only for the time
     * being. If someone is super clever and changes the dir, we're f*cked.
     * Yeah, it's really that … weird.
     *
     * However, you could also just use the directory. I'd hint to the doc, but there is
     * none. For real. This could come in handy though:
     *  http://openssl.6102.n7.nabble.com/SSL-CTX-set-default-verify-paths-and-Windows-td25299.html
     *
     */
	/*
    if (!SSL_CTX_load_verify_locations(ctx, "/usr/local/etc/openssl/cert.pem", NULL))
        error("Could not load trusted store");
	*/

    /* init socket */
    portno = 58846;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("No socket opened.");

    if ((server = gethostbyname("127.0.0.1")) == NULL)
        fprintf(stderr, "No host.");

    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = *(long *)(server->h_addr);

    if (connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    /* connect socket to ssl and prepare BIO */
    ssl = SSL_new(ctx);
    assert(ssl != NULL);

    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    sbio = BIO_new_socket(sockfd, BIO_NOCLOSE);
    SSL_set_bio(ssl, sbio, sbio);

    /* ssl handshake */
    if (SSL_connect(ssl) < 0)
        error("SSL handshake failed");

    /*
    if (SSL_get_verify_result(ssl) != X509_V_OK)
        error("SSL cert could not be verified");
    */

    bzero(buffer, 5000);

    /* authorize */
    if ((n = SSL_write(ssl, auth->str, auth->len)) < 0)
        error("ERROR writing to socket");

	SSL_write(ssl, "\0", 1);
    assert(n == auth->len);

    /* debug */
    printf("write done\n");

    if ((n = SSL_read(ssl, buffer, 511)) < 0)
        error("ERROR reading");

    printf("%dB `%s`\n", n, buffer);
    printf("%s\n", decode_str(buffer, n));

    /* get torrent list */
    bzero(buffer, 5000);
    if ((n = SSL_write(ssl, get_torrents->str, get_torrents->len)) < 0)
        error("ERROR writing");
	SSL_write(ssl, 0, 1);

    assert(n == get_torrents->len);

    if ((n = SSL_read(ssl, buffer, 4999)) < 0)
        error("ERROR reading");

    printf("%d: %s\n", n, buffer);
    printf("%s\n", decode_str(buffer, n));

    /* clean shutdown */
    SSL_shutdown(ssl);

    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return 0;
}
