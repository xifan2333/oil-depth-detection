# 使用官方Go镜像作为构建环境
FROM golang:1.21-alpine AS builder

# 设置工作目录
WORKDIR /app

# 安装必要的系统依赖
RUN apk add --no-cache gcc musl-dev sqlite-dev

# 复制go.mod和go.sum文件
COPY go.mod go.sum ./

# 下载依赖
RUN go mod download

# 复制源代码
COPY . .

# 构建应用
RUN CGO_ENABLED=1 GOOS=linux go build -o main .

# 使用轻量级的alpine作为运行环境
FROM alpine:latest

# 设置工作目录
WORKDIR /app

# 安装必要的运行时依赖
RUN apk add --no-cache ca-certificates tzdata sqlite

# 设置时区为上海
ENV TZ=Asia/Shanghai

# 创建数据目录
RUN mkdir -p /app/data

# 从builder阶段复制编译好的应用
COPY --from=builder /app/main .
# 复制静态文件
COPY --from=builder /app/static ./static

# 暴露端口
EXPOSE 8080

# 启动应用
CMD ["./main"] 