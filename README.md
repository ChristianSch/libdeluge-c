# libdeluge-c
C library for the deluge-daemon RPC api. Build with `make`.

## Status
Deluge sucks. Probably wont be finished, though my code could be helpful to someone.

Biggest problem is that with openssl sockets we can't read anything as it just gets
stuck reading and never stops. Without openssl the server returns empty answers.

Sadly the deluge documentation sucks big time and the servers are kind of down so
you'd have to rely on the google cache (yay).

## Hints
Some things to check out that could help:
* [Ruby implementation](https://github.com/mikaelwikman/deluge-ruby)
* [Another Ruby implementation](https://github.com/t3hk0d3/deluge-rpc)
* [Lua auth bruteforce](https://svn.nmap.org/nmap-exp/claudiu/scripts/deluge-rpc-brute.nse)
* [Forum entry about auth](http://webcache.googleusercontent.com/search?q=cache:0Y2PUFrK7LAJ:forum.deluge-torrent.org/viewtopic.php%3Ff%3D8%26t%3D47447&client=safari&hl=en&gl=de&strip=1&vwsrc=0)
