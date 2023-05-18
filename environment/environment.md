
- C Development Environment
- POSIX REGEX routines
- Berkeley Networking (e.g., socket(2)) routines




Use CPPFLAGS and LDFLAGS.
env CPPFLAGS=-I/usr/mylocal/include LDFLAGS=-L/usr/mylocal/lib ./configure
Details are provided in the release INSTALL document
If you're linking against shared libraries and they reside in non-standard locations, you may also need to tell the runtime dynamic linker where to find them, otherwise the resulting executable files won't run. The steps are operating-system dependent but usually involve setting an environment variable before attempting to run the programs.
For Linux, Solaris, Irix, and other SVR4-derived systems you use the LD_LIBRARY_PATH environment variable. For AIX use LIBPATH, for HP-UX use SHLIB_PATH.
LD_LIBRARY_PATH=/usr/mylocal/lib; export LD_LIBRARY_PATH
