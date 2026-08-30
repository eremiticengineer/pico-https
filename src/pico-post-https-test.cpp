#include <stdio.h>
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "HttpsJsonClient.hpp"
#include "WeatherPayload.hpp"
#include "WebServerCertificate.hpp"

#include "secrets.hpp"

#define HTTPS_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

void https_post_task(__unused void *params) {
    HttpsJsonClient* pHttpsClient = static_cast<HttpsJsonClient*>(params);

    std::string payload =
        "seq=1 temp=12.4 humidity=76.2 pressure=1008.6 "
        "wind=8.7 gust=14.2 direction=23 rain=1.4 "
        "lux=12500.0 battery=4.87 timestamp=1788004800";

    WeatherPayload weather;
    if (!WeatherPayload::parse(payload, weather)) {
        printf("Invalid weather packet: %s\n", payload.c_str());
    }
    const std::string json = weather.toJson();
    printf("JSON: %s\n", json.c_str());

    if (!pHttpsClient->initialiseWifi(WIFI_SSID, WIFI_PASSWORD)) {
        printf("WiFi failed: %s\n", pHttpsClient->errorMessage().c_str());
    }

    while (true) {
        printf("posting to website...\n");

        if (!pHttpsClient->post(json)) {
            printf("POST failed: %s\n", pHttpsClient->errorMessage().c_str());
        } else {
            printf("POST successful: HTTP %d\n", pHttpsClient->statusCode());
        }    

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

int main( void )
{
    stdio_init_all();

    for (int c=0; c<5; c++) {
        printf("waiting\n");
        sleep_ms(1000);
    }

    HttpsJsonClient httpsClient(
        SERVER,
        SERVER_PATH,
        WEB_SERVER_ROOT_CA,
        sizeof(WEB_SERVER_ROOT_CA),
        API_KEY
    );

    BaseType_t result = xTaskCreate(https_post_task, "HttpsPostTask", 1024, (void*)&httpsClient, HTTPS_TASK_PRIORITY, nullptr);

    vTaskStartScheduler();

    while (true) { tight_loop_contents(); }
}
