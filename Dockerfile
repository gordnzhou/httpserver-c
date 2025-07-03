FROM gcc:latest

RUN apt-get update

WORKDIR /httpserver-c

COPY . .

RUN make -C src

CMD ["./src/main"]

EXPOSE 8008