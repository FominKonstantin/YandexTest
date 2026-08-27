FROM gcc:11.3 as build

RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install conan==1.*

RUN conan profile new default --detect || true && \
    conan profile update settings.compiler.libcxx=libstdc++11 default || true

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11

COPY ./src /app/src
COPY ./lib /app/lib  
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04
RUN apt update && apt install -y --no-install-recommends libstdc++6 && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /tmp/volume

COPY --from=build /app/build/bin/game_server /app/game_server

COPY ./static /app/static
COPY ./data /app/data
WORKDIR /app

RUN ulimit -s unlimited

ENTRYPOINT ["/app/game_server"]