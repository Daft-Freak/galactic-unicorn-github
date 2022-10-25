#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "lwip/err.h"
#include "lwip/altcp.h"

class HTTPClient final
{
public:
    using StatusFunc = std::function<void(int, std::string_view)>;
    using HeaderFunc = std::function<void(std::string_view, std::string_view)>;
    using BodyFunc = std::function<void(unsigned int, uint8_t *)>;

    HTTPClient(const char *host, altcp_allocator_t *altcp_allocator = nullptr);

    bool get(const char *path, std::map<std::string_view, std::string_view> headers = {});
    bool post(const char *path, std::string_view body, std::map<std::string_view, std::string_view> headers = {});

    void setOnStatus(StatusFunc fun);
    void setOnHeader(HeaderFunc fun);
    void setOnBodyData(BodyFunc fun);

private:
    enum class ResponseState
    {
        Status = 0,
        Headers,
        Body
    };

    bool connect();
    err_t disconnect();

    bool do_request(const char *method, const char *path, const std::map<std::string_view, std::string_view> &headers);

    void on_dns_found(const char *name, const ip_addr_t *ipAddr);

    err_t on_connected(struct altcp_pcb *pcb, err_t err);
    err_t on_received(struct altcp_pcb *pcb, struct pbuf *buf, err_t err);
    err_t on_sent(struct altcp_pcb *pcb, u16_t len);
    void on_error(err_t err);

    // boing
    static void static_dns_found(const char *name, const ip_addr_t *ipAddr, void *arg);

    static err_t static_connected(void *arg, struct altcp_pcb *pcb, err_t err);
    static err_t static_received(void *arg, struct altcp_pcb *pcb, struct pbuf *buf, err_t err);
    static err_t static_sent(void *arg, struct altcp_pcb *pcb, u16_t len);
    static void static_error(void *arg, err_t err);

    const char *host;

    altcp_allocator_t *altcp_allocator;

    bool connected = false;

    StatusFunc onStatus;
    HeaderFunc onHeader;
    BodyFunc onBodyData;

    ResponseState res_state = ResponseState::Status;

    ip_addr_t remote_addr = {};
    altcp_pcb *pcb = nullptr;
    bool done_addr_lookup = false;

    std::string temp_header;
};