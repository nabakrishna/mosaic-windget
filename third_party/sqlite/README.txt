This folder is where the SQLite amalgamation source lives.

tries an automatic FetchContent download as a
convenience default, but the reliable path is manual:

1. Go to https://www.sqlite.org/download.html
2. Under "Source Code", download the "sqlite-amalgamation-XXXXXXX.zip"
   (NOT the "sqlite-autoconf" one — that's a different packaging).
3. Unzip it and copy exactly two files into this folder:
     sqlite3.c
     sqlite3.h
   (there's also a sqlite3ext.h in the zip — you don't need it for this
   project, but it's harmless to leave out or include).

Once sqlite3.c and sqlite3.h are sitting directly in this folder,
CMake will detect them automatically and skip the FetchContent download
entirely — see the check at the top of the SQLite section in
CMakeLists.txt.
