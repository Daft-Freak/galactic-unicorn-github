#include <cstdio>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/altcp_tls.h"

#include "http_client.hpp"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// TODO: this isn't great and also probably should be somewhere else
extern "C"
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen )
{
    *olen = 0;
    for(size_t i = 0; i < len; i += 4)
    {
        auto w = pico_lwip_rand();
        *output++ = w;
        *output++ = w >> 8;
        *output++ = w >> 16;
        *output++ = w >> 24;
        *olen += 4;
    }

    return 0;
}

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

    // tls
    // FIXME: cert
    struct altcp_tls_config * conf = altcp_tls_create_config_client(nullptr, 0);
    altcp_allocator_t tls_allocator = {
        altcp_tls_alloc, conf
    };

    HTTPClient client("api.github.com", &tls_allocator);

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

    // github API request
    const char *body = R"({
    "query": "query { viewer { login }}"
})";

    client.post("/graphql", body, {
        {"User-Agent", "PicoW"},
        {"Authorization", "bearer " GITHUB_TOKEN}
    });

    while(true)
    {
        asm volatile ("wfe");
    }

    return 0;
}
