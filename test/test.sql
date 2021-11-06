SELECT mqtt_info();
SELECT
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$.Name')) AS Name,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Version"')) AS Version,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Build"')) AS Build,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."Product name"')) AS Library,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."Version"')) AS `Library Version`,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."Build level"')) AS `Library Build`,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."OpenSSL platform"')) AS `OpenSSL platform`,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."OpenSSL build timestamp"')) AS `OpenSSL build`,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."OpenSSL version"')) AS `OpenSSL version`,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."OpenSSL directory"')) AS `OpenSSL directory`,
    JSON_UNQUOTE(JSON_VALUE(mqtt_info(),'$."Library"."OpenSSL flags"')) AS `OpenSSL flags`;

SELECT mqtt_lasterror();
SELECT
    JSON_UNQUOTE(JSON_VALUE(mqtt_lasterror(),'$.rc')) AS rc,
    JSON_UNQUOTE(JSON_VALUE(mqtt_lasterror(),'$."func"')) AS 'func',
    JSON_UNQUOTE(JSON_VALUE(mqtt_lasterror(),'$."desc"')) AS 'desc';

-- Single mqtt_publish() calls
# 1: mqtt_publish(server, [username], [password], topic, [payload])
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW());
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NULL);
SELECT mqtt_publish('ssl://localhost:8883', 'myuser', NULL,       'dev/test', NOW());
SELECT mqtt_publish('ssl://localhost:8883', 'myuser', 'mypasswd', 'dev/test', NOW(), NULL, NULL, NULL, '{"verify": true}');

# 2: mqtt_publish(server, [username], [password], topic, [payload], [qos])
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', NULL,       'dev/test', NOW(), 0);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), 0);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NULL , 0);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), NULL);

# 3: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained])
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), 0   , 0);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NULL , 0   , 0);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), NULL, 0);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), 0   , NULL);

# 4: mqtt_publish(server, [username], [password], topic, [payload], [qos], [retained], [timeout])
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), 0   , 0   , 1000);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NULL , 0   , 0   , 1000);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), NULL, 0   , 1000);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), 0   , NULL, 1000);
SELECT mqtt_publish('tcp://localhost:1883', 'myuser', 'mypasswd', 'dev/test', NOW(), 0   , 0   , NULL);


-- Multiple mqtt_publish() calls
SET @client = (SELECT mqtt_connect('tcp://localhost:1883', 'myuser', 'mypasswd'));
SELECT @client, HEX(@client);

# 5: mqtt_publish(client, topic, [payload])
SELECT mqtt_publish(@client, 'dev/test', NOW());
SELECT mqtt_publish(@client, 'dev/test', NULL);

# 6: mqtt_publish(client, topic, [payload], [qos])
SELECT mqtt_publish(@client, 'dev/test', NOW(), 0   );
SELECT mqtt_publish(@client, 'dev/test', NULL , 0   );
SELECT mqtt_publish(@client, 'dev/test', NOW(), NULL);

# 7: mqtt_publish(client, topic, [payload], [qos], [retained])
SELECT mqtt_publish(@client, 'dev/test', NOW(), 0   , 0   );
SELECT mqtt_publish(@client, 'dev/test', NULL , 0   , 0   );
SELECT mqtt_publish(@client, 'dev/test', NOW(), NULL, 0   );
SELECT mqtt_publish(@client, 'dev/test', NOW(), 0   , NULL);

# 8: mqtt_publish(client, topic, [payload], [qos], [retained], [timeout])
SELECT mqtt_publish(@client, 'dev/test', NOW(), 0   , 0   , 1000);
SELECT mqtt_publish(@client, 'dev/test', NULL , 0   , 0   , 1000);
SELECT mqtt_publish(@client, 'dev/test', NOW(), NULL, 0   , 1000);
SELECT mqtt_publish(@client, 'dev/test', NOW(), 0   , NULL, 1000);
SELECT mqtt_publish(@client, 'dev/test', NOW(), 0   , 0   , NULL);

SELECT mqtt_disconnect(@client);
