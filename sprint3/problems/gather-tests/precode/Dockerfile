FROM conanio/gcc11:latest

RUN pip install conan==1.54.0

WORKDIR /app

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

COPY . /app/
RUN cd /app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build .