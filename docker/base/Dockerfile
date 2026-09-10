# ai-dev-base:0.1 -- the common AI dev-agent layer (Claude Code + Codex).
# Per-project images (cpp, nextjs) build FROM this.
FROM node:22-bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      git ripgrep curl ca-certificates less openssh-client tini \
 && rm -rf /var/lib/apt/lists/*

# the agent CLIs, global
RUN npm install -g @anthropic-ai/claude-code @openai/codex \
 && npm cache clean --force

# the stock node:22 image already ships an unprivileged `node` user (uid 1000)
USER node
WORKDIR /workspace

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["bash"]
