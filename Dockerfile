FROM gcc:latest

RUN apt-get update

WORKDIR /httpserver-c

COPY . .

RUN make

CMD ["make", "run"]

EXPOSE 8008