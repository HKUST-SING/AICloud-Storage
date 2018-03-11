#Worker Interface

- librados object -- for communication with the ceph clusteres
- read/write data (object shared memory address and length)
- update quota to authentication module after completed writes




###Task 

1. Username
2. Path
3. Read/Write/Delete (operation type)
4. Data address (shared memory)
5. Data length (shared memory)
6. Transction ID (uint32_t)
7. Worker ID (uint32_t)
7. Op code (result infomation)

###Method

`Future <Task> write (Task);`

`Future <Task> read (Task);`

`Future <Task> delete (Task);`

`Future <Task> completeRead(Task);`



**inner:** subdivide mulitiple rados objects (TODO: consider how to lock)