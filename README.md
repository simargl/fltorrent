# FLTK Torrent Client

A lightweight BitTorrent client for Linux built with **FLTK** and **libtorrent-rasterbar**.

## Build

```bash
sudo apt install libtorrent-rasterbar-dev libfltk1.3-dev libboost-thread-dev

g++ fltorrent.cxx -o fltorrent \
$(fltk-config --cxxflags --ldflags) \
-ltorrent-rasterbar -lboost_system -lboost_thread -lpthread
```

## Run

```bash
./fltorrent
```

## Usage

1. Paste a magnet link.
2. Click **Download**.
3. Files are saved to `./downloads`.

Displays download progress, speeds, peers, seeders, and seeding status.
