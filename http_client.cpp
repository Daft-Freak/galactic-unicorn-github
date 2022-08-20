#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/dns.h"

#include "http_client.hpp"

HTTPClient::HTTPClient(const char *host) : host(host){}

bool HTTPClient::get(const char *path)
{
    if(!connect())
        return false;

    char buf[100];
    snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, host);

    cyw43_arch_lwip_begin();
    tcp_write(pcb, buf, strlen(buf), TCP_WRITE_FLAG_COPY);
    cyw43_arch_lwip_end();

    return true;
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

    pcb = tcp_new_ip_type(IP_GET_TYPE(&remote_addr));

    tcp_arg(pcb,this);
    tcp_recv(pcb, static_received);
    tcp_sent(pcb, static_sent);
    tcp_err(pcb, static_error);

    cyw43_arch_lwip_begin();
    err = tcp_connect(pcb, &remote_addr, 80, static_connected);
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
    if(pcb && tcp_close(pcb) != ERR_OK)
    {
        tcp_abort(pcb);
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

err_t HTTPClient::on_connected(struct tcp_pcb *pcb, err_t err)
{
    if(err != ERR_OK)
        return disconnect();

    connected = true;
    return ERR_OK;
}

err_t HTTPClient::on_received(struct tcp_pcb *pcb, struct pbuf *buf, err_t err)
{
    if(!buf)
        return disconnect();

    printf("recv %i\n", buf->tot_len);

    cyw43_arch_lwip_check();
    if(buf->tot_len)
    {
        // TODO: parse
        for(auto buffer = buf; buffer; buffer = buffer->next)
            fwrite(buffer->payload, 1, buffer->len, stdout);
        
        tcp_recved(pcb, buf->tot_len);
    }
    pbuf_free(buf);

    return ERR_OK;
}

err_t HTTPClient::on_sent(struct tcp_pcb *pcb, u16_t len)
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

err_t HTTPClient::static_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_connected(pcb, err);
}

err_t HTTPClient::static_received(void *arg, struct tcp_pcb *pcb, struct pbuf *buf, err_t err)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_received(pcb, buf, err);
}

err_t HTTPClient::static_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_sent(pcb, len);
}

void HTTPClient::static_error(void *arg, err_t err)
{
    auto that = reinterpret_cast<HTTPClient *>(arg);
    return that->on_error(err);
}