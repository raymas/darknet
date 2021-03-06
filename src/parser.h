#ifndef PARSER_H
#define PARSER_H
#include "network.h"

#ifdef __cplusplus
extern "C" {
#endif
network parse_network_cfg(char *filename);
network parse_network_cfg_custom(char *filename, int batch, int time_steps);
void save_network(network net, char *filename);
void save_weights(network net, char *filename);
void save_weights_upto(network net, char *filename, int cutoff);
void save_weights_double(network net, char *filename);
void load_weights(network *net, char *filename);
void load_weights_upto(network *net, char *filename, int cutoff);
void get_date(char * buf);
void save_current_progress(char * progress, char * filename, int clear);

#ifdef __cplusplus
}
#endif
#endif
