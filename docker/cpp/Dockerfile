# ai-dev-cpp:0.1 -- ai-dev-base + a C/C++ toolchain (matches config/project-types.json "cpp").
FROM ai-dev-base:0.1

USER root
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake gdb ninja-build pkg-config \
 && rm -rf /var/lib/apt/lists/*
USER node
