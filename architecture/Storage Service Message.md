## Inter-Processing Message Structure

| Data type  | Generic                                     |
| ---------- | ------------------------------------------- |
| `uint8_t`  | msg_type (STATUS,AUTH,READ,WRITE,CON_REPLY) |
| `uint32_t` | msg_length (in bytes)                       |
|            | specific content (describe in below)        |

| Data type  | Authentication             |
| ---------- | -------------------------- |
| `uint16_t` | username_length (in bytes) |
| `char*`    | username                   |
| `char[64]` | password (hash)            |

| Data type  | Read                   |
| ---------- | ---------------------- |
| `uint16_t` | path_length (in bytes) |
| `char*`    | path_value             |
| `uint32_t` | prperties (bitmap)     |

| Data type  | Write                  |
| ---------- | ---------------------- |
| `uint16_t` | path_length (in bytes) |
| `char*`    | path_value             |
| `uint32_t` | properties (bitmap)    |
| `uint64_t` | starting address       |
| `uint64_t` | data_length (in bytes) |

| Data type  | Status      |
| ---------- | ----------- |
| `uint16_t` | status_type |

| Data type  | CON_REPLY            |
| ---------- | -------------------- |
| `uint64_t` | write_buffer_address |
| `uint32_t` | write_buffer_size    |
| `uint64_t` | read_buffer_address  |
| `uint32_t` | read_buffer_size     |
| `char[64]` | write_buffer_name    |
| `char[64]` | read_buffer_name     |


## Inter-Connecting Message

we will use **google protocol buffer**