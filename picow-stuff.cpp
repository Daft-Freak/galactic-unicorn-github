#include <cstdio>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/apps/http_client.h"

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

static err_t http_headers_done(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len)
{
    printf("Headers %i b, body %i b\n", hdr_len, content_len);

    printf("Headers:\n");
    for(auto buffer = hdr; buffer && hdr_len; buffer = buffer->next)
    {
        auto len = std::min(buffer->len, hdr_len);
        fwrite(buffer->payload, 1, len, stdout);
        hdr_len -= len;
    }

    return ERR_OK;
}

static err_t http_received(void *arg, struct altcp_pcb *pcb, struct pbuf *buf, err_t err)
{
    if(!buf || !buf->tot_len)
        return ERR_OK;

    for(auto buffer = buf; buffer; buffer = buffer->next)
    {
        fwrite(buffer->payload, 1, buffer->len, stdout);
    }

    altcp_recved(pcb, buf->tot_len);
    pbuf_free(buf);
    
    return ERR_OK;
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

    /*HTTPClient client("daft.games");

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


    client.get("/");*/

    httpc_state_t *http_state = nullptr;
    httpc_connection_t http_conn_settings = {};

    http_conn_settings.headers_done_fn = http_headers_done;

    httpc_get_file_dns("daft.games", 80, "/", &http_conn_settings, http_received, nullptr, &http_state);
    
    while(true)
    {
        asm volatile ("wfe");
    }

    return 0;
}
