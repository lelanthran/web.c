#ifndef H_CONFIG
#define H_CONFIG

/* This file contains all the configuration defaults for the server.
 */

// This is the port that will be listened on if the user does not
// specify a port.
#define DEFAULT_LISTEN_PORT      (80)

// The maximum number of connections to hold in the queue while we
// start up the thread to service a client.
#define DEFAULT_BACKLOG          (50)

// The timeout in seconds to wait for a new connection before checking
// if the user/OS requested a shutdown of the process.
#define TIMEOUT_TO_SHUTDOWN      (1)

// The maximum line length for HTTP requests and HTTP headers. Most
// webservers impose a maximum length of 4096 bytes for each line in the
// request or the header. This is usually sufficient.
// #define MAX_HTTP_LINE_LENGTH     (4096)

// The maximum number of headers to process before giving up.
#define MAX_HTTP_HEADERS         (125)

// The default web-root directory. Note that this can be either absolute
// (using a leading "/") or relative to the current directory.
#define DEFAULT_WEB_ROOT         ("www-root")

// The default patterns that we handle
#define EXTENSION_HTML           (".html")
#define EXTENSION_TEXT           (".txt")
#define EXTENSION_DIR            ("/")
#define EXTENSION_NONE           ("")


// Do we follow links or not? This is a dangerous one as it allows a careless
// administrator to let the client go below the directories in the webserver
// root.
#define FOLLOW_SYMLINKS          (1)


// The default index file to use when the client does a GET on a directory
// name.
#define DEFAULT_INDEX_FILE       ("index.html")


// The application identifier, and the version string
#define APPLICATION_ID           "Web.c"
#define VERSION_STRING           "0.0.1"


#endif

