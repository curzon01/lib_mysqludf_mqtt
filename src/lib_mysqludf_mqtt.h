#define LIBVERSION                  "1.0.0"
#define LIBNAME                     "lib_mysqludf_mqtt"
#define LIBBUILD                    __DATE__ " " __TIME__

#define DEFAULT_QOS                 0       // default MQTT QOS
#define DEFAULT_RETAINED            0       // default MQTT retain
#define DEFAULT_TIMEOUT             5000L   // default MQTT timeout
#define DEFAULT_KEEPALIVEINTERVAL   20      // default MQTT keepalive

#define UUID_LEN                    8       // number of hex chars for MQTT unique client id
#define MAX_RET_STRLEN              512     // max string length returned by functions using strings

//#define DEBUG                       // debug output via syslog

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#define DLLEXP __declspec(dllexport)
#else
#define DLLEXP
#endif


#ifdef __WIN__
typedef unsigned __int64 ulonglong;
typedef __int64 longlong;
#else
typedef unsigned long long ulonglong;
typedef long long longlong;
#endif  // __WIN__


/* MQTT connection information for MySQL UDF */
typedef struct CONNECTION {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts;
    int mqtt_publish_format;
    int mqtt_subscribe_format;
    int rc;
} connection;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * mqtt_info
 *
 * Returns udf library info as JSON string
 * mqtt_info()
 *
 */
DLLEXP bool mqtt_info_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
DLLEXP void mqtt_info_deinit(UDF_INIT *initid);
DLLEXP char* mqtt_info(UDF_INIT *initid, UDF_ARGS *args, char* result, unsigned long* length, char *is_null, char *error);

/**
 * mqtt_lasterror
 *
 * Returns last error as JSON string
 * mqtt_lasterror()
 *
 */
DLLEXP bool mqtt_lasterror_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
DLLEXP void mqtt_lasterror_deinit(UDF_INIT *initid);
DLLEXP char* mqtt_lasterror(UDF_INIT *initid, UDF_ARGS *args, char* result, unsigned long* length, char *is_null, char *error);

/**
 * mqtt_connect
 *
 * Connect to a mqtt server and returns a handle.
 * mqtt_connect(server {, username} {, password}})
 *
 * Parameter in [] means it can be NULL - in this case a default value will be use:
 *        server    String
 *                  Specifying the server to which the client will
 *                  connect. It takes the form protocol://host:port.
 *                  Currently, protocol must be tcp or ssl.
 *                  For host, you can specify either an IP address or a
 *                  host name.
 *                  For instance, to connect to a server running on the
 *                  local machines with the default MQTT port, specify
 *                  "tcp://localhost:1883".
 *        username  String - default ''
 *                  Username for authentification or NULL if unused
 *        password  String - default ''
 *                  Password for authentification or NULL if unused
 *
 *  returns valid handle or 0 on errors
 */
DLLEXP bool mqtt_connect_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
DLLEXP void mqtt_connect_deinit(UDF_INIT *initid);
DLLEXP ulonglong mqtt_connect(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);


/**
 * mqtt_disconnect
 *
 * Disconnect from a mqtt server.
 * mqtt_disconnect(handle, timeout)
 *  returns 0 if successful
 */
DLLEXP bool mqtt_disconnect_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
DLLEXP void mqtt_disconnect_deinit(UDF_INIT *initid);
DLLEXP ulonglong mqtt_disconnect(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);


/**
 * mqtt_publish
 *
 * Publish a mqtt payload and returns its status.
 * Possible calls
 * 1: mqtt_publish(server, [username], [password], topic, [payload])
 * 2: mqtt_publish(server, [username], [password], topic, [payload], [qos])
 * 3: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained])
 * 4: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained], [timeout])
 * 5: mqtt_publish(client, topic, [payload])
 * 6: mqtt_publish(client, topic, [payload], [qos])
 * 7: mqtt_publish(client, topic, [payload], [qos], [retained])
 * 8: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout])
 *
 * Parameter in [] means it can be NULL - in this case a default value
 * will be use:
 *        server    String
 *                  Specifying the server to which the client will
 *                  connect. It takes the form protocol://host:port.
 *                  Currently, protocol must be tcp or ssl.
 *                  For host, you can specify either an IP address or a
 *                  host name.
 *                  For instance, to connect to a server running on the
 *                  local machines with the default MQTT port, specify
 *                  "tcp://localhost:1883".
  *        username  String - default ''
 *                  Username for authentification or NULL if unused
 *        password  String - default ''
 *                  Password for authentification or NULL if unused
 *        client    Handle
 *                  A valid handle returned from mqtt_connect() call.
 *        topic     String
 *                  The topic to be published
 *        payload   String - default ''
 *                  The message published for the topic
 *        qos       Integer [0-2] - default 0
 *                  The QOS (Quality Of Service) number
 *        retained  Integer [0,1] - default 0
 *                  Flag if message should be retained (1) or not (0)
 *        timeout   Integer (ms]  - default 5000
 *                  Timeout value for connecting to MQTT server (in ms)
 *
 * returns 0 for success, otherwise error code from MQTTClient_connect() call
 * (see http://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/_m_q_t_t_client_8h.html)
 *
 * If this function is called with the parameter
 *     server, username and password
 * the function will connect to MQTT, publish the payload and disconnnect
 * after publish.
 * Because this may slow down when a lot of publishing will be done you
 * can do publish using an alternate way with a client handle:
 * 1. Call mqtt_connect() to get a valid mqtt connection handle
 * 2. Call mqtt_publish() using 'client' parameter.
 * 3. Repeat step 2. as long as possible
 * 4. Call mqtt_disconnect() to free the handle
 *
 */
DLLEXP bool mqtt_publish_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
DLLEXP void mqtt_publish_deinit(UDF_INIT *initid);
DLLEXP ulonglong mqtt_publish(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);

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
 *
 * Parameter in [] means it can be NULL - in this case a default value
 * will be use:
 *        server    String
 *                  Specifying the server to which the client will
 *                  connect. It takes the form protocol://host:port.
 *                  Currently, protocol must be tcp or ssl.
 *                  For host, you can specify either an IP address or a
 *                  host name.
 *                  For instance, to connect to a server running on the
 *                  local machines with the default MQTT port, specify
 *                  "tcp://localhost:1883".
 *        username  String - default ''
 *                  Username for authentification or NULL if unused
 *        password  String - default ''
 *                  Password for authentification or NULL if unused
 *        client    Handle
 *                  A valid handle returned from mqtt_connect() call.
 *        topic     String
 *                  The topic to be subscribe
 *        qos       Integer [0-2] - default 0
 *                  The QOS (Quality Of Service) number
 *        timeout   Integer (ms]  - default 5000
 *                  Timeout value for connecting to MQTT server (in ms)
 *
 * returns the payload as string for success, otherwise it returns NULL
 * (see http://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/_m_q_t_t_client_8h.html)
 *
 * If this function is called with the parameter
 *     server, username and password
 * the function will connect to MQTT, publish the payload and disconnnect
 * after publish.
 * Because this may slow down when a lot of publishing will be done you
 * can do publish using an alternate way with a client handle:
 * 1. Call mqtt_connect() to get a valid mqtt connection handle
 * 2. Call mqtt_subscribe() using 'client' parameter.
 * 3. Repeat step 2. as long as possible
 * 4. Call mqtt_disconnect() to free the handle
 *
 */
DLLEXP bool mqtt_subscribe_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
DLLEXP void mqtt_subscribe_deinit(UDF_INIT *initid);
DLLEXP char* mqtt_subscribe(UDF_INIT *initid, UDF_ARGS *args, char* result, unsigned long* length, char *is_null, char *error);

#ifdef __cplusplus
}
#endif  // __cplusplus
