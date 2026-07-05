# syntax=docker/dockerfile:1.7

ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS build

ARG JOBS=4
ARG VCS_REF=unknown
ARG BUILD_DATE=unknown

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    KOINOS_DEPS_ROOT=/opt/teleno-deps \
    KOINOS_NODE_BUILD_DIR=/src/node/teleno-node/build

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      autoconf \
      automake \
      build-essential \
      ca-certificates \
      cmake \
      curl \
      file \
      git \
      libssl-dev \
      libtool \
      make \
      ninja-build \
      perl \
      pkg-config \
      python3 \
      tar \
      xz-utils \
      zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN --mount=type=cache,target=/opt/teleno-deps,sharing=locked \
    chmod +x scripts/build-cpp-libp2p-koinos.sh \
    scripts/build-rocksdb-zstd.sh \
    scripts/build-zstd-static.sh \
    scripts/build-gmp-static.sh \
    scripts/build-libssh-static.sh \
    && JOBS="${JOBS}" ./scripts/build-cpp-libp2p-koinos.sh \
    && install -Dm755 "${KOINOS_NODE_BUILD_DIR}/teleno_node" /out/teleno_node \
    && /out/teleno_node --version

FROM ubuntu:${UBUNTU_VERSION} AS runtime

ARG VCS_REF=unknown
ARG BUILD_DATE=unknown

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      ca-certificates \
      libgcc-s1 \
      libstdc++6 \
      python3 \
      tzdata \
      zlib1g \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /data /usr/local/share/teleno/config /usr/local/share/teleno/public-bootstrap

COPY --from=build /out/teleno_node /usr/local/bin/teleno_node
COPY config/testnet-public-bootstrap-observer.yml /usr/local/share/teleno/config/testnet-public-bootstrap-observer.yml
COPY config/testnet-public-bootstrap-observer.container.yml /usr/local/share/teleno/config/testnet-public-bootstrap-observer.container.yml
COPY config/prodnet-docker-producer.yml /usr/local/share/teleno/config/prodnet-docker-producer.yml
COPY config/public-bootstrap/ /usr/local/share/teleno/public-bootstrap/
COPY docker/teleno-prod-producer /usr/local/bin/teleno-prod-producer
RUN chmod +x /usr/local/bin/teleno-prod-producer

LABEL org.opencontainers.image.title="Teleno Node" \
      org.opencontainers.image.description="Linux container image for the monolithic teleno_node runtime" \
      org.opencontainers.image.source="https://github.com/koinos/koinos-one" \
      org.opencontainers.image.revision="${VCS_REF}" \
      org.opencontainers.image.created="${BUILD_DATE}"

VOLUME ["/data"]
WORKDIR /data

EXPOSE 8080/tcp 8888/tcp 18088/tcp 18122/tcp 18888/tcp

ENTRYPOINT ["teleno_node"]
CMD ["--help"]
