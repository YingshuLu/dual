#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "cJSON.h"
#include "libcask/co_define.h"

#define TPROXY_TUNNEL_TIMEOUT 60
#define TPROXY_BIND_PORT 1234
#define TUNNEL_BIND_PORT 1212
#define DATABASE_ROTATION_PERIOD (12 * 3600 * 1000)

configuration g_config;

const configuration* config() {
    return &g_config;
}

void dump_configuration() {
    INF_LOG("========CONFIG========");
    INF_LOG("listen_port = %d", g_config.listen_port);
    INF_LOG("tunnel_port = %d", g_config.tunnel_port);
    INF_LOG("tunnel_timeout = %d", g_config.tunnel_timeout);
    INF_LOG("database_rotation = %d", g_config.database_rotation);
    INF_LOG("enable_logging = %d", g_config.enable_logging);

    INF_LOG("enforced_domains:");
    for (int i = 0; i < g_config.enforced_domains_count; i++) {
        const char * domain = g_config.enforced_domains[i];
        if (domain) {
            INF_LOG("\t\"%s\"", domain);
        }
    }
    INF_LOG("-------CONFIG--------");
}

void init_configuration() {
    g_config.enable_logging = 0;
    g_config.enforced_domains = NULL;
    g_config.database_rotation = DATABASE_ROTATION_PERIOD;
    g_config.tunnel_timeout = TPROXY_TUNNEL_TIMEOUT;
    g_config.listen_port = TPROXY_BIND_PORT;
    g_config.tunnel_port = TUNNEL_BIND_PORT;
}

void set_configuration(cJSON* root) {
    const cJSON* listen_port = cJSON_GetObjectItem(root, "listen_port");
    if (cJSON_IsNumber(listen_port)) {
        g_config.listen_port = listen_port->valueint;
    }

    const cJSON* tunnel_port = cJSON_GetObjectItem(root, "tunnel_port");
    if (cJSON_IsNumber(tunnel_port)) {
        g_config.tunnel_port = tunnel_port->valueint;
    }

    const cJSON* tunnel_timeout = cJSON_GetObjectItem(root, "tunnel_timeout");
    if (cJSON_IsNumber(tunnel_timeout)) {
        g_config.tunnel_timeout = tunnel_timeout->valueint;
    }

    const cJSON* database_rotation = cJSON_GetObjectItem(root, "database_rotation");
    if (cJSON_IsNumber(database_rotation)) {
        g_config.database_rotation = database_rotation->valueint * 1000;
    }

    const cJSON* enable_logging = cJSON_GetObjectItem(root, "enable_logging");
    if (cJSON_IsNumber(enable_logging)) {
        g_config.enable_logging = enable_logging->valueint;
    }
        
    const cJSON* enforced_domains = cJSON_GetObjectItem(root, "enforced_domains");
    if (cJSON_IsArray(enforced_domains)) {
        int nums = cJSON_GetArraySize(enforced_domains);
        g_config.enforced_domains_count = nums;
        g_config.enforced_domains = (char**) malloc(nums * sizeof(char*));
        for (int index = 0; index < nums; index++) {
            const cJSON* item = cJSON_GetArrayItem(enforced_domains, index);
            g_config.enforced_domains[index] = NULL;
            if (cJSON_IsString(item)) {
                g_config.enforced_domains[index] = strdup(item->valuestring);
            }
        }
    }
}

int load_configuration(const char* config_file) {
    FILE *fp = fopen(config_file, "r");
    if (fp == NULL) {
        ERR_LOG("fopen error %s", config_file);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    char *buffer = (char*)malloc(fsize + 1);
    if (buffer == NULL) {
        fclose(fp);
        ERR_LOG("malloc error for config");
        return -2;
    }

    size_t result = fread(buffer, 1, fsize, fp);
    if (result != fsize) {
        fclose(fp);
        free(buffer);
        ERR_LOG("fread %s error", config_file);
        return -3;
    }

    buffer[fsize] = 0;
    fclose(fp);

    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL) {
        ERR_LOG("Error before: [%s]", cJSON_GetErrorPtr());
        return -4;
    } else {
        set_configuration(root);
        cJSON_Delete(root);
    }

    free(buffer);
    return 0;
}

int parse_configuration(const char* config_file) {
    init_configuration();
    int ret = 0;
    if (access(config_file, F_OK) == 0) {
        ret = load_configuration(config_file);
    } else {
        ERR_LOG("configuration %s not existing, use the default!!!", config_file);
    }
    dump_configuration();
    return ret;
}