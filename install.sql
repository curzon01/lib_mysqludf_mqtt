/*
	lib_mysqludf_mqtt - a library with miscellaneous (operating) client functions
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

DROP FUNCTION IF EXISTS mqtt_info;
DROP FUNCTION IF EXISTS mqtt_lasterror;
DROP FUNCTION IF EXISTS mqtt_connect;
DROP FUNCTION IF EXISTS mqtt_disconnect;
DROP FUNCTION IF EXISTS mqtt_publish;
DROP FUNCTION IF EXISTS mqtt_subscribe;

CREATE FUNCTION mqtt_info RETURNS STRING SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_lasterror RETURNS STRING SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_connect RETURNS INTEGER SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_disconnect RETURNS INTEGER SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_publish RETURNS INTEGER SONAME 'lib_mysqludf_mqtt.so';
CREATE FUNCTION mqtt_subscribe RETURNS STRING SONAME 'lib_mysqludf_mqtt.so';
