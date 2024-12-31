# 使用官方Go镜像作为构建环境
FROM golang:1.21-alpine AS builder

# 设置工作目录
WORKDIR /app

# 设置 Go 代理
ENV GOPROXY=https://goproxy.cn,direct
ENV GO111MODULE=on

# 使用阿里云的 Alpine 镜像源
RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories

# 安装必要的系统依赖
RUN apk add --no-cache gcc musl-dev sqlite-dev

# 复制go.mod和go.sum文件
COPY go.mod go.sum ./

# 下载依赖并验证
RUN go mod download && go mod verify

# 复制源代码
COPY . .

# 先进行代码检查
RUN go vet ./...

# 构建应用
RUN CGO_ENABLED=1 GOOS=linux go build -o main . && \
    # 验证二进制文件是否存在且可执行
    test -x main

# 使用轻量级的alpine作为运行环境
FROM alpine:latest

# 设置工作目录
WORKDIR /app

# 使用阿里云的 Alpine 镜像源
RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories

# 安装必要的运行时依赖
RUN apk add --no-cache ca-certificates tzdata sqlite

# 设置时区为上海
ENV TZ=Asia/Shanghai

# 从builder阶段复制编译好的应用
COPY --from=builder /app/main .
# 复制静态文件
COPY --from=builder /app/static ./static

# 验证文件是否正确复制
RUN test -f main && test -x main && \
    test -d static

# 暴露端口
EXPOSE 8080

# 启动应用
CMD ["./main"] 