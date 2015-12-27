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
    int i = 0;
    struct WriteMsg *ret = malloc(sizeof(struct WriteMsg));

    uLong ucompSize = strlen(str) + 1;
    uLong compSize = compressBound(ucompSize);
    uLong targetSize = compSize + 5;

    /* printf("%lu/%lu/%lu len\n", ucompSize, compSize, targetSize); */

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
    /*
    printf("out:\n");
    printf("[ ");
    for (i = 0; i < compSize; i++)
        printf("%d, ", out[i]);
    printf(" ]\n");
    */

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
    BIO * bio;
    int n;
    char buffer[5000];
    SSL_CTX *ctx;
    SSL *ssl;

    struct WriteMsg *auth = encode_str("[[1,'daemon.login',[],{}]]");
    struct WriteMsg *get_torrents = encode_str("[[2,'daemon.get_torrents_status',[],{}]]");

    /* init openssl */
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_BIO_strings();

    ctx = SSL_CTX_new(SSLv23_client_method());
    assert(ctx != NULL);
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

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
    if (!SSL_CTX_load_verify_locations(ctx, "/usr/local/etc/openssl/cert.pem", NULL))
        error("Could not load trusted store");

    /* encode_str("[[1,'info',[],{}]] "); */

    ssl = SSL_new(ctx);
    assert(ssl != NULL);

    bio = BIO_new_ssl_connect(ctx);

    if (bio == NULL)
        error("Connection init error");

    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    BIO_set_conn_hostname(bio, "localhost:58846");

    if (BIO_do_connect(bio) <= 0)
        error("Connection could not be established");

    /* Guess what! Deluge's cert does not validate, or something is wrong. I dunno. */
    /*
    if (SSL_get_verify_result(ssl) != X509_V_OK)
        error("SSL cert could not be verified");
    */

    bzero(buffer, 5000);

    /* authorize */
    if ((n = BIO_write(bio, &auth->str, auth->len)) < 0)
        error("ERROR writing to socket");

    assert(n == auth->len);

    if ((n = BIO_read(bio, buffer, 255)) < 0)
        error("ERROR reading");

    printf("%dB `%s`\n", n, buffer);
    printf("%s\n", decode_str(buffer, n));

    /* get torrent list */
    bzero(buffer, 5000);
    if ((n = BIO_write(bio, &get_torrents->str, get_torrents->len)) < 0)
        error("ERROR writing");

    assert(n == get_torrents->len);

    if ((n = BIO_read(bio, buffer, 4999)) < 0)
        error("ERROR reading");

    printf("%d: %s\n", n, buffer);
    printf("%s\n", decode_str(buffer, n));

    BIO_free_all(bio);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return 0;
}
