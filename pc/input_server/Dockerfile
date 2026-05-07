FROM alpine:latest

#RUN apk add --no-cache \
RUN apk add \
    build-base \
    cmake \
    boost-dev \
    git

#websocketpp headers
RUN git clone https://github.com/zaphoyd/websocketpp.git /usr/local/include/websocketpp --depth=1

WORKDIR /app
COPY . .

RUN cmake -S . -B build && cmake --build build

CMD ["./build/input_server"]