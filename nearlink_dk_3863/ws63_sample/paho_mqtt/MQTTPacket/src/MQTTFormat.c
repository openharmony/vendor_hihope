/**
 * Copyright (c) 2020 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.  *
 *
 * History: \n
 * 2022-09-16， Create file. \n
 */

#include "StackTrace.h"
#include "MQTTPacket.h"

#include <string.h>


const char* MQTTPacket_names[] =
{
	"RESERVED", "CONNECT", "CONNACK", "PUBLISH", "PUBACK", "PUBREC", "PUBREL",
	"PUBCOMP", "SUBSCRIBE", "SUBACK", "UNSUBSCRIBE", "UNSUBACK",
	"PINGREQ", "PINGRESP", "DISCONNECT"
};


const char* MQTTPacket_getName(unsigned short packetid)
{
	return MQTTPacket_names[packetid];
}


int MQTTStringFormat_connect(char* strbuf, int strbuflen, MQTTPacket_connectData* data)
{
	int strindex = 0;

	strindex = snprintf(strbuf, strbuflen,
			"CONNECT MQTT version %d, client id %.*s, clean session %d, keep alive %d",
			(int)data->MQTTVersion, data->clientID.lenstring.len, data->clientID.lenstring.data,
			(int)data->cleansession, data->keepAliveInterval);
	if (data->willFlag)
		strindex += snprintf(&strbuf[strindex], strbuflen - strindex,
				", will QoS %d, will retain %d, will topic %.*s, will message %.*s",
				data->will.qos, data->will.retained,
				data->will.topicName.lenstring.len, data->will.topicName.lenstring.data,
				data->will.message.lenstring.len, data->will.message.lenstring.data);
	if (data->username.lenstring.data && data->username.lenstring.len > 0)
		strindex += snprintf(&strbuf[strindex], strbuflen - strindex,
				", user name %.*s", data->username.lenstring.len, data->username.lenstring.data);
	if (data->password.lenstring.data && data->password.lenstring.len > 0)
		strindex += snprintf(&strbuf[strindex], strbuflen - strindex,
				", password %.*s", data->password.lenstring.len, data->password.lenstring.data);
	return strindex;
}


int MQTTStringFormat_connack(char* strbuf, int strbuflen, unsigned char connack_rc, unsigned char sessionPresent)
{
	int strindex = snprintf(strbuf, strbuflen, "CONNACK session present %d, rc %d", sessionPresent, connack_rc);
	return strindex;
}


int MQTTStringFormat_publish(char* strbuf, int strbuflen, unsigned char dup, int qos, unsigned char retained,
		unsigned short packetid, MQTTString topicName, unsigned char* payload, int payloadlen)
{
	int strindex = snprintf(strbuf, strbuflen,
				"PUBLISH dup %d, QoS %d, retained %d, packet id %d, topic %.*s, payload length %d, payload %.*s",
				dup, qos, retained, packetid,
				(topicName.lenstring.len < 20) ? topicName.lenstring.len : 20, topicName.lenstring.data,
				payloadlen, (payloadlen < 20) ? payloadlen : 20, payload);
	return strindex;
}


int MQTTStringFormat_ack(char* strbuf, int strbuflen, unsigned char packettype, unsigned char dup, unsigned short packetid)
{
	int strindex = snprintf(strbuf, strbuflen, "%s, packet id %d", MQTTPacket_names[packettype], packetid);
	if (dup)
		strindex += snprintf(strbuf + strindex, strbuflen - strindex, ", dup %d", dup);
	return strindex;
}


int MQTTStringFormat_subscribe(char* strbuf, int strbuflen, unsigned char dup, unsigned short packetid, int count,
		MQTTString topicFilters[], int requestedQoSs[])
{
	return snprintf(strbuf, strbuflen,
		"SUBSCRIBE dup %d, packet id %d count %d topic %.*s qos %d",
		dup, packetid, count,
		topicFilters[0].lenstring.len, topicFilters[0].lenstring.data,
		requestedQoSs[0]);
}


int MQTTStringFormat_suback(char* strbuf, int strbuflen, unsigned short packetid, int count, int* grantedQoSs)
{
	return snprintf(strbuf, strbuflen,
		"SUBACK packet id %d count %d granted qos %d", packetid, count, grantedQoSs[0]);
}


int MQTTStringFormat_unsubscribe(char* strbuf, int strbuflen, unsigned char dup, unsigned short packetid,
		int count, MQTTString topicFilters[])
{
	return snprintf(strbuf, strbuflen,
					"UNSUBSCRIBE dup %d, packet id %d count %d topic %.*s",
					dup, packetid, count,
					topicFilters[0].lenstring.len, topicFilters[0].lenstring.data);
}


#if defined(MQTT_CLIENT)
char* MQTTFormat_toClientString(char* strbuf, int strbuflen, unsigned char* buf, int buflen)
{
	int index = 0;
	int rem_length = 0;
	MQTTHeader header = {0};
	int strindex = 0;

	header.byte = buf[index++];
	index += MQTTPacket_decodeBuf(&buf[index], &rem_length);

	switch (header.bits.type)
	{

	case CONNACK:
	{
		unsigned char sessionPresent, connack_rc;
		if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) == 1)
			strindex = MQTTStringFormat_connack(strbuf, strbuflen, connack_rc, sessionPresent);
	}
	break;
	case PUBLISH:
	{
		unsigned char dup, retained, *payload;
		unsigned short packetid;
		int qos, payloadlen;
		MQTTString topicName = MQTTString_initializer;
		if (MQTTDeserialize_publish(&dup, &qos, &retained, &packetid, &topicName,
				&payload, &payloadlen, buf, buflen) == 1)
			strindex = MQTTStringFormat_publish(strbuf, strbuflen, dup, qos, retained, packetid,
					topicName, payload, payloadlen);
	}
	break;
	case PUBACK:
	case PUBREC:
	case PUBREL:
	case PUBCOMP:
	{
		unsigned char packettype, dup;
		unsigned short packetid;
		if (MQTTDeserialize_ack(&packettype, &dup, &packetid, buf, buflen) == 1)
			strindex = MQTTStringFormat_ack(strbuf, strbuflen, packettype, dup, packetid);
	}
	break;
	case SUBACK:
	{
		unsigned short packetid;
		int maxcount = 1, count = 0;
		int grantedQoSs[1];
		if (MQTTDeserialize_suback(&packetid, maxcount, &count, grantedQoSs, buf, buflen) == 1)
			strindex = MQTTStringFormat_suback(strbuf, strbuflen, packetid, count, grantedQoSs);
	}
	break;
	case UNSUBACK:
	{
		unsigned short packetid;
		if (MQTTDeserialize_unsuback(&packetid, buf, buflen) == 1)
			strindex = MQTTStringFormat_ack(strbuf, strbuflen, UNSUBACK, 0, packetid);
	}
	break;
	case PINGREQ:
	case PINGRESP:
	case DISCONNECT:
		strindex = snprintf(strbuf, strbuflen, "%s", MQTTPacket_names[header.bits.type]);
		break;
	}
	return strbuf;
}
#endif

#if defined(MQTT_SERVER)
char* MQTTFormat_toServerString(char* strbuf, int strbuflen, unsigned char* buf, int buflen)
{
	int index = 0;
	int rem_length = 0;
	MQTTHeader header = {0};
	int strindex = 0;

	header.byte = buf[index++];
	index += MQTTPacket_decodeBuf(&buf[index], &rem_length);

	switch (header.bits.type)
	{
	case CONNECT:
	{
		MQTTPacket_connectData data;
		int rc;
		if ((rc = MQTTDeserialize_connect(&data, buf, buflen)) == 1)
			strindex = MQTTStringFormat_connect(strbuf, strbuflen, &data);
	}
	break;
	case PUBLISH:
	{
		unsigned char dup, retained, *payload;
		unsigned short packetid;
		int qos, payloadlen;
		MQTTString topicName = MQTTString_initializer;
		if (MQTTDeserialize_publish(&dup, &qos, &retained, &packetid, &topicName,
				&payload, &payloadlen, buf, buflen) == 1)
			strindex = MQTTStringFormat_publish(strbuf, strbuflen, dup, qos, retained, packetid,
					topicName, payload, payloadlen);
	}
	break;
	case PUBACK:
	case PUBREC:
	case PUBREL:
	case PUBCOMP:
	{
		unsigned char packettype, dup;
		unsigned short packetid;
		if (MQTTDeserialize_ack(&packettype, &dup, &packetid, buf, buflen) == 1)
			strindex = MQTTStringFormat_ack(strbuf, strbuflen, packettype, dup, packetid);
	}
	break;
	case SUBSCRIBE:
	{
		unsigned char dup;
		unsigned short packetid;
		int maxcount = 1, count = 0;
		MQTTString topicFilters[1];
		int requestedQoSs[1];
		if (MQTTDeserialize_subscribe(&dup, &packetid, maxcount, &count,
				topicFilters, requestedQoSs, buf, buflen) == 1)
			strindex = MQTTStringFormat_subscribe(strbuf, strbuflen, dup, packetid, count, topicFilters, requestedQoSs);;
	}
	break;
	case UNSUBSCRIBE:
	{
		unsigned char dup;
		unsigned short packetid;
		int maxcount = 1, count = 0;
		MQTTString topicFilters[1];
		if (MQTTDeserialize_unsubscribe(&dup, &packetid, maxcount, &count, topicFilters, buf, buflen) == 1)
			strindex =  MQTTStringFormat_unsubscribe(strbuf, strbuflen, dup, packetid, count, topicFilters);
	}
	break;
	case PINGREQ:
	case PINGRESP:
	case DISCONNECT:
		strindex = snprintf(strbuf, strbuflen, "%s", MQTTPacket_names[header.bits.type]);
		break;
	}
	strbuf[strbuflen] = '\0';
	return strbuf;
}
#endif
