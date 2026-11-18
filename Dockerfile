FROM gcc:12-bookworm AS build
WORKDIR /src
COPY core/ core/
COPY config/ config/
COPY src/ src/
COPY include/ include/
COPY third_party/ third_party/
COPY build.sh ./
RUN bash build.sh
FROM scratch
COPY --from=build /src/build/ima-worker /ima-worker
USER 1000:1000
ENTRYPOINT ["/ima-worker"]
