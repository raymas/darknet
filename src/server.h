#ifndef SERVER_H
#define SERVER_H

#include "anet.h"

void modesInitNet(void);
void modesAcceptClients(void);
void modesFreeClient(int fd);
void modesSendAllClients(int service, void *msg, int len);
void modesSendRawOutput(struct modesMessage *mm);
int hexDigitVal(int c);
char * performancesToJson(float * performances, int *len);
int handleHTTPRequest(struct client *c);
void modesReadFromClient(struct client *c, char *sep, int(*handler)(struct client *));
void modesReadFromClients(void);
void modesWaitReadableClients(int timeout_ms);

#endif
