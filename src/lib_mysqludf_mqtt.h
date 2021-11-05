#define LIBVERSION                  "1.1.0"
#define LIBNAME                     "lib_mysqludf_mqtt"
#define LIBBUILD                    __DATE__ " " __TIME__

#define DEFAULT_QOS                 0       // default MQTT QOS
#define DEFAULT_RETAINED            0       // default MQTT retain
#define DEFAULT_TIMEOUT             5000L   // default MQTT timeout
#define DEFAULT_KEEPALIVEINTERVAL   20      // default MQTT keepalive

#define UUID_LEN                    8       // number of hex chars for MQTT unique client id
#define MAX_RET_STRLEN              2048    // max string length returned by functions using strings

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

// get_json_value() return codes
#define JSON_OK                  0
#define JSON_ERROR_EMPTY_STR    -1
#define JSON_ERROR_INVALID_STR  -2
#define JSON_ERROR_WRONG_TYPE   -3
#define JSON_ERROR_WRONG_VALUE  -4
#define JSON_ERROR_NOT_FOUND    -5


/* MQTT connection information for MySQL UDF */
typedef struct CONNECTION {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts;
    MQTTClient_SSLOptions ssl_opts;
    MQTTClient_willOptions will_opts;
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
 * mqtt_connect(server {,[username]} {,[password] {,[options]}}})
 *
 * Parameter in {} are optional an can be omit.
 * Parameter in [] can be NULL - in this case a default value is used.
 * To use optional parameters after omitting other optional parameters, use NULL, e.g.
 * mqtt_connect(server ,NULL, NULL, options)
 * 
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
 *        options   String - default ''
 *                  JSON string containing additonal options or NULL if unused
 *                  The following JSON objects are accept:
 *                  "CApath": String
 *                      Points to a directory containing CA certificates in PEM format
 *                  "CAfile": String
 *                      The file in PEM format containing the public digital
 *                      certificates trusted by the client.
 *                  "keyStore": String
 *                      The file in PEM format containing the public certificate
 *                      chain of the client. It may also include the client's
 *                      private key.
 *                  "privateKey": String
 *                      If not included in the sslKeyStore, this setting points
 *                      to the file in PEM format containing the client's private key.
 *                  "privateKeyPassword": String
 *                      The password to load the client's privateKey if encrypted.
 *                  "enabledCipherSuites": String
 *                      The list of cipher suites that the client will present to the
 *                      server during the SSL handshake.
 *                  "verify": bool
 *                      Whether to carry out post-connect checks, including
 *                      that a certificate matches the given host name.
 *                  "enableServerCertAuth": bool
 *                      True/False option to enable verification of the server
 *                      certificate.
 *                  "sslVersion": integer
 *                      The SSL/TLS version to use. Specify one of:
 *                      0 = MQTT_SSL_VERSION_DEFAULT
 *                      1 = MQTT_SSL_VERSION_TLS_1_0
 *                      2 = MQTT_SSL_VERSION_TLS_1_1
 *                      3 = MQTT_SSL_VERSION_TLS_1_2
 *                  "keepAliveInterval": integer
 *                      The "keep alive" interval, measured in seconds, defines
 *                      the maximum time that should pass without communication
 *                      between the client and the server.
 *                  "cleansession": bool
 *                      The cleansession setting controls the behaviour of both
 *                      the client and the server at connection and
 *                      disconnection time.
 *                  "reliable": bool
 *                      This is a boolean value that controls how many messages
 *                      can be in-flight simultaneously. Setting reliable to
 *                      true means that a published message must be completed
 *                      (acknowledgements received) before another can be sent.
 *                  "MQTTVersion": String
 *                      Sets the version of MQTT to be used on the connect:
 *                      0 = MQTTVERSION_DEFAULT start with 3.1.1, and if that
 *                          fails, fall back to 3.1
 *                      3 = MQTTVERSION_3_1
 *                      4 = MQTTVERSION_3_1_1
 *                      5 = MQTTVERSION_5
 *                  "maxInflightMessages": integer
 *                      The maximum number of messages in flight
 *                  "willTopic": String
 *                      The LWT topic to which the LWT message will be published.
 *                  "willMessage": String
 *                      The LWT payload.
 *                  "willRetained": bool
 *                      The retained flag for the LWT messag
 *                  "willQos": integer
 *                      The quality of service setting for the LWT message
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
 * mqtt_publish(server, [username], [password], topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})
 * mqtt_publish(client, topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})
 *
 * Parameter in {} are optional an can be omit.
 * Parameter in [] can be NULL - in this case a default value is used.
 * To use optional parameters after omitting other optional parameters, use NULL.
 *
 *
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
 *        options   String - default ''
 *                  JSON string containing additonal options or NULL if unused.
 *                  For details see mqtt_connect()
 *
 * returns 0 for success, otherwise error code from MQTTClient_connect() call
 * (see http://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/_m_q_t_t_client_8h.html)
 *
 * If this function is called with server as first parameter, the function will
 * connect to MQTT, publish the payload and disconnnect after publish.
 * Because this may slow down when a lot of publishing will be done you can do
 * publish using an alternate way with a client handle:
 * 1. Call mqtt_connect() to get a valid mqtt connection handle
 * 2. Call mqtt_publish() using 'client' as first parameter.
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
 * mqtt_subscribe(server, [username], [password], topic, {,[qos] {,[timeout] {,[options]}}})
 * mqtt_subscribe(client, topic, [payload] {,[qos] {,[timeout] {,[options]}}})
 * [qos] currently unused
 *
 * Parameter in {} are optional an can be omit.
 * Parameter in [] can be NULL - in this case a default value is used.
 * To use optional parameters after omitting other optional parameters, use NULL.
 *
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
 *        qos       Integer [0..2] - default 0
 *                  The QOS (Quality Of Service) number
 *        timeout   Integer (ms] - default 5000
 *                  Timeout value for connecting to MQTT server (in ms)
 *        options   String - default ''
 *                  JSON string containing additonal options or NULL if unused.
 *                  For detateils see mqtt_connect()
 *
 * returns the payload as string for success, otherwise it returns NULL
 * (see http://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/_m_q_t_t_client_8h.html)
 *
 * If this function is called with server as first parameter, the function will
 * connect to MQTT, subscribe to topic and disconnnect after subscribed.
 * Because this may slow down when a lot of subscribes will be done you can do
 * subscribing use an alternate way with a client handle:
 * 1. Call mqtt_connect() to get a valid mqtt connection handle
 * 2. Call mqtt_subscribe() using 'client' as first parameter.
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
