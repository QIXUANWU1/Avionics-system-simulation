FROM ubuntu:22.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev cyclonedds-dev cyclonedds-tools \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY core/ core/
COPY config/ config/
COPY dds/ dds/
COPY src/ src/
COPY include/ include/
COPY third_party/ third_party/
COPY build.sh ./
RUN bash build.sh
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends libddsc0 && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/ima-worker /ima-worker
COPY --from=build /src/build/ima-record-mirror /ima-record-mirror
COPY --from=build /src/build/ima-panel /ima-panel
COPY --from=build /src/build/ima-health /ima-health
USER 1000:1000
ENTRYPOINT ["/ima-worker"]
