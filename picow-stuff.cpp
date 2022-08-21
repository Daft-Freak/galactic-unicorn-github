#include <cstdio>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "http_client.hpp"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

int main()
{
    stdio_init_all();

    if(cyw43_arch_init_with_country(CYW43_COUNTRY_UK))
    {
        printf("failed to initialise\n");
        return 1;
    }
    printf("initialised\n");

    cyw43_arch_enable_sta_mode();

    if(cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        printf("failed to connect\n");
        return 1;
    }
    printf("wifi connected\n");

    HTTPClient client("daft.games");

    client.setOnStatus([](int code, std::string_view message)
    {
        printf("Status code: %i, message: %.*s\n", code, message.length(), message.data());
    });

    client.setOnHeader([](std::string_view name, std::string_view value)
    {
        printf("Header: %.*s, value: %.*s\n", name.length(), name.data(), value.length(), value.data());
    });

    client.setOnBodyData([](unsigned int len, uint8_t *data)
    {
        printf("Body: %.*s\n", len, data);
    });


    client.get("/");
    
    while(true)
    {
        asm volatile ("wfe");
    }

    return 0;
}
