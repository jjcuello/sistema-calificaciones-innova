FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential ca-certificates cmake git \
    && apt-get clean

WORKDIR /app
COPY CMakeLists.txt ./
COPY src ./src
COPY web ./web
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --parallel 2

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 \
    && apt-get clean

WORKDIR /app
COPY --from=build /app/build/innova ./innova
COPY --from=build /app/web ./web

ENV PORT=10000
EXPOSE 10000
USER 10001:10001
CMD ["./innova"]
