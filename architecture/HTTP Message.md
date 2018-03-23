## HTTP Message

#### Authentication

```http
GET /auth HTTP/1.1
Host: {server_address:port}
Content-Length: {size of the body}
Accept: application/json;charset=utf-8
Keep-Alive: timeout={value larger than heartbeat}
Authorization: Basic name:password (in base64)\r\n\r\n

{"Transaction_ID":uint32_t}
```

```http
HTTP/1.1 200 OK
Content-Type: application/json;charset=utf-8
Content-Length: {size of the body}\r\n\r\n

{"Transaction_ID":uint32_t,
 "User_ID":uint64_t}
```

```http
HTTP/1.1 404 NOT FOUND
Content-Type: application/json;charset=utf-8
Connection: close
Content-Length: {size of the body}\r\n\r\n

{"Transaction_ID":uint32_t,
 "Error_Type":uint8_t}
```

#### Object Get
```http
GET /sing/{bucket_name}/{object_name} HTTP/1.1
Host: {Server_address:port}
Content-Length: {size of the body}
Accept: application/json;charset=utf-8
Authorization: SING_AUTH USER_ID:secret_key\r\n\r\n

{"Transaction_ID":uint32_t(,"Commit":uint)}
```

```http
HTTP/1.1 200 OK
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t,
 "Pending_Read"  :uint,
 metadata}
```

```http
HTTP/1.1 401 UNAUTHORIZED
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t}
```

```http
HTTP/1.1 404 NOT FOUND
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t}
```

#### Object Write

```http
PUT /sing/{bucket_name}/{object_name} HTTP/1.1
Host: {Server_address:port}
Content-Length: {size of the body}
Accept: application/json;charset=utf-8
Authorization: SING_AUTH USER_ID:secret_key\r\n\r\n

{"Transaction_ID":uint32_t,
 "Object_Size"   :uint64_t(,"Commit":uint)}
```

```http
HTTP/1.1 200 OK
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t,
 "Pending_Write" :uint,
 metadata}
```

```http
HTTP/1.1 401 UNAUTHORIZED
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t}
```

```http
HTTP/1.1 404 NOT FOUND
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t}
```

```http
HTTP/1.1 451 UNAVAILABLE FOR LEGAL REASON
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t,
 "Error_Type"    :uint8_t}
```

#### Resume Pending Reading/Writing

```http
HTTP/1.1 200 OK
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t,
 "Continue"      :uint(,metadata)}
```

#### Object Delete

```http
DELETE /sing/{bucket_name}/{object_name} HTTP/1.1
Host: {Server_address:port}
Content-Length: {size of the body}
Accept: application/json;charset=utf-8
Authorization: SING_AUTH USER_ID:secret_key\r\n\r\n

{"Transaction_ID":uint32_t}
```

```http
HTTP/1.1 200 OK
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t}
```

```http
HTTP/1.1 401 UNAUTHORIZED
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t}
```

```http
HTTP/1.1 404 NOT FOUND
Content-Type: application/json;charset=utf-8
Content-Length: {size of body}\r\n\r\n

{"Transaction_ID":uint32_t}
```

