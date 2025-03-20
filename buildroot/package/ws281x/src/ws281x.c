#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <getopt.h>

#define DEFAULT_SPI_DEVICE        "/dev/spidev0.0"
#define DEFAULT_SPEED     6000000
#define DEFAULT_LED_COUNT 23
#define MAX_LED_COUNT     256
#define BITS_PER_BYTE     8
#define RESET_US          50

// WS281x 的 0 和 1 时序对应的 SPI 数据
#define WS281x_0 0b11000000
#define WS281x_1 0b11111000

typedef struct {
    int index;
    uint8_t r, g, b;
} led_info;

// RGB 转换为 SPI 数据 (3 字节 RGB -> 72 字节 SPI 数据)
void ws281x_encode(const uint8_t *rgb_data, uint8_t *spi_data, size_t count) {
    size_t i, j, k = 0;
    for (i = 0; i < count; ++i) {
        for (j = 0; j < 8; ++j) {
            if (rgb_data[i] & (1 << (7 - j)))
                spi_data[k++] = WS281x_1;  // 1 对应的 SPI 数据
            else
                spi_data[k++] = WS281x_0;  // 0 对应的 SPI 数据
        }
    }
}

// 解析 `-p` 参数，获取多个 LED 索引及其颜色
void parse_led_custom(const char *arg, led_info *leds, int *count) {
    char *copy = strdup(arg);  // 复制字符串，防止 `strtok` 破坏原始数据
    char *token = strtok(copy, " ");  // 以空格拆分每个 LED 配置项

    while (token != NULL) {
        int idx, r, g, b;
        if (sscanf(token, "%d:%d,%d,%d", &idx, &r, &g, &b) == 4) {
            leds[*count].index = idx;
            leds[*count].r = (uint8_t)r;
            leds[*count].g = (uint8_t)g;
            leds[*count].b = (uint8_t)b;
            (*count)++;
        } else {
            fprintf(stderr, "Invalid LED format: %s\n", token);
        }

        token = strtok(NULL, " ");  // 继续解析下一个 LED
    }

    free(copy);
}

int main(int argc, char *argv[]) {
    int fd, ret;
    uint8_t mode = 0;
    uint32_t speed = DEFAULT_SPEED;
    uint8_t bits = BITS_PER_BYTE;
    int led_count = DEFAULT_LED_COUNT;
    led_info led_custom[MAX_LED_COUNT];
    int led_custom_count = 0;
    size_t reset_len = 0;
    size_t verbose_mode = 0;
    int reset_delay_us = RESET_US;
    const char *spi_dev = DEFAULT_SPI_DEVICE;
    uint8_t red = 0, green = 0, blue = 5;

    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "d:s:l:r:g:b:R:p:v")) != -1) {
        switch (opt) {
            case 'd':
                spi_dev = optarg;
                break;
            case 's':
                speed = atoi(optarg);
                break;
            case 'l':
                led_count = atoi(optarg);
                break;
            case 'r':
                red = (uint8_t)atoi(optarg);
                break;
            case 'g':
                green = (uint8_t)atoi(optarg);
                break;
            case 'b':
                blue = (uint8_t)atoi(optarg);
                break;
            case 'R':
                reset_delay_us = atoi(optarg);
                break;
            case 'p':
                parse_led_custom(optarg, led_custom, &led_custom_count);  // 解析多个 LED 索引
                break;
            case 'v':
                verbose_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s -d <spi_device> -s <speed> -l <led_count> -r <red_brightness> -g <green_brightness> -b <blue_brightness> -R <reset_delay_us> -p 'led_custom' [-v]\n", argv[0]);
                fprintf(stderr, "Example: %s -d '/dev/spidev0.0' -s 600000 -l 23 -r 255 -g 128 -b 64 -R 50 -p '0:255,0,0 2:0,255,0 4:0,0,255' -v\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    printf("SPI Device: %s\n", spi_dev);
    printf("SPI Speed: %d\n", speed);
    printf("LED Count: %d\n", led_count);
    printf("RGB: (%d, %d, %d)\n", red, green, blue);
    printf("Reset Delay: %d us\n", reset_delay_us);
    printf("LED Custom: ");
    for (int i = 0; i < led_custom_count; i++) {
        printf("%d ", led_custom[i]);
    }
    printf("\n");

    // 打开 SPI 设备
    fd = open(spi_dev, O_WRONLY);
    if (fd < 0) {
        perror("open spi_dev failed\n");
        return -1;
    }

    // 配置 SPI
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    // 发送 reset 信号
    if (reset_delay_us > 0) {
        reset_len = (speed / 8) * reset_delay_us / 1000000 + 1;
    }

    // 分配 RGB 数据
    uint8_t *rgb_data = calloc(led_count * 3, sizeof(uint8_t));

    if (led_custom_count > 0) {
        // 点亮指定的 LED
        for (int i = 0; i < led_custom_count; ++i) {
            if (led_custom[i].index < led_count) {
                rgb_data[led_custom[i].index * 3 + 0] = led_custom[i].g;  // G
                rgb_data[led_custom[i].index * 3 + 1] = led_custom[i].r;    // R
                rgb_data[led_custom[i].index * 3 + 2] = led_custom[i].b;   // B
            }
        }
    } else {
        // 点亮所有 LED
        for (int i = 0; i < led_count; ++i) {
            rgb_data[i * 3 + 0] = green;  // G
            rgb_data[i * 3 + 1] = red;    // R
            rgb_data[i * 3 + 2] = blue;   // B
        }
    }

    // 转换为 SPI 数据格式
    size_t spi_data_size = 2 * reset_len + led_count * 3 * 8;
    uint8_t *spi_data = calloc(2 * reset_len + spi_data_size, sizeof(uint8_t));
    ws281x_encode(rgb_data, spi_data + reset_len, led_count * 3);

    // 发送 SPI 数据
    ret = write(fd, spi_data, reset_len + spi_data_size + reset_len);
    if (ret < 0) {
        perror("write\n");
    }

    // 打印 spi_data 的十六进制数据，每16个字节为一组
    if (verbose_mode) {
        printf("%04d: ", 0);
        for (int i = 0; i < spi_data_size; i++) {
            printf("%02X ", spi_data[i]);
            if ((i + 1) % 16 == 0) {
                printf("\n%04d: ", (i + 1));
            }
        }
        printf("\n");
    }

    free(rgb_data);
    free(spi_data);
    close(fd);

    printf("Sent data to %d LEDs at %d Hz with color (R:%d, G:%d, B:%d), LED Custom: %d, reset_delay_us: %d\n",
           led_count, speed, red, green, blue, led_custom_count, reset_delay_us);
    return 0;
}
