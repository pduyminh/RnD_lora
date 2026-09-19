#ifndef PROTOCOL_H
#define PROTOCOL_H

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic bytes xác thực gói tin */
#define CTRL_PACKET_MAGIC       0xA5
#define TELEMETRY_PACKET_MAGIC  0x5A

/* Tham số thuật toán CRC-16/CCITT-FALSE */
#define CRC16_CCITT_FALSE_POLY  0x1021
#define CRC16_CCITT_FALSE_INIT  0xFFFF

/**
 * @brief Gói lệnh TX -> RX (12 bytes, packed)
 * Theo mục 4.1 trong knowledge.md
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic;         // = 0xA5, kiểm tra nhanh gói hợp lệ
    uint16_t seq;           // tăng dần mỗi gói, phát hiện mất gói/gói cũ
    int16_t  vx_mm_s;       // vận tốc trục X robot, mm/s
    int16_t  vy_mm_s;       // vận tốc trục Y robot, mm/s
    int16_t  omega_mrad_s;  // vận tốc góc, mrad/s (1000 = 1 rad/s)
    uint8_t  estop;         // 1 = dừng khẩn cấp ngay, bỏ qua mọi ramp
    uint16_t crc16;         // CRC-16/CCITT-FALSE tính trên các byte trước nó
} ctrl_packet_t;

/**
 * @brief Gói telemetry RX -> TX (7 bytes packed)
 * Định nghĩa đúng các trường và thứ tự theo mục 4.2 trong knowledge.md.
 * Lưu ý đặc tả: Tiêu đề mục 4.2 ghi "(8 bytes, packed)" nhưng tổng kích thước các trường
 * thành phần uint8(1) + uint16(2) + uint8(1) + uint8(1) + uint16(2) = 7 bytes.
 * Tuân thủ nghiêm ngặt ràng buộc plan.md: KHÔNG tự ý chèn trường padding/reserved.
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic;               // = 0x5A
    uint16_t seq_echo;            // seq của ctrl_packet_t gần nhất nhận được hợp lệ
    uint8_t  driver_fault_bitmap; // dự phòng cho tương lai, hiện luôn = 0 (không đọc ALM)
    uint8_t  link_ok;             // 1 nếu chưa vượt failsafe timeout
    uint16_t crc16;               // CRC-16/CCITT-FALSE tính trên các byte trước nó
} telemetry_packet_t;

/* Kiểm tra kích thước struct tại thời điểm biên dịch */
_Static_assert(sizeof(ctrl_packet_t) == 12, "ctrl_packet_t must be exactly 12 bytes");
_Static_assert(sizeof(telemetry_packet_t) == 7, "telemetry_packet_t must be exactly 7 bytes as specified in knowledge.md 4.2");

/* Chiều dài dữ liệu dùng để tính CRC (chỉ tính các byte đứng trước trường crc16) */
#define CTRL_PACKET_PAYLOAD_LEN      (offsetof(ctrl_packet_t, crc16))
#define TELEMETRY_PACKET_PAYLOAD_LEN (offsetof(telemetry_packet_t, crc16))

_Static_assert(CTRL_PACKET_PAYLOAD_LEN == 10, "CTRL_PACKET_PAYLOAD_LEN must be 10 bytes");
_Static_assert(TELEMETRY_PACKET_PAYLOAD_LEN == 5, "TELEMETRY_PACKET_PAYLOAD_LEN must be 5 bytes");

/**
 * @brief Tính CRC-16/CCITT-FALSE cho chuỗi byte dữ liệu.
 * Poly: 0x1021, Init: 0xFFFF, RefIn: false, RefOut: false, XorOut: 0x0000.
 * Test vector chuẩn: ASCII "123456789" -> 0x29B1.
 *
 * @param data Con trỏ vùng dữ liệu
 * @param len Độ dài vùng dữ liệu (bytes)
 * @return uint16_t Giá trị CRC-16
 */
static inline uint16_t crc16_ccitt_false(const uint8_t *data, size_t len)
{
    uint16_t crc = CRC16_CCITT_FALSE_INIT;
    if (data == NULL || len == 0) {
        return crc;
    }
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_FALSE_POLY);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/**
 * @brief Tiện ích tính CRC cho gói ctrl_packet_t (chỉ tính trên các byte trước crc16)
 */
static inline uint16_t ctrl_packet_calc_crc(const ctrl_packet_t *pkt)
{
    return crc16_ccitt_false((const uint8_t *)pkt, CTRL_PACKET_PAYLOAD_LEN);
}

/**
 * @brief Tiện ích tính CRC cho gói telemetry_packet_t (chỉ tính trên các byte trước crc16)
 */
static inline uint16_t telemetry_packet_calc_crc(const telemetry_packet_t *pkt)
{
    return crc16_ccitt_false((const uint8_t *)pkt, TELEMETRY_PACKET_PAYLOAD_LEN);
}

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
