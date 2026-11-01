FROM node:22-alpine
WORKDIR /app
COPY package.json server.mjs udp-probe.mjs auth.mjs warlink-mark-white.svg ./
ENV NODE_ENV=production DATA_DIR=/data
EXPOSE 3000
CMD ["node","server.mjs"]
