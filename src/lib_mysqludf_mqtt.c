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
#include "lib_mysqludf_mqtt.h"

#ifdef DEBUG
#include <syslog.h>
#endif  // DEBUG


volatile int last_rc = 0;
char last_func[128] = {0};

/* Helper */
const char *GetUUID(void)
{
    static char struuid[sizeof(LIBNAME)+sizeof(LIBVERSION)+UUID_LEN + 1];
    char uuid[UUID_LEN + 1];

    for (int i = 0; i < UUID_LEN; i += 2)
    {
        sprintf(&uuid[i], "%02x", rand() % 16);
    }
    sprintf(struuid, "%s-%s_%s", LIBNAME, LIBVERSION, uuid);
    return struuid;
}

void parmerror(const char *context, UDF_ARGS *args)
{
    char *type;

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

    *is_null = 0;
    *error = 0;

    if (res == NULL) {
        strcpy(result, "memory allocation error");
        *length = strlen(result);
        *error = 1;
        return result;
    }

    mqttClientVersion = MQTTClient_getVersionInfo();
    while (mqttClientVersion != NULL && mqttClientVersion->name != NULL && mqttClientVersion->value!=NULL) {
        sprintf(libinfo+strlen(libinfo),"\"%s\":\"%s\"", mqttClientVersion->name, mqttClientVersion->value);
        mqttClientVersion++;
        if (mqttClientVersion->name != NULL && mqttClientVersion->value!=NULL) {
            strcat(libinfo, ",");
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(res, MAX_RET_STRLEN, "{\"Name\": \"%s\", \"Version\": \"%s\", \"Build\": \"%s\", \"Library\": {%s}}", LIBNAME, LIBVERSION, LIBBUILD, libinfo);
#pragma GCC diagnostic pop

    *length = strlen(res);
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
    return res;
}


/**
 * mqtt_connect
 *
 * Connect to a mqtt server and returns a handle.
 * mqtt_connect(server, [username], [password])
 *      returns connect handle or 0 on erro,ar
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

    char *address   = "";
    char *username  = "";
    char *password  = "";

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

#ifdef DEBUG
    syslog (LOG_NOTICE, "mqtt_connect 'conn - username %s, password %s", username, password);
#endif
    memcpy(&conn->conn_opts, &(MQTTClient_connectOptions)MQTTClient_connectOptions_initializer, sizeof(MQTTClient_connectOptions));
    conn->conn_opts.keepAliveInterval = DEFAULT_KEEPALIVEINTERVAL;
    conn->conn_opts.cleansession = 1;
    conn->conn_opts.username = username;
    conn->conn_opts.password = password;
    conn->conn_opts.MQTTVersion = MQTTVERSION_DEFAULT;

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

    strcpy(last_func, "MQTTClient_disconnect");
    int rc = last_rc = MQTTClient_disconnect(client, DEFAULT_TIMEOUT);
    if (rc == MQTTCLIENT_SUCCESS) {
        MQTTClient_destroy(&client);
    }
    else {
        *error = 1;
    }
    return rc;
}


/**
 * mqtt_publish
 *
 * Publish a mqtt payload and returns its status.
 *
 * Possible calls
 * 1: mqtt_publish(server, [username], [password], topic, [payload])
 * 2: mqtt_publish(server, [username], [password], topic, [payload], [qos])
 * 3: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained])
 * 4: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained], [timeout])
 * 5: mqtt_publish(client, topic, [payload])
 * 6: mqtt_publish(client, topic, [payload], [qos])
 * 7: mqtt_publish(client, topic, [payload], [qos], [retained])
 * 8: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout])
 */
bool mqtt_publish_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    initid->ptr = malloc(sizeof(connection));
    if (initid->ptr == NULL) {
        parmerror("mqtt_publish()", args);
        strcpy(message, "memory allocation error");
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
        return 0;
    }
    //~ 5: mqtt_publish(client, topic, [payload])
    else if ( args->arg_count==3
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // payload
         && args->arg_type[2]==STRING_RESULT
        ) {
        conn->mqtt_publish_format = 5;
        return 0;
    }
    //~ 6: mqtt_publish(client, topic, [payload], [qos])
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
        conn->mqtt_publish_format = 6;
        return 0;
    }
    //~ 7: mqtt_publish(client, topic, [payload], [qos], [retained])
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
        conn->mqtt_publish_format = 7;
        return 0;
    }
    //~ 8: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout])
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
        conn->mqtt_publish_format = 8;
        return 0;
    }
    else {
        parmerror("mqtt_publish()", args);
        strcpy(message, "function argument(s) error");
        return 1;
    }



}
void mqtt_publish_deinit(UDF_INIT *initid)
{
    if (initid->ptr != NULL) {
        free(initid->ptr);
    }
}
ulonglong mqtt_publish(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    connection *conn = (connection *)initid->ptr;
    char *address, *username, *password, *topic, *payload;
    int qos,retained, timeout, payloadlength;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif

    *is_null = 0;
    *error = 0;

    qos         = DEFAULT_QOS;
    retained    = DEFAULT_RETAINED;
    timeout     = DEFAULT_TIMEOUT;

    switch (conn->mqtt_publish_format) {
        //~ 8: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout])
        case 8:
            timeout     = args->args[5]!=NULL ? (int)*((longlong*)args->args[5]) : DEFAULT_TIMEOUT;
        //~ 7: mqtt_publish(client, topic, [payload], [qos], [retained])
        case 7:
            retained    = args->args[4]!=NULL ? (int)*((longlong*)args->args[4]) : DEFAULT_RETAINED;
        //~ 6: mqtt_publish(client, topic, [payload], [qos])
        case 6:
            qos         = args->args[3]!=NULL ? (int)*((longlong*)args->args[3]) : DEFAULT_QOS;
        //~ 5: mqtt_publish(client, topic, [payload])
        case 5:
            payload     = args->args[2]!=NULL ? (char *)args->args[2] : "";
            payloadlength=args->args[2]!=NULL ? args->lengths[2] : 0;
            topic       = (char *)args->args[1];
            conn->client= (MQTTClient)*(longlong*)args->args[0];
            break;
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

    // Do not assume that the string is null-terminated
    // see https://dev.mysql.com/doc/refman/5.7/en/udf-arguments.html
    switch (conn->mqtt_publish_format) {
        case 1:
        case 2:
        case 3:
        case 4:
            if (args->args[0]!=NULL) address[args->lengths[0]]  = '\0';
            if (args->args[1]!=NULL) username[args->lengths[1]] = '\0';
            if (args->args[2]!=NULL) password[args->lengths[2]] = '\0';
            if (args->args[3]!=NULL) topic[args->lengths[3]]    = '\0';
            if (args->args[4]!=NULL) payload[args->lengths[4]]  = '\0';

            strcpy(last_func, "MQTTClient_create");
            conn->rc = last_rc = MQTTClient_create(&conn->client, address, GetUUID(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
            if (conn->rc == MQTTCLIENT_SUCCESS) {
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_publish 'conn - username %s, password %s", username, password);
#endif
                memcpy(&conn->conn_opts, &(MQTTClient_connectOptions)MQTTClient_connectOptions_initializer, sizeof(MQTTClient_connectOptions));
                conn->conn_opts.keepAliveInterval = DEFAULT_KEEPALIVEINTERVAL;
                conn->conn_opts.cleansession = 1;
                conn->conn_opts.username = username;
                conn->conn_opts.password = password;
                conn->conn_opts.MQTTVersion = MQTTVERSION_DEFAULT;
                strcpy(last_func, "MQTTClient_connect");
                conn->rc = last_rc = MQTTClient_connect(conn->client, &conn->conn_opts);
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_publish 'client %p, rc=%d", conn->client, conn->rc);
#endif
            }
            break;

        case 5:
        case 6:
        case 7:
        case 8:
            if (args->args[1]!=NULL) topic[args->lengths[1]]    = '\0';
            if (args->args[2]!=NULL) payload[args->lengths[2]]  = '\0';
            strcpy(last_func, "mqtt_publish");
            conn->rc = last_rc = (conn->client!=NULL) ? MQTTCLIENT_SUCCESS : MQTTCLIENT_DISCONNECTED;
            break;
    }

    if (conn->rc == MQTTCLIENT_SUCCESS) {
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;
#ifdef DEBUG
        syslog (LOG_NOTICE, "mqtt_publish '%s': %s", topic, payload);
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
                syslog (LOG_NOTICE, "mqtt_publish disconnnect 'client %p, rc=%d", conn->client, conn->rc);
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
 * 1: mqtt_subscribe(server, [username], [password], topic)
 * 2: mqtt_subscribe(server, [username], [password], topic [qos])
 * 3: mqtt_subscribe(server, [username], [password], topic, [qos], [timeout])
 * 4: mqtt_subscribe(client, topic)
 * 5: mqtt_subscribe(client, topic, [qos])
 * 6: mqtt_subscribe(client, topic, [qos], [timeout])
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
    //~ 4: mqtt_subscribe(client, topic)
    else if ( args->arg_count==2
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        ) {
        conn->mqtt_subscribe_format = 4;
        return 0;
    }
    //~ 5: mqtt_subscribe(client, topic, [qos])
    else if ( args->arg_count==3
        // client
         && args->arg_type[0]==INT_RESULT
        // topic
         && args->arg_type[1]==STRING_RESULT
         && args->args[1]!=NULL
        // qos
         && ((args->args[2]==NULL) || (args->arg_type[2]==INT_RESULT && ((int)*((longlong*)args->args[2])>=0 && (int)*((longlong*)args->args[2])<=2)))
        ) {
        conn->mqtt_subscribe_format = 5;
        return 0;
    }
    //~ 6: mqtt_subscribe(client, topic, [qos], [timeout])
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
        conn->mqtt_subscribe_format = 6;
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
    char *address, *username, *password, *topic;
    int topiclengths;
    // [qos] currently unused
    __attribute__((unused)) int qos;
    int timeout;

#ifdef DEBUG
    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog (LIBNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif

    *is_null = 0;
    *error = 0;

    qos         = DEFAULT_QOS;
    timeout     = DEFAULT_TIMEOUT;

    switch (conn->mqtt_subscribe_format) {
        //~ 6: mqtt_subscribe(client, topic, [qos], [timeout])
        case 6:
            timeout     = args->args[3]!=NULL ? (int)*((longlong*)args->args[3]) : DEFAULT_TIMEOUT;
        //~ 5: mqtt_subscribe(client, topic, [qos])
        case 5:
            qos         = args->args[2]!=NULL ? (int)*((longlong*)args->args[2]) : DEFAULT_QOS;
        //~ 4: mqtt_subscribe(client, topic)
        case 4:
            topic       = (char *)args->args[1];
            topiclengths= args->args[1]!=NULL ? args->lengths[1] : 0;
            conn->client= (MQTTClient)*(longlong*)args->args[0];
            break;
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
        case 1:
        case 2:
        case 3:
            if (args->args[0]!=NULL) address[args->lengths[0]]  = '\0';
            if (args->args[1]!=NULL) username[args->lengths[1]] = '\0';
            if (args->args[2]!=NULL) password[args->lengths[2]] = '\0';
            if (args->args[3]!=NULL) topic[args->lengths[3]]    = '\0';

            strcpy(last_func, "MQTTClient_create");
            conn->rc = last_rc = MQTTClient_create(&conn->client, address, GetUUID(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
            if (conn->rc == MQTTCLIENT_SUCCESS) {
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_subscribe 'conn - username %s, password %s", username, password);
#endif
                memcpy(&conn->conn_opts, &(MQTTClient_connectOptions)MQTTClient_connectOptions_initializer, sizeof(MQTTClient_connectOptions));
                conn->conn_opts.keepAliveInterval = DEFAULT_KEEPALIVEINTERVAL;
                conn->conn_opts.cleansession = 1;
                conn->conn_opts.username = username;
                conn->conn_opts.password = password;
                conn->conn_opts.MQTTVersion = MQTTVERSION_DEFAULT;
                strcpy(last_func, "MQTTClient_connect");
                conn->rc = last_rc = MQTTClient_connect(conn->client, &conn->conn_opts);
#ifdef DEBUG
                syslog (LOG_NOTICE, "mqtt_subscribe 'client %p, rc=%d", conn->client, conn->rc);
#endif
            }
            break;

        case 4:
        case 5:
        case 6:
            if (args->args[1]!=NULL) topic[args->lengths[1]]    = '\0';
            strcpy(last_func, "mqtt_subscribe");
            conn->rc = last_rc = (conn->client!=NULL) ? MQTTCLIENT_SUCCESS : MQTTCLIENT_DISCONNECTED;
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
                syslog (LOG_NOTICE, "mqtt_publish disconnnect 'client %p, rc=%d", conn->client, conn->rc);
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
