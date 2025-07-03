# HTTP Server (in C)

### Docker commands
```bash
# build container
docker build -t http_server .

# run container on port 8008
docker run --rm -it --name="httpserver-c" -w="/httpserver-c" -p 8008:8008 http_server 
```