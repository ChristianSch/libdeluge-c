# libdeluge-c
C library for the deluge-daemon RPC api. Build with `make`.

## Note
The previous version of this readme contained a rant about deluge itself as well about
its documentation and servers. I'm sorry for this, I didn't mean to offend anyone.

## Status
Currently just fiddling around. The deluge rpc protocol seems to be changing soon which
introduces a header for message size (which is a good thing though).
I am unsure whether or not this project will be maintained in the future.

## Hints
Some things to check out that could help:
* [Ruby implementation](https://github.com/mikaelwikman/deluge-ruby)
* [Another Ruby implementation](https://github.com/t3hk0d3/deluge-rpc)
* [Lua auth bruteforce](https://svn.nmap.org/nmap-exp/claudiu/scripts/deluge-rpc-brute.nse)
* [Forum entry about auth](http://webcache.googleusercontent.com/search?q=cache:0Y2PUFrK7LAJ:forum.deluge-torrent.org/viewtopic.php%3Ff%3D8%26t%3D47447&client=safari&hl=en&gl=de&strip=1&vwsrc=0)

If no SSL is used, the deluged says:
```
Deluge client disconnected: [('SSL routines', 'SSL23_GET_CLIENT_HELLO', 'unknown protocol')]
```

which seems to indicate that we need it whatsoever.
