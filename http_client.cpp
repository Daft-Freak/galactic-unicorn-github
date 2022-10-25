#include <charconv>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/dns.h"

#include "http_client.hpp"

HTTPClient::HTTPClient(const char *host, altcp_allocator_t *altcp_allocator) : host(host), altcp_allocator(altcp_allocator){}

bool HTTPClient::get(const char *path, std::map<std::string_view, std::string_view> headers)
{
    if(!connect())
        return false;

    char buf[1024];
    int off = snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\nHost: %s\r\n", path, host);

    // headers
    for(auto &header : headers)
        off += snprintf(buf + off, sizeof(buf) - off, "%.*s: %.*s\r\n", header.first.length(), header.first.data(), header.second.length(), header.second.data());

    if(off >= sizeof(buf) - 3)
        return false;

    buf[off++] = '\r';
    buf[off++] = '\n';
    buf[off] = 0;

    res_state = ResponseState::Status;

    cyw43_arch_lwip_begin();
    altcp_write(pcb, buf, strlen(buf), TCP_WRITE_FLAG_COPY);
    cyw43_arch_lwip_end();

    return true;
}

void HTTPClient::setOnStatus(StatusFunc fun)
{
    onStatus = fun;
}

void HTTPClient::setOnHeader(HeaderFunc fun)
{
    onHeader = fun;
}

void HTTPClient::setOnBodyData(BodyFunc fun)
{
    onBodyData = fun;
}

bool HTTPClient::connect()
{
    if(connected)
        return true;

    err_t err;

    // DNS lookup
    if(!done_addr_lookup)
    {
        err = dns_gethostbyname(host, &remote_addr, static_dns_found, this);

        if(err == ERR_OK)
            done_addr_lookup = true;
        else if(err == ERR_INPROGRESS)
        {
            // wait for it
            // TODO: avoid blocking?
            while(!done_addr_lookup)
                sleep_ms(1);
        }
    }

    pcb = altcp_new_ip_type(altcp_allocator, IP_GET_TYPE(&remote_addr));

    altcp_arg(pcb,this);
    altcp_recv(pcb, static_received);
    altcp_sent(pcb, static_sent);
    altcp_err(pcb, static_error);

    cyw43_arch_lwip_begin();

    // TODO: assuming allocator is TLS allocator
    bool is_tls = altcp_allocator != nullptr;
    err = altcp_connect(pcb, &remote_addr, is_tls ? 443 : 80, static_connected);

    cyw43_arch_lwip_end();

    if(err != ERR_OK)
    {
        printf("tcp_connect failed %i\n", err);
        return false;
    }

    // wait
    // TODO:?
    while(!connected && pcb)
        sleep_ms(1);

    if(!pcb)
    {
        printf("tcp_connect failed\n");
        return false;
    }

    return true;
}

err_t HTTPClient::disconnect()
{
    auto ret = ERR_OK;
    if(pcb && altcp_close(pcb) != ERR_OK)
    {
        altcp_abort(pcb);
        ret = ERR_ABRT;
    }
    
    pcb = nullptr;
    connected = false;

    return ret;
}

void HTTPClient::on_dns_found(const char *name, const ip_addr_t *ipAddr)
{
    remote_addr = *ipAddr;
    done_addr_lookup = true;
}

err_t HTTPClient::on_connected(struct altcp_pcb *pcb, err_t err)
{
    if(err != ERR_OK)
        return disconnect();

    connected = true;
    return ERR_OK;
}

err_t HTTPClient::on_received(struct altcp_pcb *pcb, struct pbuf *buf, err_t err)
{
    if(!buf)
        return disconnect();

    cyw43_arch_lwip_check();
    if(buf->tot_len)
    {
        // TODO: parse
        for(auto buffer = buf; buffer; buffer = buffer->next)
        {
            unsigned int off = 0;
            while(off < buffer->len)
            {
                std::string_view str(reinterpret_cast<char *>(buffer->payload) + off, buffer->len - off);
                switch(res_state)
                {
                    case ResponseState::Status:
                    {
                        auto end = str.find("\r\n");
                        // TODO: should handle not finding end
                        auto line = str.substr(0, end);

                        // extract code/message
                        if(onStatus)
                        {
                            int code = 0;
                            auto space = line.find_first_of(' ');
                            std::from_chars(line.data() + space + 1, line.data() + line.length(), code);

                            space = line.find_first_of(' ', space + 1);
                            auto message = line.substr(space + 1);

                            onStatus(code, message);
                        }

                        off += end + 2;
                        res_state = ResponseState::Headers;
                        break;
                    }

                    case ResponseState::Headers:
                    {
                        auto end = str.find("\r\n");

                        if(end == std::string_view::npos)
                        {
                            // need more data
                            temp_header += str;
                            off = buffer->len;
                            continue;
                        }

                        // get full header line
                        std::string_view line;
                        if(temp_header.empty())
                            line = str.substr(0, end);
                        else
                        {
                            temp_header += str.substr(0, end);
                            line = temp_header;
                        }

                        if(end == 0)
                            res_state = ResponseState::Body;
                        else if(onHeader)
                        {
                            auto colon = line.find_first_of(':');
                            auto name = line.substr(0, colon);
                            auto value = line.substr(colon + 1);
                            if(value[0] == ' ')
                                value.remove_prefix(1);

                            onHeader(name, value);

                            if(!temp_header.empty())
                                temp_header.clear();
                        }
                        off += end + 2;

                        break;
                    }

                    case ResponseState::Body:
                        if(onBodyData)
                            onBodyData(buffer->len - off, reinterpret_cast<uint8_t *>(buffer->payload) + off);

                        off = buffer->len;
                }
            }
        }
        
        altcp_recved(pcb, buf->tot_len);
    }
    pbuf_free(buf);

    return ERR_OK;
}

err_t HTTPClient::on_sent(struct altcp_pcb *pcb, u16_t len)
{
    // TODO: something
    printf("sent %i\n", len);
    return ERR_OK;
}

void HTTPClient::on_error(err_t err)
{
    if(err != ERR_ABRT)
        disconnect();
}

void HTTPClient::static_dns_found(const char *name, const ip_addr_t *ipAddr, void *arg)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    that->on_dns_found(name, ipAddr);
}

err_t HTTPClient::static_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_connected(pcb, err);
}

err_t HTTPClient::static_received(void *arg, struct altcp_pcb *pcb, struct pbuf *buf, err_t err)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_received(pcb, buf, err);
}

err_t HTTPClient::static_sent(void *arg, struct altcp_pcb *pcb, u16_t len)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_sent(pcb, len);
}

void HTTPClient::static_error(void *arg, err_t err)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_error(err);
}