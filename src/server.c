#include "server.h"

struct client {
    int fd;         /* File descriptor. */
    int service;    /* TCP port the client is connected to. */
    char buf[MODES_CLIENT_BUF_SIZE+1];    /* Read buffer. */
    int buflen;                         /* Amount of data on buffer. */
};

struct {
  /* Networking */
  char aneterr[ANET_ERR_LEN];
  struct client *clients[MODES_NET_MAX_FD]; /* Our clients. */
  int https; /* HTTP listening socket. */
  int debug;
  int net;
  long long stat_http_requests;
} Modes;


#define MODES_NET_SERVICE_HTTP 0
#define MODES_NET_SERVICES_NUM 1
struct {
    char *descr;
    int *socket;
    int port;
} modesNetServices[MODES_NET_SERVICES_NUM] = {
    {"HTTP server", &Modes.https, MODES_NET_HTTP_PORT}
};

/* Networking "stack" initialization. */
void modesInitNet(void) {
    int j;

    memset(Modes.clients,0,sizeof(Modes.clients));
    Modes.maxfd = -1;

    for (j = 0; j < MODES_NET_SERVICES_NUM; j++) {
        int s = anetTcpServer(Modes.aneterr, modesNetServices[j].port, NULL);
        if (s == -1) {
            fprintf(stderr, "Error opening the listening port %d (%s): %s\n",
                modesNetServices[j].port,
                modesNetServices[j].descr,
                strerror(errno));
            exit(1);
        }
        anetNonBlock(Modes.aneterr, s);
        *modesNetServices[j].socket = s;
    }

    signal(SIGPIPE, SIG_IGN);
}

/* This function gets called from time to time when the decoding thread is
 * awakened by new data arriving. This usually happens a few times every
 * second. */
void modesAcceptClients(void) {
    int fd, port;
    unsigned int j;
    struct client *c;

    for (j = 0; j < MODES_NET_SERVICES_NUM; j++) {
        fd = anetTcpAccept(Modes.aneterr, *modesNetServices[j].socket,
                           NULL, &port);
        if (fd == -1) {
            if (Modes.debug & MODES_DEBUG_NET && errno != EAGAIN)
                printf("Accept %d: %s\n", *modesNetServices[j].socket,
                       strerror(errno));
            continue;
        }

        if (fd >= MODES_NET_MAX_FD) {
            close(fd);
            return; /* Max number of clients reached. */
        }

        anetNonBlock(Modes.aneterr, fd);
        c = malloc(sizeof(*c));
        c->service = *modesNetServices[j].socket;
        c->fd = fd;
        c->buflen = 0;
        Modes.clients[fd] = c;
        anetSetSendBuffer(Modes.aneterr,fd,MODES_NET_SNDBUF_SIZE);

        if (Modes.maxfd < fd) Modes.maxfd = fd;
        if (*modesNetServices[j].socket == Modes.sbsos)
            Modes.stat_sbs_connections++;

        j--; /* Try again with the same listening port. */

        if (Modes.debug & MODES_DEBUG_NET)
            printf("Created new client %d\n", fd);
    }
}

/* On error free the client, collect the structure, adjust maxfd if needed. */
void modesFreeClient(int fd) {
    close(fd);
    free(Modes.clients[fd]);
    Modes.clients[fd] = NULL;

    if (Modes.debug & MODES_DEBUG_NET)
        printf("Closing client %d\n", fd);

    /* If this was our maxfd, scan the clients array to find the new max.
     * Note that we are sure there is no active fd greater than the closed
     * fd, so we scan from fd-1 to 0. */
    if (Modes.maxfd == fd) {
        int j;

        Modes.maxfd = -1;
        for (j = fd-1; j >= 0; j--) {
            if (Modes.clients[j]) {
                Modes.maxfd = j;
                break;
            }
        }
    }
}

/* Send the specified message to all clients listening for a given service. */
void modesSendAllClients(int service, void *msg, int len) {
    int j;
    struct client *c;

    for (j = 0; j <= Modes.maxfd; j++) {
        c = Modes.clients[j];
        if (c && c->service == service) {
            int nwritten = write(j, msg, len);
            if (nwritten != len) {
                modesFreeClient(j);
            }
        }
    }
}

/* Write raw output to TCP clients. */
void modesSendRawOutput(struct modesMessage *mm) {
    char msg[128], *p = msg;
    int j;

    *p++ = '*';
    for (j = 0; j < mm->msgbits/8; j++) {
        sprintf(p, "%02X", mm->msg[j]);
        p += 2;
    }
    *p++ = ';';
    *p++ = '\n';
    modesSendAllClients(Modes.ros, msg, p-msg);
}

/* Turn an hex digit into its 4 bit decimal value.
 * Returns -1 if the digit is not in the 0-F range. */
int hexDigitVal(int c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') return c-'0';
    else if (c >= 'a' && c <= 'f') return c-'a'+10;
    else return -1;
}

/* This function decodes a string representing a Mode S message in
 * raw hex format like: *8D4B969699155600E87406F5B69F;
 * The string is supposed to be at the start of the client buffer
 * and null-terminated.
 *
 * The message is passed to the higher level layers, so it feeds
 * the selected screen output, the network output and so forth.
 *
 * If the message looks invalid is silently discarded.
 *
 * The function always returns 0 (success) to the caller as there is
 * no case where we want broken messages here to close the client
 * connection. */

/* Return a description of planes in json. */
char * performancesToJson(float * performances, int *len) {
    int buflen = 1024; /* The initial buffer is incremented as needed. */
    char *buf = malloc(buflen), *p = buf;
    int l;

    l = snprintf(p,buflen,"[\n");
    p += l; buflen -= l;

    l = snprintf(p,buflen,
        "{\"current_batch\":\"%f\", \"loss\":\"%f\", \"avg_loss\":%f, "
        "\"rate\":%f, \"time\":%lf, \"nb_images\":%d, "
        "\"map\":%f},\n",
        performances[0], performances[1], performances[2],
        performances[3], performances[4], performances[5],
        performances[6]);
    p += l; buflen -= l;
    /* Resize if needed. */
    if (buflen < 256) {
        int used = p-buf;
        buflen += 1024; /* Our increment. */
        buf = realloc(buf,used+buflen);
        p = buf+used;
    }

    /* Remove the final comma if any, and closes the json array. */
    if (*(p-2) == ',') {
        *(p-2) = '\n';
        p--;
        buflen++;
    }
    l = snprintf(p,buflen,"]\n");
    p += l; buflen -= l;

    *len = p-buf;
    return buf;
}

#define MODES_CONTENT_TYPE_HTML "text/html;charset=utf-8"
#define MODES_CONTENT_TYPE_JSON "application/json;charset=utf-8"

/* Get an HTTP request header and write the response to the client.
 * Again here we assume that the socket buffer is enough without doing
 * any kind of userspace buffering.
 *
 * Returns 1 on error to signal the caller the client connection should
 * be closed. */
int handleHTTPRequest(struct client *c) {
    char hdr[512];
    int clen, hdrlen;
    int httpver, keepalive;
    char *p, *url, *content;
    char *ctype;

    if (Modes.debug & MODES_DEBUG_NET)
        printf("\nHTTP request: %s\n", c->buf);

    /* Minimally parse the request. */
    httpver = (strstr(c->buf, "HTTP/1.1") != NULL) ? 11 : 10;
    if (httpver == 10) {
        /* HTTP 1.0 defaults to close, unless otherwise specified. */
        keepalive = strstr(c->buf, "Connection: keep-alive") != NULL;
    } else if (httpver == 11) {
        /* HTTP 1.1 defaults to keep-alive, unless close is specified. */
        keepalive = strstr(c->buf, "Connection: close") == NULL;
    }

    /* Identify he URL. */
    p = strchr(c->buf,' ');
    if (!p) return 1; /* There should be the method and a space... */
    url = ++p; /* Now this should point to the requested URL. */
    p = strchr(p, ' ');
    if (!p) return 1; /* There should be a space before HTTP/... */
    *p = '\0';

    if (Modes.debug & MODES_DEBUG_NET) {
        printf("\nHTTP keep alive: %d\n", keepalive);
        printf("HTTP requested URL: %s\n\n", url);
    }

    /* Select the content to send, we have just two so far:
     * "/" -> Our google map application.
     * "/data.json" -> Our ajax request to update planes. */
    if (strstr(url, "/data.json")) {
        content = performancesToJson(&clen);
        ctype = MODES_CONTENT_TYPE_JSON;
    } else {
        struct stat sbuf;
        int fd = -1;

        if (stat("../http/dashboard.html",&sbuf) != -1 &&
            (fd = open("../http/dashboard.html",O_RDONLY)) != -1)
        {
            content = malloc(sbuf.st_size);
            if (read(fd,content,sbuf.st_size) == -1) {
                snprintf(content,sbuf.st_size,"Error reading from file: %s",
                    strerror(errno));
            }
            clen = sbuf.st_size;
        } else {
            char buf[128];

            clen = snprintf(buf,sizeof(buf),"Error opening HTML file: %s",
                strerror(errno));
            content = strdup(buf);
        }
        if (fd != -1) close(fd);
        ctype = MODES_CONTENT_TYPE_HTML;
    }

    /* Create the header and send the reply. */
    hdrlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Server: Dump1090\r\n"
        "Content-Type: %s\r\n"
        "Connection: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        ctype,
        keepalive ? "keep-alive" : "close",
        clen);

    if (Modes.debug & MODES_DEBUG_NET)
        printf("HTTP Reply header:\n%s", hdr);

    /* Send header and content. */
    if (write(c->fd, hdr, hdrlen) != hdrlen ||
        write(c->fd, content, clen) != clen)
    {
        free(content);
        return 1;
    }
    free(content);
    Modes.stat_http_requests++;
    return !keepalive;
}

/* This function polls the clients using read() in order to receive new
 * messages from the net.
 *
 * The message is supposed to be separated by the next message by the
 * separator 'sep', that is a null-terminated C string.
 *
 * Every full message received is decoded and passed to the higher layers
 * calling the function 'handler'.
 *
 * The handelr returns 0 on success, or 1 to signal this function we
 * should close the connection with the client in case of non-recoverable
 * errors. */
void modesReadFromClient(struct client *c, char *sep,
                         int(*handler)(struct client *))
{
    while(1) {
        int left = MODES_CLIENT_BUF_SIZE - c->buflen;
        int nread = read(c->fd, c->buf+c->buflen, left);
        int fullmsg = 0;
        int i;
        char *p;

        if (nread <= 0) {
            if (nread == 0 || errno != EAGAIN) {
                /* Error, or end of file. */
                modesFreeClient(c->fd);
            }
            break; /* Serve next client. */
        }
        c->buflen += nread;

        /* Always null-term so we are free to use strstr() */
        c->buf[c->buflen] = '\0';

        /* If there is a complete message there must be the separator 'sep'
         * in the buffer, note that we full-scan the buffer at every read
         * for simplicity. */
        while ((p = strstr(c->buf, sep)) != NULL) {
            i = p - c->buf; /* Turn it as an index inside the buffer. */
            c->buf[i] = '\0'; /* Te handler expects null terminated strings. */
            /* Call the function to process the message. It returns 1
             * on error to signal we should close the client connection. */
            if (handler(c)) {
                modesFreeClient(c->fd);
                return;
            }
            /* Move what's left at the start of the buffer. */
            i += strlen(sep); /* The separator is part of the previous msg. */
            memmove(c->buf,c->buf+i,c->buflen-i);
            c->buflen -= i;
            c->buf[c->buflen] = '\0';
            /* Maybe there are more messages inside the buffer.
             * Start looping from the start again. */
            fullmsg = 1;
        }
        /* If our buffer is full discard it, this is some badly
         * formatted shit. */
        if (c->buflen == MODES_CLIENT_BUF_SIZE) {
            c->buflen = 0;
            /* If there is garbage, read more to discard it ASAP. */
            continue;
        }
        /* If no message was decoded process the next client, otherwise
         * read more data from the same client. */
        if (!fullmsg) break;
    }
}

/* Read data from clients. This function actually delegates a lower-level
 * function that depends on the kind of service (raw, http, ...). */
void modesReadFromClients(void) {
    int j;
    struct client *c;

    for (j = 0; j <= Modes.maxfd; j++) {
        if ((c = Modes.clients[j]) == NULL) continue;
        if (c->service == Modes.ris)
            modesReadFromClient(c,"\n",decodeHexMessage);
        else if (c->service == Modes.https)
            modesReadFromClient(c,"\r\n\r\n",handleHTTPRequest);
    }
}

/* This function is used when "net only" mode is enabled to know when there
 * is at least a new client to serve. Note that the dump1090 networking model
 * is extremely trivial and a function takes care of handling all the clients
 * that have something to serve, without a proper event library, so the
 * function here returns as long as there is a single client ready, or
 * when the specified timeout in milliesconds elapsed, without specifying to
 * the caller what client requires to be served. */
void modesWaitReadableClients(int timeout_ms) {
    struct timeval tv;
    fd_set fds;
    int j, maxfd = Modes.maxfd;

    FD_ZERO(&fds);

    /* Set client FDs */
    for (j = 0; j <= Modes.maxfd; j++) {
        if (Modes.clients[j]) FD_SET(j,&fds);
    }

    /* Set listening sockets to accept new clients ASAP. */
    for (j = 0; j < MODES_NET_SERVICES_NUM; j++) {
        int s = *modesNetServices[j].socket;
        FD_SET(s,&fds);
        if (s > maxfd) maxfd = s;
    }

    tv.tv_sec = timeout_ms/1000;
    tv.tv_usec = (timeout_ms%1000)*1000;
    /* We don't care why select returned here, timeout, error, or
     * FDs ready are all conditions for which we just return. */
    select(maxfd+1,&fds,NULL,NULL,&tv);
}
