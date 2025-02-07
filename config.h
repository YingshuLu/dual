#ifndef DUAL_CONFIG_H
#define DUAL_CONFIG_H

typedef struct {
    int listen_port;
    int tunnel_port;
    int tunnel_timeout;
    int database_rotation;
    int enable_logging;
    int ad_shield;
    char** enforced_domains;
    int enforced_domains_count;
} configuration;

const configuration* config();

int parse_configuration(const char* config_file);

#endif