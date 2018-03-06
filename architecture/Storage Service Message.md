## Inter-Processing Message Structure

| Data type | Generic               |
| --------- | --------------------- |
| `uint8_t` | msg_type              |
| `uint8_t` | msg_length (in bytes) |
|           | specific content      |

| Data type    | Read/Write msg_type    |
| ------------ | ---------------------- |
| ``uint64_t`` | data_length (in bytes) |
| ``uint64_t`` | starting address       |

| Data type  | Error msg_type |
| ---------- | -------------- |
| `uint16_t` | error_type     |

```c++
struct inter_processing_msg{
    uint8_t msg_type; // B
    uint8_t msg_length; // B
    union specific_content{
        struct rw{
            uint32_t data_length; // I
            uint64_t starting_addr; // Q
        };
        uint16_t error_type; // H
    };
};
```



## Inter-Connecting Message

we will use **google protocol buffer**
