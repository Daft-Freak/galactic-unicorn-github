#include <charconv>
#include <cstdio>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/altcp_tls.h"

#include "galactic_unicorn.hpp"
#include "pico_graphics.hpp"

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

static int year = 2022;

static json_t json_mem[1024];

static altcp_allocator_t tls_allocator = {
    altcp_tls_alloc, nullptr
};

static HTTPClient client("api.github.com", &tls_allocator);

static std::string response_data;
static unsigned int response_len = 0;
static bool request_in_progress = false;

// unicorn/graphics
pimoroni::PicoGraphics_PenRGB888 graphics(53, 11, nullptr);
pimoroni::GalacticUnicorn galactic_unicorn;

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

    graphics.set_pen(0, 0, 0);
    graphics.clear();

    int week_no = 0;
    for(auto week = json_getChild(weeks); week; week = json_getSibling(week), week_no++)
    {
        auto days = json_getProperty(week, "contributionDays");
        if(!days || json_getType(days) != JSON_ARRAY)
            continue;

        int num_days = 0;
        for(auto day = json_getChild(days); day; day = json_getSibling(day))
            num_days++;

        int day_no = 0;

        // adjust first week
        if(week_no == 0 && num_days < 7)
            day_no = 7 - num_days;

        for(auto day = json_getChild(days); day; day = json_getSibling(day), day_no++)
        {
            std::string_view level = json_getPropertyValue(day, "contributionLevel");

            printf("W %i D %i %s\n", week_no, day_no, level.data());

            int index = 0;

            if(level == "FIRST_QUARTILE")
                graphics.set_pen(0x0E, 0x22, 0x14);
            else if(level == "SECOND_QUARTILE")
                graphics.set_pen(0x00, 0x6D, 0x32);
            else if(level == "THIRD_QUARTILE")
                graphics.set_pen(0x36, 0xB6, 0x51);
            else if(level == "FOURTH_QUARTILE")
                graphics.set_pen(0x45, 0xFF, 0x64);
            else
                continue;

            graphics.pixel({week_no, day_no});
        }
    }
}

static void make_http_request()
{
    if(request_in_progress)
        return;

    // tls
    // FIXME: cert
    if(!tls_allocator.arg)
        tls_allocator.arg  = altcp_tls_create_config_client(nullptr, 0);

    client.setOnStatus([](int code, std::string_view message)
    {
        printf("Status code: %i, message: %.*s\n", code, message.length(), message.data());
    });

    response_data.clear();
    response_len = 0;

    client.setOnHeader([](std::string_view name, std::string_view value)
    {
        printf("Header: %.*s, value: %.*s\n", name.length(), name.data(), value.length(), value.data());

        if(name == "Content-Length") // TODO: case
        {
            // TODO: check errors
            std::from_chars(value.data(), value.data() + value.length(), response_len);
            response_data.reserve(response_len);
        }
    });

    client.setOnBodyData([](unsigned int len, uint8_t *data)
    {
        response_data += std::string_view(reinterpret_cast<char *>(data), len);

        if(response_data.length() == response_len)
        {
            parse_response_json(response_data);
            request_in_progress = false;
        }
    });

    // github API request
    char body[512];
    char variables[128];

    snprintf(variables, sizeof(variables), R"({"login" : "Daft-Freak", "startTime": "%i-01-01T00:00:00"})", year);
    build_query_body(body, sizeof(body), contributionsQuery, variables);

    printf("Request body %s\n", body);

    bool ret = client.post("/graphql", body, {
        {"User-Agent", "PicoW"},
        {"Authorization", "bearer " GITHUB_TOKEN}
    });

    if(!ret)
        return;

    request_in_progress = true;
}

static void status_message(const char *message)
{
    graphics.set_pen(0);
    graphics.clear();
    graphics.set_font("bitmap8");

    graphics.set_pen(0xFF, 0xFF, 0xFF);
    graphics.text(message, {0, 2}, 100, 1.0f);

    galactic_unicorn.update(&graphics);
}

int main()
{
    stdio_init_all();

    galactic_unicorn.init();

    graphics.set_pen(0);
    graphics.clear();

    if(cyw43_arch_init_with_country(CYW43_COUNTRY_UK))
    {
        printf("failed to initialise\n");
        return 1;
    }
    printf("initialised\n");

    cyw43_arch_enable_sta_mode();

    status_message("Connecting");

    printf("ssid %s pass %s\n", ssid, password);

    if(cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        printf("failed to connect\n");

        status_message("Failed!");

        return 1;
    }
    printf("wifi connected\n");

    status_message("Connected.");

    make_http_request();

    while(true)
    {
        if(!request_in_progress && galactic_unicorn.is_pressed(pimoroni::GalacticUnicorn::SWITCH_A))
        {
            year++;
            make_http_request();
        }
        else if(!request_in_progress && galactic_unicorn.is_pressed(pimoroni::GalacticUnicorn::SWITCH_B))
        {
            year--;
            make_http_request();
        }
        galactic_unicorn.update(&graphics);
        sleep_ms(10);
    }

    return 0;
}
