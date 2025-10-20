
#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include <stdarg.h>

// ===== CONFIG =====
#define DEBUG_ENABLED   1
#define DEBUG_SENSOR    0
#define DEBUG_DHT       0
#define DEBUG_AHT       0
#define DEBUG_MLX       0
#define DEBUG_DHT_ERROR      1
#define DEBUG_AHT_ERROR      1
#define DEBUG_MLX_ERROR      1

// ===== EMOJIS =====
#define EMOJI_DHT  "🌡️"
#define EMOJI_AHT  "💧"
#define EMOJI_MLX  "🔥"
#define EMOJI_INFO "ℹ️"
#define EMOJI_WARN "⚠️"
#define EMOJI_ERR  "❌"

// ===== BUFFER =====
#ifndef DEBUG_BUF_SZ
#define DEBUG_BUF_SZ 192
#endif

class Debug {
  private:
    // Formatea en 'out' usando un va_list ya iniciado
    void vformat_to(char* out, size_t out_sz, const char* fmt, va_list ap) {
      vsnprintf(out, out_sz, fmt, ap);
    }

    // Conveniencia: inicia va_list y llama a vformat_to
    void format_to(char* out, size_t out_sz, const char* fmt, ...) {
      va_list ap;
      va_start(ap, fmt);
      vformat_to(out, out_sz, fmt, ap);
      va_end(ap);
    }

    // Imprime una línea con prefijo y mensaje ya formateado
    void printLine(const char* emoji, const char* tag, const char* line) {
      if (tag && *tag) Serial.printf("%s [%s] %s\n", emoji, tag, line);
      else            Serial.printf("%s %s\n", emoji, line);
    }

  public:
    Debug() {}

    // --------- DHT ---------
    void dht(const char* msg) {
    #if DEBUG_ENABLED && DEBUG_DHT
      printLine(EMOJI_DHT, "DHT", msg);
    #endif
    }
    void dhtf(const char* fmt, ...) {
    #if DEBUG_ENABLED && DEBUG_DHT
      char buf[DEBUG_BUF_SZ];
      va_list ap; va_start(ap, fmt);
      vformat_to(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      printLine(EMOJI_DHT, "DHT", buf);
    #endif
    }

    // --------- AHT ---------
    void aht(const char* msg) {
    #if DEBUG_ENABLED && DEBUG_AHT
      printLine(EMOJI_AHT, "AHT", msg);
    #endif
    }

    void ahtf(const char* fmt, ...) {
    #if DEBUG_ENABLED && DEBUG_AHT
      char buf[DEBUG_BUF_SZ];
      va_list ap; va_start(ap, fmt);
      vformat_to(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      printLine(EMOJI_AHT, "AHT", buf);
    #endif
    }

    // --------- MLX ---------
    void mlx(const char* msg) {
    #if DEBUG_ENABLED && DEBUG_MLX
      printLine(EMOJI_MLX, "MLX", msg);
    #endif
    }
    void mlxf(const char* fmt, ...) {
    #if DEBUG_ENABLED && DEBUG_MLX
      char buf[DEBUG_BUF_SZ];
      va_list ap; va_start(ap, fmt);
      vformat_to(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      printLine(EMOJI_MLX, "MLX", buf);
    #endif
    }

    void infoSensor(const char* msg) {
    #if DEBUG_ENABLED && DEBUG_SENSOR
      printLine(EMOJI_INFO, "", msg);
    #endif
    }
    // --------- Genéricos ---------
    void info(const char* msg) {
    #if DEBUG_ENABLED
      printLine("", "", msg);
    #endif
    }
    void infof(const char* fmt, ...) {
    #if DEBUG_ENABLED
      char buf[DEBUG_BUF_SZ];
      va_list ap; va_start(ap, fmt);
      vformat_to(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      printLine(EMOJI_INFO, "", buf);
    #endif
    }

    void warn(const char* msg) {
    #if DEBUG_ENABLED
      printLine(EMOJI_WARN, "", msg);
    #endif
    }
    void warnf(const char* fmt, ...) {
    #if DEBUG_ENABLED
      char buf[DEBUG_BUF_SZ];
      va_list ap; va_start(ap, fmt);
      vformat_to(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      printLine(EMOJI_WARN, "", buf);
    #endif
    }

    void error(const char* msg) {
    #if DEBUG_ENABLED
      printLine(EMOJI_ERR, "", msg);
    #endif
    }
    void errorf(const char* fmt, ...) {
    #if DEBUG_ENABLED
      char buf[DEBUG_BUF_SZ];
      va_list ap; va_start(ap, fmt);
      vformat_to(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      printLine(EMOJI_ERR, "", buf);
    #endif
    }
};

#endif // DEBUG_H