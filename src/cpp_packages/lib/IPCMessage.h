/*
 * Define the message structure used in IPC
 */

/**
 * The following values are used in `msg_type`.
 */

#define STATUS    0
#define AUTH      1
#define READ      2
#define WRITE     3
#define CON_REPLY 4

namespace IPC{

/**
 * All lengthes in message are counted in bytes.
 *
 * All passwords are encrypted by hash function.
 * So they have fix length.
 *
 * All properties are bitmap.
 */
typedef struct{
	uint16_t status_type;			
}StatusINFO;

typedef struct{
	uint16_t username_length;
	char* username;
	char[64] password;
}AuthINFO;

typedef struct{
	uint16_t path_length;
	char* path_value;
	uint32_t properties;
}ReadINFO;

typedef struct{
	uint16_t path_length;
	char* path_value;
	uint32_t properties;
	uint64_t starting_address;
	uint64_t data_length;
}WriteINFO;

typedef struct{
	uint64_t write_buffer_address;
	uint32_t write_buffer_size;
	uint64_t read_buffer_address;
	uint32_t read_buffer_size;
	char[32] write_buffer_name;
	char[32] read_buffer_name;
}ReplyINFO;


typedef struct IPCMessage{
	uint8_t msg_type;
	uint32_t msg_length;
	union{
		StatusINFO status;
		AuthINFO auth;
		ReadINFO read;
		WriteINFO write;
		ReplyINFO reply; 
	}
} IPCMessage;

}