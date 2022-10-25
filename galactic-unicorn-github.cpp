#include <charconv>
#include <cstdio>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/altcp_tls.h"

#include "tiny-json.h"

#include "http_client.hpp"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// contributionLevel for 5 levels, could use contributionCount for more detail
const char *contributionsQuery = R"(
query($login:String!, $startTime:DateTime) { 
    user(login: $login){
        contributionsCollection(from: $startTime) {
            contributionCalendar {
                weeks {
                    contributionDays {
                        contributionLevel
                    }
                }
            }
        }
    }
})";

static json_t json_mem[1024];

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

static void build_query_body(char *out, size_t out_len, const char *query, const char *variables = "{}")
{
    strcpy(out, "{\"query\": \"");
    out += 11;
    out_len += 11;

    // strip whitespace from query
    for(auto p = query; *p && out_len; p++)
    {
        if(*p == ' ' || *p == '\n')
        {
            if(*(out - 1) != ' ')
            {
                *out++ = ' ';
                out_len--;
            }

            continue;
        }
        
        *out++ = *p;
        out_len--;
    }

    snprintf(out, out_len, "\", \"variables\": %s}", variables);
}

static void parse_response_json(std::string &str)
{
    auto json = json_create(str.data(), json_mem, std::size(json_mem));

    if(!json)
    {
        printf("JSON parse failed!\n");
        return;
    }

    json = json_getProperty(json, "data");
    if(json)
        json = json_getProperty(json, "user");
    if(json)
        json = json_getProperty(json, "contributionsCollection");
    if(json)
        json = json_getProperty(json, "contributionCalendar");

    if(!json || json_getType(json) != JSON_OBJ)
    {
        printf("contributionsCalendar not found!\n");
        return;
    }

    auto weeks = json_getProperty(json, "weeks");

    if(!weeks || json_getType(weeks) != JSON_ARRAY)
    {
        printf("weeks array not found!\n");
        return;
    }

    int week_no = 0;
    for(auto week = json_getChild(weeks); week; week = json_getSibling(week), week_no++)
    {
        auto days = json_getProperty(week, "contributionDays");
        if(!days || json_getType(days) != JSON_ARRAY)
            continue;

        int day_no = 0;
        for(auto day = json_getChild(days); day; day = json_getSibling(day), day_no++)
        {
            auto level = json_getPropertyValue(day, "contributionLevel");

            printf("W %i D %i %s\n", week_no, day_no, level);
        }
    }
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

    std::string response_data;
    unsigned int response_len = 0;

    client.setOnStatus([](int code, std::string_view message)
    {
        printf("Status code: %i, message: %.*s\n", code, message.length(), message.data());
    });

    client.setOnHeader([&response_data, &response_len](std::string_view name, std::string_view value)
    {
        printf("Header: %.*s, value: %.*s\n", name.length(), name.data(), value.length(), value.data());

        if(name == "Content-Length") // TODO: case
        {
            // TODO: check errors
            std::from_chars(value.data(), value.data() + value.length(), response_len);
            response_data.reserve(response_len);
        }
    });

    client.setOnBodyData([&response_data, &response_len](unsigned int len, uint8_t *data)
    {
        response_data += std::string_view(reinterpret_cast<char *>(data), len);

        if(response_data.length() == response_len)
            parse_response_json(response_data);
    });

    // github API request
    char body[512];

    const char *variables = R"({"login" : "Daft-Freak"})";
    build_query_body(body, sizeof(body), contributionsQuery, variables);

    printf("Request body %s\n", body);

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
