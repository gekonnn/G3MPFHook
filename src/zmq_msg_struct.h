#pragma once

#include "logger.h"

#pragma pack(push, 1)
struct zmq_msg_struct
{
	LogLevel log_level;
	uint32_t size;
	char* data;
};
#pragma pack(pop)

void zms_free(zmq_msg_struct* msg);
size_t zms_size(zmq_msg_struct msg);
char* zms_write(zmq_msg_struct msg, size_t* outSize);
zmq_msg_struct zms_read(char* buf, size_t size);