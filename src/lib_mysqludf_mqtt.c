/*
    lib_mysqludf_mqtt - a library with MQTT client functions
    Copyright (C) 2021  Norbert Richter <nr@prsolution.eu>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <mysql.h>
#include <MQTTClient.h>
#include <json-parser/json.h>
#include "lib_mysqludf_mqtt.h"

#ifdef DEBUG
#include <syslog.h>
#endif  // DEBUG


volatile int last_rc = 0;
char last_func[128] = {0};

/* Helper */
const char *GetUUID(void)
{
    static char struuid[sizeof(LIBNAME) + sizeof(LIBVERSION) + UUID_LEN*2 + 16 + 1];
    char uuid[UUID_LEN*2 + 1];

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_NOTICE, "GetUUID()");
#endif
    for (int i = 0; i < UUID_LEN; i += 2)
    {
        sprintf(&uuid[i], "%02x", rand() % 256);
    }
    sprintf(struuid, "%s_%s_%s", LIBNAME, LIBVERSION, uuid);
#ifdef DEBUG
    syslog (LOG_NOTICE, "GetUUID() return %s", struuid);
    closelog ();
#endif
    return struuid;
}

void parmerror(const char *context, UDF_ARGS *args)
{
    char *type;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_NOTICE, "parmerror()");
#endif
    fprintf(stderr, "%s parameter error:\n", context);
    for (int i=0; i<args->arg_count; i++) {
        switch (args->arg_type[i]) {
            case STRING_RESULT:
                type = "STRING";
                break;
            case INT_RESULT:
                type = "INT";
                break;
            case REAL_RESULT:
                type = "RWAL";
                break;
            case DECIMAL_RESULT:
                type = "DECIMAL";
                break;
            case ROW_RESULT:
                type = "ROW";
                break;
            default:
                type = "?unknown?";
                break;
        }
        fprintf(stderr, "  arg%2d: $%p, type %s (%d), len %ld, maybe_null %c, content '%s'\n",
                i,
                args->args[i],
                type,
                args->arg_type[i],
                args->lengths[i],
                args->maybe_null[i],
                (args->arg_type[i]==STRING_RESULT && args->args[i]!=NULL) ? (char *)args->args[i] : ""
                );
    }
#ifdef DEBUG
    closelog ();
#endif
}

int get_json_value(const char *jsonstr, const char *key, int type, void *jsonvalue)
{
    int rc = JSON_OK;
    int i;

    if (NULL == jsonstr || !*jsonstr) {
        return JSON_ERROR_EMPTY_STR;
    }

    json_char *json = (json_char*)jsonstr;
    json_value *value = json_parse(json, strlen(jsonstr));
    if (value == NULL || json_object != value->type) {
        return JSON_ERROR_INVALID_STR;
    }

    for (i=0; i<value->u.object.length; i++) {
        if(0 == strcmp(key, value->u.object.values[i].name)) {
            if (type != value->u.object.values[i].value->type) {
                rc = JSON_ERROR_WRONG_TYPE;
            }
            else {
                switch (value->u.object.values[i].value->type) {
                    case json_none:
                            rc = JSON_ERROR_WRONG_VALUE;
                            break;
                    case json_null:
                            rc = JSON_ERROR_WRONG_VALUE;
                            break;
                    case json_object:
                            rc = JSON_ERROR_WRONG_VALUE;
                            break;
                    case json_array:
                            rc = JSON_ERROR_WRONG_VALUE;
                            break;
                    case json_integer:
                            *(long *)jsonvalue = (long)value->u.object.values[i].value->u.integer;
                            break;
                    case json_double:
                            rc = JSON_ERROR_WRONG_VALUE;
                            break;
                    case json_string:
                            *(char **)jsonvalue = value->u.object.values[i].value->u.string.ptr;
                            break;
                    case json_boolean:
                            *(bool *)jsonvalue = (bool)value->u.object.values[i].value->u.boolean;
                            break;
                }
                break;
            }
        }
    }
    if (i>=value->u.object.length) {
        rc = JSON_ERROR_NOT_FOUND;
    }
    json_value_free(value);
   return rc;
}

void create_conn(connection *conn, const char* username, const char*password, const char *options)
{
    char *opt_str;
    long opt_long;
    bool opt_bool;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_NOTICE, "create_conn()");
#endif
    memcpy(&conn->conn_opts, &(MQTTClient_connectOptions)MQTTClient_connectOptions_initializer, sizeof(MQTTClient_connectOptions));
    memcpy(&conn->ssl_opts, &(MQTTClient_SSLOptions)MQTTClient_SSLOptions_initializer, sizeof(MQTTClient_SSLOptions));
    memcpy(&conn->will_opts, &(MQTTClient_willOptions)MQTTClient_willOptions_initializer, sizeof(MQTTClient_willOptions));
    conn->conn_opts.ssl = &conn->ssl_opts;

    // Connection options
    if (JSON_OK == get_json_value(options, "username", json_string, &opt_str)) {
        conn->conn_opts.username = opt_str;
    }
    else {
        conn->conn_opts.username = username;
    }
    if (JSON_OK == get_json_value(options, "password", json_string, &opt_str)) {
        conn->conn_opts.password = opt_str;
    }
    else {
        conn->conn_opts.password = password;
    }
    if (JSON_OK == get_json_value(options, "keepAliveInterval", json_integer, &opt_long)) {
        conn->conn_opts.keepAliveInterval = opt_long;
    }
    else {
        conn->conn_opts.keepAliveInterval = DEFAULT_KEEPALIVEINTERVAL;
    }
    if (JSON_OK == get_json_value(options, "cleansession", json_boolean, &opt_bool)) {
        conn->conn_opts.cleansession = opt_bool;
    }
    else {
        conn->conn_opts.cleansession = 1;
    }
    if (JSON_OK == get_json_value(options, "MQTTVersion", json_integer, &opt_long)) {
        conn->conn_opts.MQTTVersion = opt_long;
    }
    else {
        conn->conn_opts.MQTTVersion = MQTTVERSION_DEFAULT;
    }
    if (JSON_OK == get_json_value(options, "reliable", json_integer, &opt_long)) {
        conn->conn_opts.reliable = opt_long;
    }
    if (JSON_OK == get_json_value(options, "connectTimeout", json_integer, &opt_long)) {
        conn->conn_opts.connectTimeout = opt_long;
    }
    if (JSON_OK == get_json_value(options, "maxInflightMessages", json_integer, &opt_long)) {
        conn->conn_opts.maxInflightMessages = opt_long;
    }

    // SSL options
    if (JSON_OK == get_json_value(options, "CApath", json_string, &opt_str)) {
        conn->ssl_opts.CApath = opt_str;
    }
    if (JSON_OK == get_json_value(options, "CAfile", json_string, &opt_str)) {
        conn->ssl_opts.trustStore = opt_str;
    }
    if (JSON_OK == get_json_value(options, "keyStore", json_string, &opt_str)) {
        conn->ssl_opts.keyStore = opt_str;
    }
    if (JSON_OK == get_json_value(options, "privateKey", json_string, &opt_str)) {
        conn->ssl_opts.privateKey = opt_str;
    }
    if (JSON_OK == get_json_value(options, "privateKeyPassword", json_string, &opt_str)) {
        conn->ssl_opts.privateKeyPassword = opt_str;
    }
    if (JSON_OK == get_json_value(options, "enabledCipherSuites", json_string, &opt_str)) {
        conn->ssl_opts.enabledCipherSuites = opt_str;
    }
    if (JSON_OK == get_json_value(options, "verify", json_boolean, &opt_bool)) {
        conn->ssl_opts.verify = opt_bool;
    }
    if (JSON_OK == get_json_value(options, "enableServerCertAuth", json_boolean, &opt_bool)) {
        conn->ssl_opts.enableServerCertAuth = opt_bool;
    }
    if (JSON_OK == get_json_value(options, "sslVersion", json_integer, &opt_long)) {
        conn->ssl_opts.sslVersion = opt_long;
    }

    // Last Will and Testament options
    if (JSON_OK == get_json_value(options, "willTopic", json_string, &opt_str)) {
        conn->will_opts.topicName = opt_str;
        conn->conn_opts.will = &conn->will_opts;
    }
    if (JSON_OK == get_json_value(options, "willMessage", json_string, &opt_str)) {
        conn->will_opts.message = opt_str;
        conn->conn_opts.will = &conn->will_opts;
    }
    if (JSON_OK == get_json_value(options, "willRetained", json_boolean, &opt_bool)) {
        conn->will_opts.retained = opt_bool;
        conn->conn_opts.will = &conn->will_opts;
    }
    if (JSON_OK == get_json_value(options, "willQos", json_integer, &opt_long)) {
        conn->will_opts.qos = opt_long;
        conn->conn_opts.will = &conn->will_opts;
    }

#ifdef DEBUG
    closelog ();
#endif
 }

/* Library functions */

/**
 * mqtt_info
 *
 * Returns udf library info as JSON string
 * mqtt_info()
 *
 */
bool mqtt_info_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 0) {
        parmerror("mqtt_info()", args);
        strcpy(message, "No arguments allowed (udf: mqtt_info)");
        return 1;
    }
    initid->ptr = malloc(MAX_RET_STRLEN+1);
    if (initid->ptr == NULL) {
        strcpy(message, "memory allocation error");
        return 1;
    }
    initid->max_length = MAX_RET_STRLEN;

    return 0;
}

void mqtt_info_deinit(UDF_INIT *initid)
{
    if (initid->ptr != NULL) {
        free(initid->ptr);
    }
}

char* mqtt_info(UDF_INIT *initid, UDF_ARGS *args, char* result, unsigned long* length, char *is_null, char *error)
{
    MQTTClient_nameValue *mqttClientVersion;
    char libinfo[MAX_RET_STRLEN] = {0};
    char *res = (char *)initid->ptr;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_NOTICE, "mqtt_info()");
#endif

    *is_null = 0;
    *error = 0;

    if (res == NULL) {
        strcpy(result, "memory allocation error");
        *length = strlen(result);
        *error = 1;
        return result;
    }

    mqttClientVersion = MQTTClient_getVersionInfo();
    while (mqttClientVersion != NULL &&
           mqttClientVersion->name != NULL &&
           mqttClientVersion->value != NULL &&
           strlen(libinfo) < MAX_RET_STRLEN-1) {
        if (*libinfo) {
            strcat(libinfo, ",");
        }
        if( (strlen(libinfo) + strlen(mqttClientVersion->name) + strlen(mqttClientVersion->value) + 5) < MAX_RET_STRLEN-1) {
            sprintf(libinfo+strlen(libinfo), "\"%s\":\"%s\"", mqttClientVersion->name, mqttClientVersion->value);
            mqttClientVersion++;
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(res, MAX_RET_STRLEN, "{\"Name\": \"%s\", \"Version\": \"%s\", \"Build\": \"%s\", \"Library\": {%s}}", LIBNAME, LIBVERSION, LIBBUILD, libinfo);
#pragma GCC diagnostic pop

    *length = strlen(res);
#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_info(): %s", res);
    closelog ();
#endif
    return res;
}

/**
 * mqtt_lasterror
 *
 * Returns last error as JSON string
 * mqtt_lasterror()
 *
 */
bool mqtt_lasterror_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 0) {
        parmerror("mqtt_lasterror()", args);
        strcpy(message, "No arguments allowed (udf: mqtt_lasterror)");
        return 1;
    }
    initid->ptr = malloc(MAX_RET_STRLEN+1);
    if (initid->ptr == NULL) {
        strcpy(message, "memory allocation error");
        return 1;
    }

    return 0;
}

void mqtt_lasterror_deinit(UDF_INIT *initid)
{
    if (initid->ptr != NULL) {
        free(initid->ptr);
    }
}

char* mqtt_lasterror(UDF_INIT *initid, UDF_ARGS *args, char* result, unsigned long* length, char *is_null, char *error)
{
    char *res = (char *)initid->ptr;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_NOTICE, "mqtt_lasterror()");
#endif

    *is_null = 0;
    *error = 0;
    if (res == NULL) {
        strcpy(result, "memory allocation error");
        *length = strlen(result);
        *error = 1;
        return result;
    }
    snprintf(res, MAX_RET_STRLEN, "{\"func\":\"%s\",\"rc\":%d, \"desc\": \"%s\"}", last_func, last_rc, MQTTClient_strerror(last_rc));
    *length = strlen(res);
#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_lasterror(): %s", res);
    closelog ();
#endif
    return res;
}


/**
 * mqtt_connect
 *
 * Connect to a mqtt server and returns a handle.
 * mqtt_connect(server {,username} {,password {,options}}})
 *      returns connect handle or 0 on error
 */
bool mqtt_connect_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    initid->ptr = NULL;
    if (   // server
        (args->arg_count == 1 && args->arg_type[0]==STRING_RESULT && args->args[0]!=NULL)
        || // server, username
        (args->arg_count == 2 && args->arg_type[0]==STRING_RESULT && args->args[0]!=NULL
                              && args->arg_type[1]==STRING_RESULT)
        || // server, username, password
        (args->arg_count == 3 && args->arg_type[0]==STRING_RESULT && args->args[0]!=NULL
                              && args->arg_type[1]==STRING_RESULT
                              && args->arg_type[2]==STRING_RESULT)
        || // server, username, password, options
        (args->arg_count == 4 && args->arg_type[0]==STRING_RESULT && args->args[0]!=NULL
                              && args->arg_type[1]==STRING_RESULT
                              && args->arg_type[2]==STRING_RESULT
                              && args->arg_type[3]==STRING_RESULT)
       ) {
        initid->ptr = malloc(sizeof(connection));
        if (initid->ptr == NULL) {
            parmerror("mqtt_connect()", args);
            strcpy(message, "memory allocation error");
            return 1;
        }
        return 0;
    }
    else {
        parmerror("mqtt_connect()", args);
        strcpy(message, "function argument(s) error");
        return 1;
    }
}

void mqtt_connect_deinit(UDF_INIT *initid)
{
    if (initid->ptr != NULL) {
        free(initid->ptr);
    }
}

ulonglong mqtt_connect(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    connection *conn = (connection *)initid->ptr;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif

    *is_null = 0;
    *error = 0;

    char *address  = "";
    char *username = "";
    char *password = "";
    char *options  = "";

    // Do not assume that the string is null-terminated
    // see https://dev.mysql.com/doc/refman/5.7/en/udf-arguments.html
    if (args->arg_count >= 1 && args->args[0]!=NULL) {
        address   = (char *)args->args[0];
        address[args->lengths[0]]  = '\0';
    }
    if (args->arg_count >= 2 && args->args[1]!=NULL) {
        username  = (char *)args->args[1];
        username[args->lengths[1]] = '\0';
    }
    if (args->arg_count >= 3 && args->args[2]!=NULL) {
        password  = (char *)args->args[2];
        password[args->lengths[2]] = '\0';
    }
    if (args->arg_count >= 4 && args->args[3]!=NULL) {
        options  = (char *)args->args[3];
        options[args->lengths[3]] = '\0';
    }

#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_connect(): MQTTClient_create \"%s\"", address);
#endif
    strcpy(last_func, "MQTTClient_create");
    int rc = last_rc = MQTTClient_create(&conn->client, address, GetUUID(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS)
    {
#ifdef DEBUG
        syslog (LOG_NOTICE, "mqtt_connect rc=%d", rc);
        closelog ();
#endif
        *is_null = 1;
        *error = 1;
        return rc;
    }

    create_conn(conn, username, password, options);

#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_connect 'client %p", conn->client);
#endif
    strcpy(last_func, "MQTTClient_connect");
    rc = last_rc = MQTTClient_connect(conn->client, &conn->conn_opts);
    if (rc != MQTTCLIENT_SUCCESS)
    {
#ifdef DEBUG
        syslog (LOG_NOTICE, "mqtt_connect rc=%d", rc);
        closelog ();
#endif
        *is_null = 1;
        *error = 1;
        return rc;
    }
#ifdef DEBUG
    closelog ();
#endif
    return (longlong)conn->client;
}





/**
 * mqtt_disconnect
 *
 * Disconnect from a mqtt server.
 * mqtt_disconnect(handle(, timeout))
 *  returns 0 if successful
 */
bool mqtt_disconnect_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if ( args->arg_count == 1
        // handle
         && args->arg_type[0]==INT_RESULT
         ) {
        return 0;
    }
    else {
        parmerror("mqtt_disconnect()", args);
        strcpy(message, "function argument(s) error");
        return 1;
    }
}

void mqtt_disconnect_deinit(UDF_INIT *initid)
{
}

ulonglong mqtt_disconnect(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    MQTTClient client = (MQTTClient)*(longlong*)args->args[0];

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_NOTICE, "mqtt_disconnect()");
#endif
    strcpy(last_func, "MQTTClient_disconnect");
    int rc = last_rc = MQTTClient_disconnect(client, DEFAULT_TIMEOUT);
    if (rc == MQTTCLIENT_SUCCESS) {
        MQTTClient_destroy(&client);
    }
    else {
        *error = 1;
    }
#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_disconnect() rc=%d", rc);
    closelog ();
#endif
    return rc;
}


/**
 * mqtt_publish
 *
 * Publish a mqtt payload and returns its status.
 *
 * Possible calls
 * mqtt_publish(server, [username], [password], topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})
 * mqtt_publish(client, topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})
 */
bool mqtt_publish_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif
#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_publish_init()");
#endif
    initid->ptr = malloc(sizeof(connection));
    if (initid->ptr == NULL) {
        parmerror("mqtt_publish()", args);
        strcpy(message, "memory allocation error");
#ifdef DEBUG
        closelog ();
#endif
        return 1;
    }
    connection *conn = (connection *)initid->ptr;
    conn->client = NULL;
    strcpy(last_func, "mqtt_publish_init");
    conn->rc = last_rc = MQTTCLIENT_DISCONNECTED;

    conn->mqtt_publish_format = 0;
    //~ 1: mqtt_publish(server, [username], [password], topic, [payload])
    if ( args->arg_count==5
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // payload
         && args->arg_type[4]==STRING_RESULT
        ) {
        conn->mqtt_publish_format = 1;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 2: mqtt_publish(server, [username], [password], topic, [payload], [qos])
    else if ( args->arg_count==6
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // payload
         && args->arg_type[4]==STRING_RESULT
        // qos
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0 && (int)*((longlong*)args->args[5])<=2)))
        ) {
        conn->mqtt_publish_format = 2;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 3: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained])
    else if ( args->arg_count==7
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // payload
         && args->arg_type[4]==STRING_RESULT
        // qos
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0 && (int)*((longlong*)args->args[5])<=2)))
        // retained
         && ((args->args[6]==NULL) || (args->arg_type[6]==INT_RESULT && ((int)*((longlong*)args->args[6])>=0 && (int)*((longlong*)args->args[6])<=1)))
        ) {
        conn->mqtt_publish_format = 3;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 4: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained], [timeout])
    else if ( args->arg_count==8
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // payload
         && args->arg_type[4]==STRING_RESULT
        // qos
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0 && (int)*((longlong*)args->args[5])<=2)))
        // retained
         && ((args->args[6]==NULL) || (args->arg_type[6]==INT_RESULT && ((int)*((longlong*)args->args[6])>=0 && (int)*((longlong*)args->args[6])<=1)))
        // timeout
         && ((args->args[7]==NULL) || (args->arg_type[7]==INT_RESULT && ((int)*((longlong*)args->args[7])>=0)))
        ) {
        conn->mqtt_publish_format = 4;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 5: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained], [timeout], [options])
    else if ( args->arg_count==9
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // payload
         && args->arg_type[4]==STRING_RESULT
        // qos
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0 && (int)*((longlong*)args->args[5])<=2)))
        // retained
         && ((args->args[6]==NULL) || (args->arg_type[6]==INT_RESULT && ((int)*((longlong*)args->args[6])>=0 && (int)*((longlong*)args->args[6])<=1)))
        // timeout
         && ((args->args[7]==NULL) || (args->arg_type[7]==INT_RESULT && ((int)*((longlong*)args->args[7])>=0)))
         // options
         && args->arg_type[8]==STRING_RESULT
        ) {
        conn->mqtt_publish_format = 5;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 6: mqtt_publish(client, topic, [payload])
    else if ( args->arg_count==3
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // payload
         && args->arg_type[2]==STRING_RESULT
        ) {
        conn->mqtt_publish_format = 6;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 7: mqtt_publish(client, topic, [payload], [qos])
    else if ( args->arg_count==4
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // payload
         && args->arg_type[2]==STRING_RESULT
        // qos
         && ((args->args[3]==NULL) || (args->arg_type[3]==INT_RESULT && ((int)*((longlong*)args->args[3])>=0 && (int)*((longlong*)args->args[3])<=2)))
        ) {
        conn->mqtt_publish_format = 7;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 8: mqtt_publish(client, topic, [payload], [qos], [retained])
    else if ( args->arg_count==5
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // payload
         && args->arg_type[2]==STRING_RESULT
        // qos
         && ((args->args[3]==NULL) || (args->arg_type[3]==INT_RESULT && ((int)*((longlong*)args->args[3])>=0 && (int)*((longlong*)args->args[3])<=2)))
        // retained
         && ((args->args[4]==NULL) || (args->arg_type[4]==INT_RESULT && ((int)*((longlong*)args->args[4])>=0 && (int)*((longlong*)args->args[4])<=1)))
        ) {
        conn->mqtt_publish_format = 8;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 9: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout])
    else if ( args->arg_count==6
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // payload
         && args->arg_type[2]==STRING_RESULT
        // qos
         && ((args->args[3]==NULL) || (args->arg_type[3]==INT_RESULT && ((int)*((longlong*)args->args[3])>=0 && (int)*((longlong*)args->args[3])<=2)))
        // retained
         && ((args->args[4]==NULL) || (args->arg_type[4]==INT_RESULT && ((int)*((longlong*)args->args[4])>=0 && (int)*((longlong*)args->args[4])<=1)))
        // timeout
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0)))
        ) {
        conn->mqtt_publish_format = 9;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    //~ 10: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout], [options])
    else if ( args->arg_count==7
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // payload
         && args->arg_type[2]==STRING_RESULT
        // qos
         && ((args->args[3]==NULL) || (args->arg_type[3]==INT_RESULT && ((int)*((longlong*)args->args[3])>=0 && (int)*((longlong*)args->args[3])<=2)))
        // retained
         && ((args->args[4]==NULL) || (args->arg_type[4]==INT_RESULT && ((int)*((longlong*)args->args[4])>=0 && (int)*((longlong*)args->args[4])<=1)))
        // timeout
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0)))
        // options
         && (args->arg_type[6]==STRING_RESULT)
        ) {
        conn->mqtt_publish_format = 10;
#ifdef DEBUG
        closelog ();
#endif
        return 0;
    }
    parmerror("mqtt_publish()", args);
    strcpy(message, "function argument(s) error");
#ifdef DEBUG
    closelog ();
#endif
    return 1;
}
void mqtt_publish_deinit(UDF_INIT *initid)
{
#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif
#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_publish_deinit");
#endif
    if (initid->ptr != NULL) {
        free(initid->ptr);
    }
#ifdef DEBUG
    closelog ();
#endif
}
ulonglong mqtt_publish(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    connection *conn = (connection *)initid->ptr;
    char *address, *username, *password, *topic, *payload, *options = "";
    int qos, retained, timeout, payloadlength = 0;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif

#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_publish(): mqtt_publish_format %d", conn->mqtt_publish_format);
#endif

    *is_null = 0;
    *error = 0;

    qos         = DEFAULT_QOS;
    retained    = DEFAULT_RETAINED;
    timeout     = DEFAULT_TIMEOUT;

    switch (conn->mqtt_publish_format) {
        //~ 10: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout], [options])
        case 10:
            options     = args->args[6]!=NULL ? (char *)args->args[6] : "";
        //~ 9: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout])
        case 9:
            timeout     = args->args[5]!=NULL ? (int)*((longlong*)args->args[5]) : DEFAULT_TIMEOUT;
        //~ 8: mqtt_publish(client, topic, [payload], [qos], [retained])
        case 8:
            retained    = args->args[4]!=NULL ? (int)*((longlong*)args->args[4]) : DEFAULT_RETAINED;
        //~ 7: mqtt_publish(client, topic, [payload], [qos])
        case 7:
            qos         = args->args[3]!=NULL ? (int)*((longlong*)args->args[3]) : DEFAULT_QOS;
        //~ 6: mqtt_publish(client, topic, [payload])
        case 6:
            payload     = args->args[2]!=NULL ? (char *)args->args[2] : "";
            payloadlength=args->args[2]!=NULL ? args->lengths[2] : 0;
            topic       = (char *)args->args[1];
            conn->client= (MQTTClient)*(longlong*)args->args[0];
            break;
        //~ 5: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained], [timeout], [options])
        case 5:
            options     = args->args[8]!=NULL ? (char *)args->args[8] : "";
        //~ 4: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained], [timeout])
        case 4:
            timeout     = args->args[7]!=NULL ? (int)*((longlong*)args->args[7]) : DEFAULT_TIMEOUT;
        //~ 3: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained])
        case 3:
            retained    = args->args[6]!=NULL ? (int)*((longlong*)args->args[6]) : DEFAULT_RETAINED;
        //~ 2: mqtt_publish(server, [username], [password], topic, [payload], [qos])
        case 2:
            qos         = args->args[5]!=NULL ? (int)*((longlong*)args->args[5]) : DEFAULT_QOS;
        //~ 1: mqtt_publish(server, [username], [password], topic, [payload])
        case 1:
            payload     = args->args[4]!=NULL ? (char *)args->args[4] : "";
            payloadlength=args->args[4]!=NULL ? args->lengths[4] : 0;
            topic       = (char *)args->args[3];
            password    = (char *)args->args[2];
            username    = (char *)args->args[1];
            address      = (char *)args->args[0];
            break;
        default:
            break;
    }
#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_publish(): preset done");
#endif

    // Do not assume that the string is null-terminated
    // see https://dev.mysql.com/doc/refman/5.7/en/udf-arguments.html
    switch (conn->mqtt_publish_format) {
        case 10:
            if (args->args[6]!=NULL) options[args->lengths[6]]  = '\0';
        case 9:
        case 8:
        case 7:
        case 6:
            if (args->args[1]!=NULL) topic[args->lengths[1]]    = '\0';
            if (args->args[2]!=NULL) payload[args->lengths[2]]  = '\0';
            strcpy(last_func, "mqtt_publish");
            conn->rc = last_rc = (conn->client!=NULL) ? MQTTCLIENT_SUCCESS : MQTTCLIENT_DISCONNECTED;
            break;
        case 5:
            if (args->args[8]!=NULL) options[args->lengths[8]]  = '\0';
        case 4:
        case 3:
        case 2:
        case 1:
            if (args->args[0]!=NULL) address[args->lengths[0]]  = '\0';
            if (args->args[1]!=NULL) username[args->lengths[1]] = '\0';
            if (args->args[2]!=NULL) password[args->lengths[2]] = '\0';
            if (args->args[3]!=NULL) topic[args->lengths[3]]    = '\0';
            if (args->args[4]!=NULL) payload[args->lengths[4]]  = '\0';

            strcpy(last_func, "MQTTClient_create");
            conn->rc = last_rc = MQTTClient_create(&conn->client, address, GetUUID(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
            if (conn->rc == MQTTCLIENT_SUCCESS) {
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_publish(): username=\"%s\", password=\"%s\", options='%s'", username, password, options);
#endif
                create_conn(conn, username, password, options);

                strcpy(last_func, "MQTTClient_connect");
                conn->rc = last_rc = MQTTClient_connect(conn->client, &conn->conn_opts);
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_publish 'client %p, rc=%d", conn->client, conn->rc);
#endif
            }
            break;
    }

#ifdef DEBUG
        syslog (LOG_NOTICE, "mqtt_publish(): rc=%d", conn->rc);
#endif
    if (conn->rc == MQTTCLIENT_SUCCESS) {
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;
#ifdef DEBUG
        syslog (LOG_NOTICE, "mqtt_publish() topic=\"%s\", payload=%s", topic, payload);
#endif
        pubmsg.payload = payload;
        pubmsg.payloadlen = payloadlength;
        pubmsg.qos = qos;
        pubmsg.retained = retained;
        strcpy(last_func, "MQTTClient_publishMessage");
        conn->rc = last_rc = MQTTClient_publishMessage(conn->client, topic, &pubmsg, &token);
        if (conn->rc == MQTTCLIENT_SUCCESS) {
            strcpy(last_func, "MQTTClient_waitForCompletion");
            conn->rc = last_rc = MQTTClient_waitForCompletion(conn->client, token, timeout);
        }
    }
    else {
        *error = 1;
    }

    if (conn->rc == MQTTCLIENT_SUCCESS) {
        switch (conn->mqtt_publish_format) {
            case 1:
            case 2:
            case 3:
            case 4:
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_publish() disconnnect - client=%p, rc=%d", conn->client, conn->rc);
#endif
                MQTTClient_disconnect(conn->client, timeout);
                MQTTClient_destroy(&conn->client);
                break;
        }
    }
    else {
        *error = 1;
    }

#ifdef DEBUG
    closelog ();
#endif
    return conn->rc;
}




/**
 * mqtt_subscribe
 *
 * Subsribe to a mqtt topic and returns the payload if any.
 * Possible calls
 * mqtt_subscribe(server, [username], [password], topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})
 * mqtt_subscribe(client, topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})
 *
 * [qos] currently unused
 */
bool mqtt_subscribe_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    initid->ptr = malloc(sizeof(connection));
    if (initid->ptr == NULL) {
        parmerror("mqtt_subscribe()", args);
        strcpy(message, "memory allocation error");
        return 1;
    }
    connection *conn = (connection *)initid->ptr;
    conn->client = NULL;
    strcpy(last_func, "mqtt_subscribe_init");
    conn->rc = last_rc = MQTTCLIENT_DISCONNECTED;

    conn->mqtt_publish_format = 0;
    //~ 1: mqtt_subscribe(server, [username], [password], topic)
    if ( args->arg_count==4
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        ) {
        conn->mqtt_subscribe_format = 1;
        return 0;
    }
    //~ 2: mqtt_subscribe(server, [username], [password], topic, [qos])
    else if ( args->arg_count==5
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // qos
         && ((args->args[4]==NULL) || (args->arg_type[4]==INT_RESULT && ((int)*((longlong*)args->args[4])>=0 && (int)*((longlong*)args->args[4])<=2)))
        ) {
        conn->mqtt_subscribe_format = 2;
        return 0;
    }
    //~ 3: mqtt_subscribe(server, [username], [password], topic, [qos], [timeout])
    else if ( args->arg_count==6
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // qos
         && ((args->args[4]==NULL) || (args->arg_type[4]==INT_RESULT && ((int)*((longlong*)args->args[4])>=0 && (int)*((longlong*)args->args[4])<=2)))
        // timeout
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0)))
        ) {
        conn->mqtt_subscribe_format = 3;
        return 0;
    }
    //~ 4: mqtt_subscribe(server, [username], [password], topic, [qos], [timeout], [options])
    else if ( args->arg_count==7
        // server
         && args->arg_type[0]==STRING_RESULT
         && args->args[0]!=NULL
        // username
         && args->arg_type[1]==STRING_RESULT
        // password
         && args->arg_type[2]==STRING_RESULT
        // topic
         && args->arg_type[3]==STRING_RESULT
         && args->args[3]!=NULL
        // qos
         && ((args->args[4]==NULL) || (args->arg_type[4]==INT_RESULT && ((int)*((longlong*)args->args[4])>=0 && (int)*((longlong*)args->args[4])<=2)))
        // timeout
         && ((args->args[5]==NULL) || (args->arg_type[5]==INT_RESULT && ((int)*((longlong*)args->args[5])>=0)))
        // options
         && args->arg_type[6]==STRING_RESULT
        ) {
        conn->mqtt_subscribe_format = 4;
        return 0;
    }
    //~ 5: mqtt_subscribe(client, topic)
    else if ( args->arg_count==2
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        ) {
        conn->mqtt_subscribe_format = 5;
        return 0;
    }
    //~ 6: mqtt_subscribe(client, topic, [qos])
    else if ( args->arg_count==3
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // qos
         && ((args->args[2]==NULL) || (args->arg_type[2]==INT_RESULT && ((int)*((longlong*)args->args[2])>=0 && (int)*((longlong*)args->args[2])<=2)))
        ) {
        conn->mqtt_subscribe_format = 6;
        return 0;
    }
    //~ 7: mqtt_subscribe(client, topic, [qos], [timeout])
    else if ( args->arg_count==4
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // qos
         && ((args->args[2]==NULL) || (args->arg_type[2]==INT_RESULT && ((int)*((longlong*)args->args[2])>=0 && (int)*((longlong*)args->args[2])<=2)))
        // timeout
         && ((args->args[3]==NULL) || (args->arg_type[3]==INT_RESULT && ((int)*((longlong*)args->args[3])>=0)))
        ) {
        conn->mqtt_subscribe_format = 7;
        return 0;
    }
    //~ 8: mqtt_subscribe(client, topic, [qos], [timeout], [options])
    else if ( args->arg_count==5
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // qos
         && ((args->args[2]==NULL) || (args->arg_type[2]==INT_RESULT && ((int)*((longlong*)args->args[2])>=0 && (int)*((longlong*)args->args[2])<=2)))
        // timeout
         && ((args->args[3]==NULL) || (args->arg_type[3]==INT_RESULT && ((int)*((longlong*)args->args[3])>=0)))
        // topic
         && args->arg_type[4]==STRING_RESULT
        ) {
        conn->mqtt_subscribe_format = 8;
        return 0;
    }
    else {
        parmerror("mqtt_subscribe()", args);
        strcpy(message, "function argument(s) error");
        return 1;
    }
}
void mqtt_subscribe_deinit(UDF_INIT *initid)
{
    if (initid->ptr != NULL) {
        free(initid->ptr);
    }
}
char* mqtt_subscribe(UDF_INIT *initid, UDF_ARGS *args, char* result, unsigned long* length, char *is_null, char *error)
{
    connection *conn = (connection *)initid->ptr;
    char *address, *username, *password, *topic, *options = "";
    int timeout, qos, topiclengths = 0;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif

    *is_null = 0;
    *error = 0;

    qos         = DEFAULT_QOS;
    timeout     = DEFAULT_TIMEOUT;

    switch (conn->mqtt_subscribe_format) {
        //~ 8: mqtt_subscribe(client, topic, [qos], [timeout], [options])
        case 8:
            options     = args->args[4]!=NULL ? (char *)args->args[4] : "";
        //~ 7: mqtt_subscribe(client, topic, [qos], [timeout])
        case 7:
            timeout     = args->args[3]!=NULL ? (int)*((longlong*)args->args[3]) : DEFAULT_TIMEOUT;
        //~ 6: mqtt_subscribe(client, topic, [qos])
        case 6:
            qos         = args->args[2]!=NULL ? (int)*((longlong*)args->args[2]) : DEFAULT_QOS;
        //~ 5: mqtt_subscribe(client, topic)
        case 5:
            topic       = (char *)args->args[1];
            topiclengths= args->args[1]!=NULL ? args->lengths[1] : 0;
            conn->client= (MQTTClient)*(longlong*)args->args[0];
            break;
        //~ 4: mqtt_subscribe(server, [username], [password], topic, [qos], [timeout], [options])
        case 4:
            options     = args->args[6]!=NULL ? (char *)args->args[6] : "";
        //~ 3: mqtt_subscribe(server, [username], [password], topic, [qos], [timeout])
        case 3:
            timeout     = args->args[5]!=NULL ? (int)*((longlong*)args->args[5]) : DEFAULT_TIMEOUT;
        //~ 2: mqtt_subscribe(server, [username], [password], topic [qos])
        case 2:
            qos         = args->args[4]!=NULL ? (int)*((longlong*)args->args[4]) : DEFAULT_QOS;
        //~ 1: mqtt_subscribe(server, [username], [password], topic)
        case 1:
            topic       = (char *)args->args[3];
            topiclengths= args->args[3]!=NULL ? args->lengths[3] : 0;
            password    = (char *)args->args[2];
            username    = (char *)args->args[1];
            address      = (char *)args->args[0];
            break;
        default:
            break;
    }

    // Do not assume that the string is null-terminated
    // see https://dev.mysql.com/doc/refman/5.7/en/udf-arguments.html
    switch (conn->mqtt_subscribe_format) {
        case 8:
            if (args->args[4]!=NULL) options[args->lengths[4]]  = '\0';
        case 7:
        case 6:
        case 5:
            if (args->args[1]!=NULL) topic[args->lengths[1]]    = '\0';
            strcpy(last_func, "mqtt_subscribe");
            conn->rc = last_rc = (conn->client!=NULL) ? MQTTCLIENT_SUCCESS : MQTTCLIENT_DISCONNECTED;
            break;
        case 4:
            if (args->args[6]!=NULL) options[args->lengths[6]]  = '\0';
        case 3:
        case 2:
        case 1:
            if (args->args[0]!=NULL) address[args->lengths[0]]  = '\0';
            if (args->args[1]!=NULL) username[args->lengths[1]] = '\0';
            if (args->args[2]!=NULL) password[args->lengths[2]] = '\0';
            if (args->args[3]!=NULL) topic[args->lengths[3]]    = '\0';

            strcpy(last_func, "MQTTClient_create");
            conn->rc = last_rc = MQTTClient_create(&conn->client, address, GetUUID(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
            if (conn->rc == MQTTCLIENT_SUCCESS) {
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_subscribe(): username=\"%s\", password=\"%s\", options='%s'", username, password, options);
#endif
                create_conn(conn, username, password, options);

                strcpy(last_func, "MQTTClient_connect");
                conn->rc = last_rc = MQTTClient_connect(conn->client, &conn->conn_opts);
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_subscribe(): client=%p, rc=%d", conn->client, conn->rc);
#endif
            }
            break;
    }

    if (conn->rc == MQTTCLIENT_SUCCESS) {
        MQTTClient_message *submsg = NULL;
        int rc;

#ifdef DEBUG
        syslog (LOG_NOTICE, "mqtt_subscribe '%s'", topic);
#endif
        strcpy(last_func, "MQTTClient_subscribe");
        rc = last_rc = MQTTClient_subscribe(conn->client, topic, qos);
        if (rc == MQTTCLIENT_SUCCESS) {
#ifdef DEBUG
            syslog (LOG_NOTICE, "mqtt_subscribe MQTTClient_receive() returned %sdata, rc=%d", submsg != NULL ? "":"no ", rc);
#endif
            strcpy(last_func, "MQTTClient_receive");
            rc = last_rc = MQTTClient_receive(conn->client, &topic, &topiclengths, &submsg, timeout);
#ifdef DEBUG
            syslog (LOG_NOTICE, "mqtt_subscribe MQTTClient_receive() returned %sdata, rc=%d", submsg != NULL ? "":"no ", rc);
#endif
            if ((rc == MQTTCLIENT_SUCCESS) && (submsg != NULL)) {
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_subscribe payload returned bytes %d", submsg->payloadlen);
#endif
                memcpy(result, submsg->payload, submsg->payloadlen);
                *length = submsg->payloadlen;
                MQTTClient_freeMessage(&submsg);
            }
            else {
                *result = '\0';
                *is_null = 1;
                *error = 1;

            }
        }
        else {
            *result = '\0';
            *is_null = 1;
            *error = 1;
        }
    }

    if (conn->rc == MQTTCLIENT_SUCCESS) {
        switch (conn->mqtt_subscribe_format) {
            case 1:
            case 2:
            case 3:
    #ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_subscribe() disconnnect - client=%p, rc=%d", conn->client, conn->rc);
    #endif
                MQTTClient_disconnect(conn->client, timeout);
                MQTTClient_destroy(&conn->client);
                break;
        }
    }
    else {
        *result = '\0';
        *error = 1;
    }

#ifdef DEBUG
    closelog ();
#endif

    return result;
}
