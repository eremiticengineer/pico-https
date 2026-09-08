#include "HttpsClient.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <utility>

#include "pico/cyw43_arch.h"
#include "pico/error.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "mbedtls/error.h"

namespace {

constexpr size_t RECEIVE_BUFFER_SIZE = 1024;

}

HttpsClient::HttpsClient(
    std::string host,
    std::string path,
    const uint8_t* caCertificate,
    size_t caCertificateLength,
    std::string apiKey,
    uint16_t port
)
    : host_(std::move(host)),
      path_(std::move(path)),
      caCertificate_(caCertificate),
      caCertificateLength_(caCertificateLength),
      apiKey_(std::move(apiKey)),
      port_(port) {

    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&sslConfig_);
    mbedtls_x509_crt_init(&caCertificateChain_);
    mbedtls_ctr_drbg_init(&ctrDrbg_);
    mbedtls_entropy_init(&entropy_);
}

HttpsClient::~HttpsClient() {
    closeConnection();

    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&sslConfig_);
    mbedtls_x509_crt_free(&caCertificateChain_);
    mbedtls_ctr_drbg_free(&ctrDrbg_);
    mbedtls_entropy_free(&entropy_);
}

bool HttpsClient::initialiseWifi(
    const std::string& ssid,
    const std::string& password,
    uint32_t timeoutMs
) {
    errorMessage_.clear();

    if (!wifiInitialised_) {
        const int result = cyw43_arch_init();

        if (result != 0) {
            setError("cyw43_arch_init() failed", result);
            return false;
        }

        wifiInitialised_ = true;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to WiFi: %s\n", ssid.c_str());

    const int result = cyw43_arch_wifi_connect_timeout_ms(
        ssid.c_str(),
        password.c_str(),
        CYW43_AUTH_WPA2_AES_PSK,
        timeoutMs
    );

    if (result != 0) {
        if (result == PICO_ERROR_TIMEOUT) {
            errorMessage_ = "WiFi connection timed out";
        } else if (result == PICO_ERROR_BADAUTH) {
            errorMessage_ = "WiFi authentication failed";
        } else if (result == PICO_ERROR_CONNECT_FAILED) {
            errorMessage_ = "WiFi connection failed";
        } else {
            errorMessage_ =
                "WiFi connection failed: " +
                std::to_string(result);
        }

        return false;
    }

    printf("WiFi connected\n");

    return true;
}

bool HttpsClient::initialiseTls() {
    if (tlsInitialised_) {
        return true;
    }

    static constexpr char personalisation[] = "pico2w_https_client";

    int result = mbedtls_ctr_drbg_seed(
        &ctrDrbg_,
        mbedtls_entropy_func,
        &entropy_,
        reinterpret_cast<const unsigned char*>(personalisation),
        sizeof(personalisation) - 1
    );

    if (result != 0) {
        setError("mbedtls_ctr_drbg_seed() failed", result);
        return false;
    }

    result = mbedtls_x509_crt_parse(
        &caCertificateChain_,
        caCertificate_,
        caCertificateLength_
    );

    if (result < 0) {
        setError("Could not parse CA certificate", result);
        return false;
    }

    result = mbedtls_ssl_config_defaults(
        &sslConfig_,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT
    );

    mbedtls_ssl_conf_min_tls_version(
        &sslConfig_,
        MBEDTLS_SSL_VERSION_TLS1_2
    );

    mbedtls_ssl_conf_max_tls_version(
        &sslConfig_,
        MBEDTLS_SSL_VERSION_TLS1_2
    );

    if (result != 0) {
        setError("mbedtls_ssl_config_defaults() failed", result);
        return false;
    }

    /*
     * This is the important certificate verification setting.
     * The connection fails if the server certificate cannot be
     * validated against caCertificateChain_.
     */
    mbedtls_ssl_conf_authmode(
        &sslConfig_,
        MBEDTLS_SSL_VERIFY_REQUIRED
    );

    mbedtls_ssl_conf_ca_chain(
        &sslConfig_,
        &caCertificateChain_,
        nullptr
    );

    mbedtls_ssl_conf_rng(
        &sslConfig_,
        mbedtls_ctr_drbg_random,
        &ctrDrbg_
    );

    tlsInitialised_ = true;

    return true;
}

bool HttpsClient::connectSocket(uint32_t timeoutMs) {
    struct addrinfo hints {};
    struct addrinfo* addressList = nullptr;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const std::string port = std::to_string(port_);

    const int result = getaddrinfo(
        host_.c_str(),
        port.c_str(),
        &hints,
        &addressList
    );

    if (result != 0 || addressList == nullptr) {
        setError("DNS lookup failed", result);
        return false;
    }

    for (struct addrinfo* address = addressList;
         address != nullptr;
         address = address->ai_next) {

        socket_ = lwip_socket(
            address->ai_family,
            address->ai_socktype,
            address->ai_protocol
        );

        if (socket_ < 0) {
            continue;
        }

        struct timeval timeout {};
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        lwip_setsockopt(
            socket_,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        );

        lwip_setsockopt(
            socket_,
            SOL_SOCKET,
            SO_SNDTIMEO,
            &timeout,
            sizeof(timeout)
        );

        if (lwip_connect(
                socket_,
                address->ai_addr,
                address->ai_addrlen
            ) == 0) {

            freeaddrinfo(addressList);

            return true;
        }

        lwip_close(socket_);
        socket_ = -1;
    }

    freeaddrinfo(addressList);

    setError("Could not connect to HTTPS server");

    return false;
}

bool HttpsClient::performTlsHandshake() {
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_init(&ssl_);

    int result = mbedtls_ssl_setup(
        &ssl_,
        &sslConfig_
    );

    if (result != 0) {
        setError("mbedtls_ssl_setup() failed", result);
        return false;
    }

    /*
     * Sets SNI and, importantly, the hostname that the server
     * certificate must match.
     */
    result = mbedtls_ssl_set_hostname(
        &ssl_,
        host_.c_str()
    );

    if (result != 0) {
        setError("mbedtls_ssl_set_hostname() failed", result);
        return false;
    }

    mbedtls_ssl_set_bio(
        &ssl_,
        &socket_,
        tlsSend,
        tlsRecv,
        nullptr
    );

    while ((result = mbedtls_ssl_handshake(&ssl_)) != 0) {
        if (result == MBEDTLS_ERR_SSL_WANT_READ ||
            result == MBEDTLS_ERR_SSL_WANT_WRITE) {

            continue;
        }

        setError("TLS handshake failed", result);

        return false;
    }

    const uint32_t verificationResult =
        mbedtls_ssl_get_verify_result(&ssl_);

    if (verificationResult != 0) {
        char verificationInfo[512] {};

        mbedtls_x509_crt_verify_info(
            verificationInfo,
            sizeof(verificationInfo),
            "",
            verificationResult
        );

        errorMessage_ =
            "TLS certificate verification failed: " +
            std::string(verificationInfo);

        return false;
    }

    return true;
}

bool HttpsClient::sendRequest(const std::string& json) {
    std::ostringstream request;

    request
        << "POST " << path_ << " HTTP/1.1\r\n"
        << "Host: " << host_ << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << json.size() << "\r\n"
        << "X-API-KEY: " << apiKey_ << "\r\n"
        << "Connection: close\r\n"
        << "User-Agent: Pico2W-WeatherStation/1.0\r\n"
        << "\r\n"
        << json;

    const std::string requestString = request.str();

    size_t totalSent = 0;

    while (totalSent < requestString.size()) {
        const int result = mbedtls_ssl_write(
            &ssl_,
            reinterpret_cast<const unsigned char*>(
                requestString.data() + totalSent
            ),
            requestString.size() - totalSent
        );

        if (result > 0) {
            totalSent += static_cast<size_t>(result);
            continue;
        }

        if (result == MBEDTLS_ERR_SSL_WANT_READ ||
            result == MBEDTLS_ERR_SSL_WANT_WRITE) {

            continue;
        }

        setError("HTTPS write failed", result);

        return false;
    }

    return true;
}

bool HttpsClient::receiveResponse() {
    response_.clear();

    unsigned char buffer[RECEIVE_BUFFER_SIZE];

    while (true) {
        const int result = mbedtls_ssl_read(
            &ssl_,
            buffer,
            sizeof(buffer)
        );

        if (result > 0) {
            response_.append(
                reinterpret_cast<const char*>(buffer),
                static_cast<size_t>(result)
            );

            continue;
        }

        if (result == 0 ||
            result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {

            break;
        }

        if (result == MBEDTLS_ERR_SSL_WANT_READ ||
            result == MBEDTLS_ERR_SSL_WANT_WRITE) {

            continue;
        }

        setError("HTTPS read failed", result);

        return false;
    }

    return true;
}

bool HttpsClient::post(
    const std::string& json,
    uint32_t timeoutMs
) {
    statusCode_ = 0;

    response_.clear();
    responseBody_.clear();
    errorMessage_.clear();

    if (!wifiInitialised_) {
        errorMessage_ = "WiFi has not been initialised";
        return false;
    }

    if (!initialiseTls()) {
        return false;
    }

    closeConnection();

    if (!connectSocket(timeoutMs)) {
        closeConnection();
        return false;
    }

    if (!performTlsHandshake()) {
        closeConnection();
        return false;
    }

    if (!sendRequest(json)) {
        closeConnection();
        return false;
    }

    if (!receiveResponse()) {
        closeConnection();
        return false;
    }

    mbedtls_ssl_close_notify(&ssl_);

    closeConnection();

    parseHttpResponse();

    if (statusCode_ < 200 || statusCode_ >= 300) {
        errorMessage_ =
            "HTTP server returned status " +
            std::to_string(statusCode_);

        return false;
    }

    return true;
}

void HttpsClient::parseHttpResponse() {
    statusCode_ = 0;
    responseBody_.clear();

    const size_t statusLineEnd =
        response_.find("\r\n");

    if (statusLineEnd == std::string::npos) {
        errorMessage_ = "Invalid HTTP response";
        return;
    }

    const std::string statusLine =
        response_.substr(0, statusLineEnd);

    int status = 0;

    if (std::sscanf(
            statusLine.c_str(),
            "HTTP/%*s %d",
            &status
        ) == 1) {

        statusCode_ = status;
    }

    const size_t headerEnd =
        response_.find("\r\n\r\n");

    if (headerEnd != std::string::npos) {
        responseBody_ =
            response_.substr(headerEnd + 4);
    }
}

void HttpsClient::closeConnection() {
    if (socket_ >= 0) {
        lwip_shutdown(
            socket_,
            SHUT_RDWR
        );

        lwip_close(socket_);

        socket_ = -1;
    }
}

void HttpsClient::setError(
    const std::string& message,
    int errorCode
) {
    if (errorCode == 0) {
        errorMessage_ = message;
        return;
    }

    char mbedError[128] {};

    mbedtls_strerror(
        errorCode,
        mbedError,
        sizeof(mbedError)
    );

    errorMessage_ =
        message +
        " (" +
        std::to_string(errorCode) +
        "): " +
        mbedError;
}

int HttpsClient::tlsSend(
    void* context,
    const unsigned char* buffer,
    size_t length
) {
    auto* socket =
        static_cast<int*>(context);

    const int result = lwip_send(
        *socket,
        buffer,
        length,
        0
    );

    if (result >= 0) {
        return result;
    }

    if (errno == EWOULDBLOCK ||
        errno == EAGAIN) {

        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    if (errno == EPIPE ||
        errno == ECONNRESET) {

        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

int HttpsClient::tlsRecv(
    void* context,
    unsigned char* buffer,
    size_t length
) {
    auto* socket =
        static_cast<int*>(context);

    const int result = lwip_recv(
        *socket,
        buffer,
        length,
        0
    );

    if (result > 0) {
        return result;
    }

    if (result == 0) {
        return 0;
    }

    if (errno == EWOULDBLOCK ||
        errno == EAGAIN) {

        return MBEDTLS_ERR_SSL_WANT_READ;
    }

    if (errno == ECONNRESET) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

int HttpsClient::statusCode() const {
    return statusCode_;
}

const std::string& HttpsClient::response() const {
    return response_;
}

const std::string& HttpsClient::responseBody() const {
    return responseBody_;
}

const std::string& HttpsClient::errorMessage() const {
    return errorMessage_;
}