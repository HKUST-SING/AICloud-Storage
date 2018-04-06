## HTTP Message

#### Authentication

```http
GET /auth HTTP/1.1
Host: {server_address:port}
Keep-Alive: timeout={value larger than heartbeat}\r\n\r\n
X-Auth-User: {username}
X-Auth-Key:  {password}
X-Tran-Id:   {transaction id}
```


#### Object Get
```http 
GET /{bucket_name}/{object_name} HTTP/1.1
Host: {Server_address:port}
Accept: application/json;charset=utf-8
X-Auth-User: {username}
X-Auth-Key:  {password}
X-Tran-Id:   {transaction id}\r\n\r\n
```


#### Object Write

```http
PUT /{bucket_name}/{object_name} HTTP/1.1
Host: {Server_address:port}
Accept: application/json;charset=utf-8
X-Auth-User: {username}
X-Auth-Key:  {password}
X-Tran-Id:   {transaction id}\r\n\r\n
```

#### Object Write Commit
```http
POST /{bucket_name}/{object_name} HTTP/1.1
Host: {Server_address:port}
Content-Type: application/json;charset=utf-8
Content-Length: {size of the body}
Accept: application/json;charset=utf-8
X-Auth-User: {username}
X-Auth-Key:  {password}
X-Tran-Id:   {transaction id}\r\n\r\n
```


#### Object Delete

```http
DELETE /{bucket_name}/{object_name} HTTP/1.1
Host: {Server_address:port}
Accept: application/json;charset=utf-8
X-Auth-User: {username}
X-Auth-Key:  {password}
X-Tran-Id:   {transaction id}\r\n\r\n
```


### Generic response
```http
HTTP/1.1 HTTP_CODE HTTP_MESSAGE
Content-Type: application/json;charset=utf-8
Content-Legth: {size of the body}
Connection: close
Content-Length: {size of the body}
X-Tran-Id: {transaction id}\r\n\r\n
```
