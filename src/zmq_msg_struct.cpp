#include "pch.h"

static void write_bytes(char** ptr, void* src, size_t size) {
	memcpy(*ptr, src, size);
	*ptr += size;
}

static void write_uint32(char** ptr, uint32_t val) {
	write_bytes(ptr, &val, sizeof(val));
}

static void write_uint8(char** ptr, uint8_t val) {
	write_bytes(ptr, &val, sizeof(val));
}

static void read_bytes(char** ptr, void* dst, size_t size) {
	memcpy(dst, *ptr, size);
	*ptr += size;
}

static uint16_t read_uint32(char** ptr) {
	uint32_t val;
	read_bytes(ptr, &val, sizeof(val));
	return val;
}

static uint8_t read_uint8(char** ptr) {
	uint8_t val;
	read_bytes(ptr, &val, sizeof(val));
	return val;
}

void zms_free(zmq_msg_struct* msg)
{
	msg->size = 0;
	free(msg->data);
	msg->data = NULL;
}

size_t zms_size(zmq_msg_struct msg)
{
	size_t size = 0;
	size += 1; // type
	size += sizeof(msg.size);
	size += msg.size + 1;
	return size;
}

char* zms_write(zmq_msg_struct msg, size_t* outSize)
{
	*outSize = zms_size(msg);
	char* buf = (char*)malloc(*outSize);
	char* ptr = buf;

	write_uint8(&ptr, msg.log_level);
	write_uint32(&ptr, msg.size);
	write_bytes(&ptr, msg.data, msg.size + 1);

	return buf;
}

zmq_msg_struct zms_read(char* buf, size_t size)
{
	zmq_msg_struct msg{};
	char* ptr = buf;

	msg.log_level = (LogLevel)read_uint8(&ptr);
	msg.size = read_uint32(&ptr);
	msg.data = (char*)malloc(msg.size + 1);
	read_bytes(&ptr, msg.data, msg.size + 1);

	return msg;
}
