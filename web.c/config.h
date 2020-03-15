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


#endif

