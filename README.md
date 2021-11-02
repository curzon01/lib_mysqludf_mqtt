[![master](https://img.shields.io/badge/master-v1.0.0-blue.svg)](https://github.com/curzon01/lib_mysqludf_mqtt/tree/master)
[![License](https://img.shields.io/github/license/curzon01/lib_mysqludf_mqtt.svg)](LICENSE)

# MySQL loadable function for MQTT

This repository contains the source code for a [MySQL loadable function](https://dev.mysql.com/doc/extending-mysql/8.0/en/adding-loadable-function.html) library (previously called UDF - User Defined Functions), which provides some additonal SQL functions to interact with your MQTT server for publish and subscribe MQTT topics.

If you like **lib_mysqludf_mqtt** give it a star or fork it:

[![GitHub stars](https://img.shields.io/github/stars/curzon01/lib_mysqludf_mqtt.svg?style=social&label=Star)](https://github.com/curzon01/lib_mysqludf_mqtt/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/curzon01/lib_mysqludf_mqtt.svg?style=social&label=Fork)](https://github.com/curzon01/lib_mysqludf_mqtt/network)

## Build instructions for GNU Make

Ensure the [Eclipse Paho C Client Library for the MQTT Protocol](https://github.com/eclipse/paho.mqtt.c) is installed.

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

`mqtt_connect(server {, username} {, password}})`

Parameter in curly braces {} are optional.

<dl>
<dt><code>server</code>    String</dt>
<dd>Specifying the MQTT server to which the client should connected. It takes the form protocol://host:port. Currently, protocol must be tcp or ssl. For host, you can specify either an IP address or a host name. For instance, to connect to a server running on the local machines with the default MQTT port, specify "tcp://localhost:1883".</dd>
<dt><code>username</code>  String</dt>
<dd>Username for authentification. If <code>username</code> should remain unused, omit the parameter or set it to <code>NULL</code></dd>
<dt><code>password</code>  String</dt>
<dd>Password for authentification. If the <code>password</code> should remain unused, omit the parameter or set it to <code>NULL</code></dd>
</dl>

Returns a valid handle or 0 on error

Examples:

```sql
SET @client = (SELECT mqtt_connect('tcp://localhost:1883', NULL, NULL));
SET @client = (SELECT mqtt_connect('tcp://localhost:1883', 'myuser', 'mypasswd'));
```

## mqtt_disconnect

Disconnect from a mqtt server using previous requested handle by [`mqtt_connect()`](#mqtt_connect).

`mqtt_disconnect(handle, {timeout})`

Parameter in curly braces {} are optional.

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
Parameter in curly braces {} are optional.

Possible call variants:

`mqtt_publish(server, username, password, topic, {payload {,qos {,retained {,timeout}}}})`

This call connects to MQTT, publish the payload and disconnnect after publish. This variant is provided for individual single `mqtt_publish()` calls.

`mqtt_publish(handle, topic, {payload {,qos {,retained {,timeout}}}})`

Because the previous variant may slow down when a lot of publishing should be done you can do publish using an alternate way with a client handle. This variant should be used for multiple `mqtt_publish()` calls with a preceding [`mqtt_connect()`](#mqtt_connect) and a final [`mqtt_disconnect()`](#mqtt_disconnect):

1. Call [`mqtt_connect()`](#mqtt_connect) to get a valid mqtt client connection handle
2. Call [`mqtt_publish()`](#mqtt_publish) using `handle` parameter
3. Repeat step 2. for your needs
4. Call [`mqtt_disconnect()`](#mqtt_disconnect) using `handle` to free the client connection handle

<dl>
<dt><code>server</code>   String</dt>
<dd>Specifying the server to which the client will connect. It takes the form protocol://host:port.
Currently protocol must be tcp or ssl.
For host, you can specify either an IP address or a host name.
For instance, to connect to a server running on the local machines with the default MQTT port, specify <code>tcp://localhost:1883</code>.</dd>
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
