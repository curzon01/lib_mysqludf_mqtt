[![master](https://img.shields.io/badge/master-v1.1.0-blue.svg)](https://github.com/curzon01/lib_mysqludf_mqtt/tree/master)
[![License](https://img.shields.io/github/license/curzon01/lib_mysqludf_mqtt.svg)](LICENSE)

# MySQL loadable function for MQTT

This repository contains the source code for a [MySQL loadable function](https://dev.mysql.com/doc/extending-mysql/8.0/en/adding-loadable-function.html) library (previously called UDF - User Defined Functions), which provides some additonal SQL functions to interact with your MQTT server for publish and subscribe MQTT topics.

If you like **lib_mysqludf_mqtt** give it a star or fork it:

[![GitHub stars](https://img.shields.io/github/stars/curzon01/lib_mysqludf_mqtt.svg?style=social&label=Star)](https://github.com/curzon01/lib_mysqludf_mqtt/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/curzon01/lib_mysqludf_mqtt.svg?style=social&label=Fork)](https://github.com/curzon01/lib_mysqludf_mqtt/network)

## Build instructions for GNU Make

Ensure the [Eclipse Paho C Client Library for the MQTT Protocol](https://github.com/eclipse/paho.mqtt.c) is installed.<br>
Also install libjsonparser:

```bash
sudo apt install libjsonparser-dev
```

### Install

From the base directory run:

```bash
make
sudo make install
```

This will build and install the library file. 

To active the loadable function within your MySQL server run the follwoing SQL queries:

```SQL
CREATE FUNCTION mqtt_info RETURNS STRING SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_lasterror RETURNS STRING SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_connect RETURNS INTEGER SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_disconnect RETURNS INTEGER SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_publish RETURNS INTEGER SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_subscribe RETURNS STRING SONAME 'lib_mysqludf_mqtt.so';
```

### Uninstall

To uninstall first deactive the loadable function within your MySQL server running the SQL queries:

```SQL
DROP FUNCTION IF EXISTS mqtt_info;
DROP FUNCTION IF EXISTS mqtt_lasterror;
DROP FUNCTION IF EXISTS mqtt_connect;
DROP FUNCTION IF EXISTS mqtt_disconnect;
DROP FUNCTION IF EXISTS mqtt_publish;
DROP FUNCTION IF EXISTS mqtt_subscribe;
```

Then uninstall the library file using command line:

```bash
sudo make uninstall
```

# Usage


## MySQL loadable functions

## mqtt_connect

Connect to a mqtt server and returns a handle.

`mqtt_connect(server {,[username]} {,[password] {,[options]}}})`

Parameter in `{}` are optional an can be omit.<br>
Parameter in `[]` can be `NULL` - in this case a default value is used.<br>
To use optional parameters after omitting other optional parameters, use `NULL`.<br>

<dl>
<dt><code>server</code>    String</dt>
<dd>Specifying the server to which the client will connect. It takes the form <code>protocol://host:port</code>.<br>Currently <code>protocol</code> must be <code>tcp</code> or <code>ssl</code>.<br>
For <code>host</code> you can specify either an IP address or a hostname.<br>
For instance, to connect to a server running on the local machines with the default MQTT port, specify <code>tcp://localhost:1883</code> or <code>ssl://localhost:8883</code> for an SSL connection.</dd>
<dt><code>username</code>  String</dt>
<dd>Username for authentification. If <code>username</code> should remain unused, omit the parameter or set it to <code>NULL</code></dd>
<dt><code>password</code>  String</dt>
<dd>Password for authentification. If the <code>password</code> should remain unused, omit the parameter or set it to <code>NULL</code></dd>
<dt><code>options</code>   String</dt>
<dd>JSON string containing additonal options or NULL if unused. The following JSON objects are accept:
<dl>
<dt><code>CApath</code>: String</dt>
<dd>Points to a directory containing CA certificates in PEM format</dd>
<dt><code>CAfile</code>: String</dt>
<dd>The file in PEM format containing the public digital certificates trusted by the client.</dd>
<dt><code>keyStore</code>: String</dt>
<dd>The file in PEM format containing the public certificate chain of the client. It may also include the client's private key.</dd>
<dt><code>privateKey</code>: String</dt>
<dd>If not included in the sslKeyStore, this setting points to the file in PEM format containing the client's private key.</dd>
<dt><code>privateKeyPassword</code>: String</dt>
<dd>The password to load the client's privateKey if encrypted.</dd>
<dt><code>enabledCipherSuites</code>: String
<dd>The list of cipher suites that the client will present to the server during the SSL handshake.</dd>
<dt><code>verify</code>: boolean</dt>
<dd>Whether to carry out post-connect checks, including that a certificate matches the given host name.</dd>
<dt><code>enableServerCertAuth</code>: boolean</dt>
<dd>True/False option to enable verification of the server certificate.</dd>
<dt><code>sslVersion</code>: integer</dt>
<dd>The SSL/TLS version to use. Specify one of:<br>
    0 = MQTT_SSL_VERSION_DEFAULT<br>
    1 = MQTT_SSL_VERSION_TLS_1_0<br>
    2 = MQTT_SSL_VERSION_TLS_1_1<br>
    3 = MQTT_SSL_VERSION_TLS_1_2</dd>
<dt><code>keepAliveInterval</code>: integer</dt>
<dd>The "keep alive" interval, measured in seconds, defines the maximum time that should pass without communication between the client and the server.</dd>
<dt><code>cleansession</code>: boolean</dt>
<dd>The cleansession setting controls the behaviour of both the client and the server at connection and disconnection time.</dd>
<dt><code>reliable</code>: boolean</dt>
<dd>This is a boolean value that controls how many messages can be in-flight simultaneously. Setting reliable to true means that a published message must be completed (acknowledgements received) before another can be sent.</dd>
<dt><code>MQTTVersion</code>: String</dt>
<dd>Sets the version of MQTT to be used on the connect:<br>
    0 = MQTTVERSION_DEFAULT start with 3.1.1, and if that fails, fall back to 3.1<br>
    3 = MQTTVERSION_3_1<br>
    4 = MQTTVERSION_3_1_1<br>
    5 = MQTTVERSION_5</dd>
<dt><code>maxInflightMessages</code>: integer</dt>
<dd>The maximum number of messages in flight</dd>
<dt><code>willTopic</code>: String</dt>
<dd>The LWT topic to which the LWT message will be published.</dd>
<dt><code>willMessage</code>: String</dt>
<dd>The LWT payload.</dd>
<dt><code>willRetained</code>: boolean</dt>
<dd>The retained flag for the LWT message.</dd>
<dt><code>willQos</code>: String</dt>
<dd>The quality of service setting for the LWT message</dd>
</dl></dd>
</dl>

Returns a valid handle or 0 on error

Examples:

```sql
SET @client = (SELECT mqtt_connect('tcp://localhost:1883', NULL, NULL));
SET @client = (SELECT mqtt_connect('tcp://localhost:1883', 'myuser', 'mypasswd'));
SET @client = (SELECT mqtt_connect('ssl://mqtt.eclipseprojects.io:8883', NULL, NULL, '{"verify":true,"CApath":"/etc/ssl/certs"}'));
SET @client = (SELECT mqtt_connect('ssl://mqtt.eclipseprojects.io:8883', 'myuser', 'mypasswd', '{"verify":true,"CAfile":"/etc/ssl/certs/ISRG_Root_X1.pem"}'));
```

## mqtt_disconnect

Disconnect from a mqtt server using previous requested handle by [`mqtt_connect()`](#mqtt_connect).

`mqtt_disconnect(handle, {timeout})`

Parameter in `{}` are optional an can be omit.<br>

<dl>
<dt><code>handle</code>   BIGINT</dt>
<dd>Handle previously got from <code>mqtt_connect</code>.</dd>
<dt><code>timeout</code>  INT</dt>
<dd>Optional timeout in ms</dd>
</dl>

Returns 0 if successful.

Examples:

```sql
SELECT mqtt_disconnect(@client);
```

## mqtt_publish

Publish a mqtt payload and returns its status.

Parameter in `{}` are optional an can be omit.<br>
Parameter in `[]` can be `NULL` - in this case a default value is used.<br>
To use optional parameters after omitting other optional parameters, use `NULL`.<br>

Possible call variants:

(1) `mqtt_publish(server, [username], [password], topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})`<br>
(2) `mqtt_publish(client, topic, [payload] {,[qos] {,[retained] {,[timeout] {,[options]}}}})`

Variant (1) connects to MQTT, publish the payload and disconnnect after publish. This variant is provided for individual single `mqtt_publish()` calls.<br>
Because this variant may slow down when a lot of publishing should be done, you can do publish using variant (2) using a client handle from a previous [`mqtt_connect()`](#mqtt_connect).

Variant (2) should be used for multiple `mqtt_publish()` calls with a preceding [`mqtt_connect()`](#mqtt_connect) and a final [`mqtt_disconnect()`](#mqtt_disconnect):

1. Call [`mqtt_connect()`](#mqtt_connect) to get a valid mqtt client connection handle
2. Call [`mqtt_publish()`](#mqtt_publish) using `handle` parameter
3. Repeat step 2. for your needs
4. Call [`mqtt_disconnect()`](#mqtt_disconnect) using `handle` to free the client connection handle

<dl>
<dt><code>server</code>   String</dt>
<dd>Specifying the server to which the client will connect. It takes the form <code>protocol://host:port</code>.<br>Currently <code>protocol</code> must be <code>tcp</code> or <code>ssl</code>.<br>
For <code>host</code> you can specify either an IP address or a hostname.<br>
For instance, to connect to a server running on the local machines with the default MQTT port, specify <code>tcp://localhost:1883</code> or <code>ssl://localhost:8883</code> for an SSL connection.</dd>
<dt><code>username</code> String</dt>
<dd>Username for authentification or <code>NULL</code> if unused</dd>
<dt><code>password</code> String</dt>
<dd>Password for authentification or <code>NULL</code> if unused</dd>
<dt><code>client</code>   BIGINT</dt>
<dd>A valid handle returned from mqtt_connect() call.</dd>
<dt><code>topic</code>    String</dt>
<dd>The topic to be published</dd>
<dt><code>payload</code>  String</dt>
<dd>The message published for the topic</dd>
<dt><code>qos</code>      INT [0..2] (default 0)</dt>
<dd>The QOS (Quality Of Service) number</dd>
<dt><code>retained</code> INT [0,1] (default 0)</dt>
<dd>Flag if message should be retained (1) or not (0)</dd>
<dt><code>timeout</code>  INT</dt>
<dd>Timeout value for connecting to MQTT server (in ms)</dd>
<dt><code>options</code>   String</dt>
<dd>JSON string containing additonal options or NULL if unused.<br>
For details see <code>mqtt_connect()</code></dd>
</dl>

Returns 0 for success, otherwise error code from MQTTClient_connect() (see also http://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/_m_q_t_t_client_8h.html).

You can also retrieve the error code and description using [`mqtt_lasterror()`](#mqtt_lasterror).

## mqtt_subscribe

Subsribe to a mqtt topic and returns the payload if any..

Parameter in `{}` are optional an can be omit.<br>
Parameter in `[]` can be `NULL` - in this case a default value is used.<br>
To use optional parameters after omitting other optional parameters, use `NULL`.<br>

Possible call variants:

(1) `mqtt_subscribe(server, [username], [password], topic, {,[qos] {,[timeout] {,[options]}}})`<br>
(2) `mqtt_subscribe(client, topic, [payload] {,[qos] {,[timeout] {,[options]}}})`

Variant (1) connects to MQTT, subscribes to a topic and disconnnect after subscribe. This variant is provided for individual single `mqtt_subscribe()` calls.<br>
Because this variant may slow down when a lot of subscribes should be done, you can do subscribes using variant (2) using a client handle from a previous [`mqtt_connect()`](#mqtt_connect).

Vvariant (2) should be used for multiple `mqtt_subscribe()` calls with a preceding [`mqtt_connect()`](#mqtt_connect) and a final [`mqtt_disconnect()`](#mqtt_disconnect):

1. Call [`mqtt_connect()`](#mqtt_connect) to get a valid mqtt client connection handle
2. Call [`mqtt_subscribe()`](#mqtt_publish) using `handle` parameter
3. Repeat step 2. for your needs
4. Call [`mqtt_disconnect()`](#mqtt_disconnect) using `handle` to free the client connection handle

<dl>
<dt><code>server</code>   String</dt>
<dd>Specifying the server to which the client will connect. It takes the form <code>protocol://host:port</code>.<br>Currently <code>protocol</code> must be <code>tcp</code> or <code>ssl</code>.<br>
For <code>host</code> you can specify either an IP address or a hostname.<br>
For instance, to connect to a server running on the local machines with the default MQTT port, specify <code>tcp://localhost:1883</code> or <code>ssl://localhost:8883</code> for an SSL connection.</dd>
<dt><code>username</code> String</dt>
<dd>Username for authentification or <code>NULL</code> if unused</dd>
<dt><code>password</code> String</dt>
<dd>Password for authentification or <code>NULL</code> if unused</dd>
<dt><code>client</code>   BIGINT</dt>
<dd>A valid handle returned from mqtt_connect() call.</dd>
<dt><code>topic</code>    String</dt>
<dd>The topic to be published</dd>
<dt><code>payload</code>  String</dt>
<dd>The message published for the topic</dd>
<dt><code>qos</code>      INT [0..2] (default 0)</dt>
<dd>The QOS (Quality Of Service) number</dd>
<dt><code>retained</code> INT [0,1] (default 0)</dt>
<dd>Flag if message should be retained (1) or not (0)</dd>
<dt><code>timeout</code>  INT</dt>
<dd>Timeout value for connecting to MQTT server (in ms)</dd>
<dt><code>options</code>   String</dt>
<dd>JSON string containing additonal options or NULL if unused.<br>
For details see <code>mqtt_connect()</code></dd>
</dl>

Returns 0 for success, otherwise error code from MQTTClient_connect() (see also http://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/_m_q_t_t_client_8h.html).

You can also retrieve the error code and description using [`mqtt_lasterror()`](#mqtt_lasterror).

## mqtt_lasterror

Returns last error as JSON string

```sql
SELECT mqtt_lasterror();
```

Examples:

```sql
> SELECT mqtt_lasterror();

+----------------------------------------------------------------------+
| mqtt_lasterror()                                                     |
+----------------------------------------------------------------------+
| {"func":"MQTTClient_connect","rc":5, "desc": "Unknown error code 5"} |
+----------------------------------------------------------------------+
```

```sql
> SELECT 
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_lasterror(),'$."func"')) AS 'func',
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_lasterror(),'$."rc"')) AS 'rc',
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_lasterror(),'$."desc"')) AS 'desc';

+--------------------+------+----------------------+
| func               | rc   | desc                 |
+--------------------+------+----------------------+
| MQTTClient_connect | 5    | Unknown error code 5 |
+--------------------+------+----------------------+
```

## mqtt_info

Returns library info as JSON string

Examples:

```sql
> SELECT 
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_info(),'$.Name')) AS Name,
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_info(),'$."Version"')) AS Version,
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_info(),'$."Build"')) AS Build;
+-------------------+---------+----------------------+
| Name              | Version | Build                |
+-------------------+---------+----------------------+
| lib_mysqludf_mqtt | 1.0.0   | Nov  1 2021 09:31:40 |
+-------------------+---------+----------------------+

> SELECT 
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_info(),'$."Library"."Product name"')) AS Library,
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_info(),'$."Library"."Version"')) AS `Library Version`,
    JSON_UNQUOTE(JSON_EXTRACT(mqtt_info(),'$."Library"."Build level"')) AS `Library Build`;
+------------------------------------------------+-----------------+-------------------------------+
| Library                                        | Library Version | Library Build                 |
+------------------------------------------------+-----------------+-------------------------------+
| Eclipse Paho Synchronous MQTT C Client Library | 1.3.9           | Sa 23. Okt 11:09:35 CEST 2021 |
+------------------------------------------------+-----------------+-------------------------------+
```

## Troubleshooting

### Paho Library error
If you get some errors that system does not find the MQTT Paho library, make a copy from local lib dir to lib dir:

```bash
sudo cp /usr/local/lib/libpaho-mqtt3* /usr/lib/
```

### Text result as hex character

Function like [`mqtt_lasterror()`](#mqtt_lasterror) returns a hex string instead of a JSON like `0x7B2266756E63223A224D5154544...`.

This is a client setting.
To prevent this start your client with [`--binary-as-hex=0`](https://dev.mysql.com/doc/refman/8.0/en/mysql-command-options.html#option_mysql_binary-as-hex)

If this is not possible, convert the hex-string into a readable one using [CONVERT()](https://dev.mysql.com/doc/refman/8.0/en/cast-functions.html#function_convert):

```sql
SELECT CONVERT(mqtt_info() USING utf8);
```
