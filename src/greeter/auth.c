#define _GNU_SOURCE
#include "auth.h"
#include <errno.h>
#include <json-c/json.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
static bool failure(struct login_auth *auth, const char *message) {
    snprintf(auth->message, sizeof(auth->message), "%s", message);
    auth->state = LOGIN_FAILED; return false;
}
static bool send_request(struct login_auth *auth, struct json_object *request) {
    if (!request || auth->tx_size) { if (request) { json_object_put(request); } return failure(auth, "Serviço de login ocupado."); }
    const char *json = json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN);
    size_t length = strlen(json);
    bool valid = length <= sizeof(auth->tx) - 4;
    if (valid) {
        uint32_t prefix = (uint32_t)length;
        memcpy(auth->tx, &prefix, 4); memcpy(auth->tx + 4, json, length);
        auth->tx_size = length + 4; auth->tx_sent = 0;
    }
    /* JSON serialization and the response object both contain the credential. */
    explicit_bzero((char *)json, length);
    struct json_object *response;
    if (json_object_object_get_ex(request, "response", &response) && json_object_is_type(response, json_type_string)) {
        explicit_bzero((char *)json_object_get_string(response), (size_t)json_object_get_string_len(response));
    }
    json_object_put(request);
    return valid || failure(auth, "Resposta excede o limite do login.");
}
static struct json_object *request(const char *type) {
    struct json_object *object = json_object_new_object();
    if (object) { json_object_object_add(object, "type", json_object_new_string(type)); }
    return object;
}
bool login_auth_open(struct login_auth *auth, const char *path) {
    memset(auth, 0, sizeof(*auth)); auth->fd = -1;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    if (!path || !*path || strlen(path) >= sizeof(address.sun_path)) { return failure(auth, "Serviço de login indisponível."); }
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", path);
    auth->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (auth->fd < 0 || (connect(auth->fd, (struct sockaddr *)&address, sizeof(address)) < 0 && errno != EINPROGRESS)) {
        login_auth_close(auth); return failure(auth, "Não foi possível conectar ao login.");
    }
    auth->state = LOGIN_READY; return true;
}
bool login_auth_create(struct login_auth *auth, const char *username) {
    if (auth->state != LOGIN_READY || !username || !*username || strlen(username) > 255) { return false; }
    struct json_object *object = request("create_session");
    if (!object) { return failure(auth, "Memória insuficiente."); }
    json_object_object_add(object, "username", json_object_new_string(username));
    auth->state = LOGIN_WAIT; return send_request(auth, object);
}
bool login_auth_reply(struct login_auth *auth, char *response) {
    if (auth->state != LOGIN_PROMPT) { if (response) { explicit_bzero(response, strlen(response)); } return false; }
    struct json_object *object = request("post_auth_message_response");
    if (!object) { if (response) { explicit_bzero(response, strlen(response)); } return failure(auth, "Memória insuficiente."); }
    json_object_object_add(object, "response", response ? json_object_new_string(response) : NULL);
    if (response) { explicit_bzero(response, strlen(response)); }
    auth->state = LOGIN_WAIT; return send_request(auth, object);
}
bool login_auth_cancel(struct login_auth *auth) {
    if (auth->tx_size) { return false; }
    auth->state = LOGIN_FAILED;
    return send_request(auth, request("cancel_session"));
}
static const char *field(struct json_object *object, const char *name) {
    struct json_object *value;
    if (!json_object_object_get_ex(object, name, &value) || !json_object_is_type(value, json_type_string)) { return NULL; }
    const char *text = json_object_get_string(value);
    return strlen(text) == (size_t)json_object_get_string_len(value) ? text : NULL;
}
static bool receive_message(struct login_auth *auth, struct json_object *object) {
    const char *type = field(object, "type");
    if (!type) { return failure(auth, "Resposta inválida do serviço de login."); }
    if (!strcmp(type, "auth_message")) {
        const char *kind = field(object, "auth_message_type"), *message = field(object, "auth_message");
        if (!kind || !message || auth->state != LOGIN_WAIT) { return failure(auth, "Pedido de autenticação inválido."); }
        snprintf(auth->message, sizeof(auth->message), "%s", message);
        auth->secret = !strcmp(kind, "secret"); auth->state = LOGIN_PROMPT;
        if (!strcmp(kind, "info") || !strcmp(kind, "error")) { return login_auth_reply(auth, NULL); }
        if (strcmp(kind, "secret") && strcmp(kind, "visible")) { return failure(auth, "Método de autenticação não suportado."); }
        return true;
    }
    if (!strcmp(type, "error")) {
        snprintf(auth->message, sizeof(auth->message), "Não foi possível entrar. Confira suas credenciais.");
        auth->state = LOGIN_FAILED;
        return send_request(auth, request("cancel_session"));
    }
    if (strcmp(type, "success")) { return failure(auth, "Resposta desconhecida do serviço de login."); }
    if (auth->state == LOGIN_FAILED) { auth->state = LOGIN_READY; return true; }
    if (auth->state == LOGIN_STARTING) { auth->state = LOGIN_DONE; return true; }
    if (auth->state != LOGIN_WAIT) { return failure(auth, "Sequência de autenticação inválida."); }
    struct json_object *start = request("start_session"), *cmd = json_object_new_array(), *env = json_object_new_array();
    if (!start || !cmd || !env) {
        if (start) { json_object_put(start); } if (cmd) { json_object_put(cmd); } if (env) { json_object_put(env); }
        return failure(auth, "Memória insuficiente.");
    }
    const char *args[] = {"/usr/bin/dbus-run-session", "/usr/bin/jlinux-session", "--drm", "--no-terminal"};
    const char *vars[] = {"XDG_CURRENT_DESKTOP=JLinux", "XDG_SESSION_DESKTOP=jlinux", "XDG_SESSION_TYPE=wayland"};
    for (size_t i = 0; i < sizeof(args)/sizeof(args[0]); i++) {
        json_object_array_add(cmd, json_object_new_string(args[i]));
    }
    for (size_t i = 0; i < sizeof(vars)/sizeof(vars[0]); i++) {
        json_object_array_add(env, json_object_new_string(vars[i]));
    }
    json_object_object_add(start, "cmd", cmd); json_object_object_add(start, "env", env);
    auth->state = LOGIN_STARTING; return send_request(auth, start);
}
bool login_auth_dispatch(struct login_auth *auth, short events) {
    if (events & (POLLERR | POLLNVAL)) { return failure(auth, "Serviço de login desconectado."); }
    if (!auth->connected && events & POLLOUT) {
        int error = 0; socklen_t size = sizeof(error);
        if (getsockopt(auth->fd, SOL_SOCKET, SO_ERROR, &error, &size) || error) { return failure(auth, "Conexão de login falhou."); }
        auth->connected = true;
    }
    if (events & POLLOUT && auth->tx_size) {
        ssize_t count = send(auth->fd, auth->tx + auth->tx_sent, auth->tx_size - auth->tx_sent, MSG_NOSIGNAL);
        if (count < 0 && errno != EAGAIN && errno != EINTR) { return failure(auth, "Não foi possível enviar ao login."); }
        if (count > 0) {
            auth->tx_sent += (size_t)count;
            if (auth->tx_sent == auth->tx_size) { explicit_bzero(auth->tx, sizeof(auth->tx)); auth->tx_size = auth->tx_sent = 0; }
        }
    }
    if (events & POLLIN) {
        if (auth->rx_size == sizeof(auth->rx)) { return failure(auth, "Mensagem de login excessiva."); }
        ssize_t count = recv(auth->fd, auth->rx + auth->rx_size, sizeof(auth->rx) - auth->rx_size, 0);
        if (!count || (count < 0 && errno != EAGAIN && errno != EINTR)) { return failure(auth, "Serviço de login desconectado."); }
        if (count > 0) { auth->rx_size += (size_t)count; }
        if (auth->rx_size >= 4) {
            uint32_t length; memcpy(&length, auth->rx, 4);
            if (!length || length > sizeof(auth->rx) - 4) { return failure(auth, "Tamanho de mensagem de login inválido."); }
            if (auth->rx_size > length + 4) { return failure(auth, "Quadro de login inesperado."); }
            if (auth->rx_size == length + 4) {
                struct json_tokener *parser = json_tokener_new();
                if (!parser) { return failure(auth, "Memória insuficiente."); }
                struct json_object *object = json_tokener_parse_ex(parser, (const char *)auth->rx + 4, (int)length);
                bool valid = json_tokener_get_error(parser) == json_tokener_success &&
                    json_object_is_type(object, json_type_object) && json_tokener_get_parse_end(parser) == length;
                json_tokener_free(parser);
                if (valid) { valid = receive_message(auth, object); }
                if (object) { json_object_put(object); }
                explicit_bzero(auth->rx, sizeof(auth->rx)); auth->rx_size = 0;
                if (!valid) { return failure(auth, "Resposta inválida do serviço de login."); }
            }
        }
    }
    if (events & POLLHUP && auth->state != LOGIN_DONE) { return failure(auth, "Serviço de login desconectado."); }
    return true;
}
void login_auth_close(struct login_auth *auth) {
    if (auth->fd >= 0) { close(auth->fd); }
    explicit_bzero(auth->tx, sizeof(auth->tx)); explicit_bzero(auth->rx, sizeof(auth->rx));
    auth->fd = -1; auth->tx_size = auth->tx_sent = auth->rx_size = 0;
}
