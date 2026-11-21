/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>

// Pure HTTP/WebSocket (RFC 6455) codec for the phone page server. No sockets, no Qt event loop.
namespace phone {

constexpr int kMaxHttpHead = 16 * 1024;
constexpr int kMaxWsPayload = 64 * 1024;

enum class ParseStatus { NeedMore, Ok, Error };

struct HttpRequest {
    QByteArray method;                  // always "GET" when parsing succeeded
    QByteArray path;                    // query string stripped, always starts with '/'
    QHash<QByteArray, QByteArray> headers;   // lower-case names, trimmed values
};

// Parses "request line + headers + blank line" from the start of buf. Ok => *consumed = head length.
ParseStatus parseHttpRequestHead(const QByteArray& buf, HttpRequest* out, int* consumed);
// True for a well-formed WebSocket upgrade (Upgrade, Connection, 16-byte Sec-WebSocket-Key, version 13).
bool isWebSocketUpgrade(const HttpRequest& req);
QByteArray websocketAcceptKey(const QByteArray& clientKey);

enum class WsOpcode { Text = 1, Close = 8, Ping = 9, Pong = 10 };

struct WsFrame {
    WsOpcode opcode = WsOpcode::Text;
    QByteArray payload;                 // unmasked
};

// Decodes ONE client frame from the start of buf (call again after removing *consumed bytes).
// Error: unmasked, fragmented/continuation/binary/reserved bits, or payload > kMaxWsPayload.
ParseStatus decodeWsFrame(const QByteArray& buf, WsFrame* out, int* consumed);

// Unmasked server frames.
QByteArray encodeWsFrame(WsOpcode opcode, const QByteArray& payload);
QByteArray encodeWsClose(quint16 code);

// Compares without an early exit on the first differing byte.
bool constantTimeEquals(const QByteArray& a, const QByteArray& b);

// Finite JSON number; a missing, non-numeric or non-finite value is rejected.
bool jsonNumber(const QJsonObject& msg, const char* key, double* out);
// Exact integer in [lo, hi]; anything else is rejected (no clamping).
bool jsonExactInt(const QJsonObject& msg, const char* key, int lo, int hi, int* out);

constexpr double kMaxScrollPx = 2000;

struct ScrollMessage {
    double dx = 0, dy = 0;      // pixels, clamped to +-kMaxScrollPx
    int phase = 0;              // 0, 1 began, 2 changed, 4 ended (macOS CGScrollPhase)
    int momentum = 0;           // 0, 1 begin, 2 continue, 3 end (CGMomentumScrollPhase)
};
// {"t":"sc","dx":px,"dy":px,"ph":phase,"mo":momentum}; ph/mo may be absent (= 0).
bool parseScroll(const QJsonObject& msg, ScrollMessage* out);

} // namespace phone
