#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

class HttpsClient {
public:
    HttpsClient(
        std::string host,
        std::string path,
        const uint8_t* caCertificate,
        size_t caCertificateLength,
        std::string apiKey,
        uint16_t port = 443
    );

    ~HttpsClient();

    HttpsClient(const HttpsClient&) = delete;
    HttpsClient& operator=(const HttpsClient&) = delete;

    bool initialiseWifi(
        const std::string& ssid,
        const std::string& password,
        uint32_t timeoutMs = 60000
    );

    bool post(
        const std::string& json,
        uint32_t timeoutMs = 15000
    );

    int statusCode() const;

    const std::string& response() const;

    const std::string& responseBody() const;

    const std::string& errorMessage() const;

private:
    bool initialiseTls();

    bool connectSocket(uint32_t timeoutMs);

    bool performTlsHandshake();

    bool sendRequest(const std::string& json);

    bool receiveResponse();

    void parseHttpResponse();

    void closeConnection();

    void setError(
        const std::string& message,
        int errorCode = 0
    );

    static int tlsSend(
        void* context,
        const unsigned char* buffer,
        size_t length
    );

    static int tlsRecv(
        void* context,
        unsigned char* buffer,
        size_t length
    );

    std::string host_;
    std::string path_;

    const uint8_t* caCertificate_;
    size_t caCertificateLength_;

    std::string apiKey_;

    uint16_t port_;

    int socket_ = -1;

    bool wifiInitialised_ = false;
    bool tlsInitialised_ = false;

    int statusCode_ = 0;

    std::string response_;
    std::string responseBody_;
    std::string errorMessage_;

    mbedtls_ssl_context ssl_;
    mbedtls_ssl_config sslConfig_;
    mbedtls_x509_crt caCertificateChain_;
    mbedtls_ctr_drbg_context ctrDrbg_;
    mbedtls_entropy_context entropy_;
};