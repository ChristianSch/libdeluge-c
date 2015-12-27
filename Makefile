all:
	gcc -std=c89 -o del libdeluge.c -lz -L/usr/local/opt/openssl/lib -lssl -lcrypto -Wall -pedantic-errors
