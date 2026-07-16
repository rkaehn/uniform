#include "bd_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct cr_str* bd_cert_dir(struct cr_mem* mem) {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != NULL && xdg[0] == '/') {
        return cr_str_create_format(mem, "%s/uniform", xdg);
    }
    const char* home = getenv("HOME");
    if (home == NULL || home[0] != '/') {
        bd_fail("HOME is not set, the certificate goes into $HOME/.config/uniform");
    }
    return cr_str_create_format(mem, "%s/.config/uniform", home);
}

struct cr_str* bd_cert_host(struct cr_mem* mem) {
    struct cr_str* host = cr_str_create_empty(mem);
    if (bd_exec(bd_args_create(mem, "scutil", "--get", "LocalHostName", NULL), host) != 0 || cr_str_get_count(host) < 2) {
        cr_str_destroy(host);
        return NULL;
    }
    cr_str_remove_last(host, 1);
    char* raw = cr_str_get_raw(host);
    for (uint32_t i = 0, i_end = cr_str_get_count(host); i < i_end; i++) {
        if (raw[i] >= 'A' && raw[i] <= 'Z') {
            raw[i] = (char)(raw[i] - 'A' + 'a');
        }
    }
    cr_str_append_cstr(host, ".local");
    return host;
}

static struct cr_str* bd_cert_file(struct cr_mem* mem, struct cr_str* dir, const char* name) {
    struct cr_str* path = cr_str_create_copy(mem, dir);
    cr_str_append_path_component_cstr(path, name);
    return path;
}

static struct cr_str* bd_cert_common_name(struct cr_mem* mem, struct cr_str* host) {
    if (host == NULL) {
        return cr_str_create(mem, "uniform");
    }
    struct cr_str* name = cr_str_create_copy(mem, host);
    cr_str_remove_path_extension(name);
    cr_str_insert_cstr(name, 0, "uniform (");
    cr_str_append_cstr(name, ")");
    return name;
}

static struct cr_str* bd_cert_profile_xml(struct cr_mem* mem, struct cr_str* certificate, struct cr_str* host, struct cr_str* common_name) {
    uint8_t hash[32];
    struct cr_sha256 sha256;
    cr_sha256_init(&sha256);
    cr_sha256_update(&sha256, cr_str_get_data(certificate), cr_str_get_count(certificate));
    cr_sha256_final(&sha256, hash);
    char uuids[2][40];
    for (uint32_t u = 0; u < 2; u++) {
        const uint8_t* h = hash + u * 16;
        snprintf(uuids[u], sizeof(uuids[u]), "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9], h[10], h[11], h[12], h[13], h[14], h[15]);
    }

    struct cr_str* base64 = cr_str_create_empty(mem);
    for (const char* line = cr_str_get_data(certificate); *line != 0;) {
        const char* end = strchr(line, '\n');
        uint32_t size = end != NULL ? (uint32_t)(end - line) : (uint32_t)strlen(line);
        if (size > 0 && line[0] != '-') {
            cr_str_append_cstr(base64, "\t\t\t");
            cr_str_append_data(base64, line, size);
            cr_str_append_cstr(base64, "\n");
        }
        line = end != NULL ? end + 1 : line + size;
    }

    struct cr_str* identifier = cr_str_create(mem, "com.raffaelkaehn.uniform.certificate");
    if (host != NULL) {
        struct cr_str* name = cr_str_create_copy(mem, host);
        cr_str_remove_path_extension(name);
        cr_str_append_format(identifier, ".%s", cr_str_get_data(name));
        cr_str_destroy(name);
    }
    const char* place = host != NULL ? cr_str_get_data(host) : "this Mac";

    struct cr_str* xml = cr_str_create_format(mem,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "\t<key>PayloadContent</key>\n"
        "\t<array>\n"
        "\t\t<dict>\n"
        "\t\t\t<key>PayloadCertificateFileName</key>\n"
        "\t\t\t<string>certificate.pem</string>\n"
        "\t\t\t<key>PayloadContent</key>\n"
        "\t\t\t<data>\n"
        "%s"
        "\t\t\t</data>\n"
        "\t\t\t<key>PayloadDescription</key>\n"
        "\t\t\t<string>Trusts the uniform development server on %s.</string>\n"
        "\t\t\t<key>PayloadDisplayName</key>\n"
        "\t\t\t<string>%s</string>\n"
        "\t\t\t<key>PayloadIdentifier</key>\n"
        "\t\t\t<string>%s.root</string>\n"
        "\t\t\t<key>PayloadType</key>\n"
        "\t\t\t<string>com.apple.security.root</string>\n"
        "\t\t\t<key>PayloadUUID</key>\n"
        "\t\t\t<string>%s</string>\n"
        "\t\t\t<key>PayloadVersion</key>\n"
        "\t\t\t<integer>1</integer>\n"
        "\t\t</dict>\n"
        "\t</array>\n"
        "\t<key>PayloadDescription</key>\n"
        "\t<string>Trusts the uniform development server on %s.</string>\n"
        "\t<key>PayloadDisplayName</key>\n"
        "\t<string>%s</string>\n"
        "\t<key>PayloadIdentifier</key>\n"
        "\t<string>%s</string>\n"
        "\t<key>PayloadRemovalDisallowed</key>\n"
        "\t<false/>\n"
        "\t<key>PayloadType</key>\n"
        "\t<string>Configuration</string>\n"
        "\t<key>PayloadUUID</key>\n"
        "\t<string>%s</string>\n"
        "\t<key>PayloadVersion</key>\n"
        "\t<integer>1</integer>\n"
        "</dict>\n"
        "</plist>\n",
        cr_str_get_data(base64), place, cr_str_get_data(common_name), cr_str_get_data(identifier), uuids[0],
        place, cr_str_get_data(common_name), cr_str_get_data(identifier), uuids[1]);

    cr_str_destroy(identifier);
    cr_str_destroy(base64);
    return xml;
}

int bd_cert_create(struct cr_mem* mem) {
    struct cr_str* dir = bd_cert_dir(mem);
    if (cr_util_create_dir(dir) == false) {
        bd_fail("cannot create %s", cr_str_get_data(dir));
    }
    struct cr_str* certificate_path = bd_cert_file(mem, dir, "certificate.pem");
    struct cr_str* private_key_path = bd_cert_file(mem, dir, "private_key.pem");
    struct cr_str* profile_path = bd_cert_file(mem, dir, "certificate.mobileconfig");
    struct cr_str* xml_path = bd_cert_file(mem, dir, "certificate.mobileconfig.xml");
    struct cr_str* host = bd_cert_host(mem);
    struct cr_str* common_name = bd_cert_common_name(mem, host);
    struct cr_str* subject = cr_str_create_format(mem, "/CN=%s", cr_str_get_data(common_name));
    struct cr_str* names = cr_str_create(mem, "subjectAltName=DNS:localhost");
    if (host != NULL) {
        cr_str_append_format(names, ",DNS:%s", cr_str_get_data(host));
    }
    cr_str_append_cstr(names, ",IP:127.0.0.1,IP:::1");

    struct cr_str* output = cr_str_create_empty(mem);
    int code = bd_exec(bd_args_create(mem, "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "820", "-sha256",
        "-subj", cr_str_get_data(subject),
        "-addext", "basicConstraints=critical,CA:TRUE",
        "-addext", "keyUsage=critical,digitalSignature,keyEncipherment,keyCertSign",
        "-addext", "extendedKeyUsage=serverAuth",
        "-addext", cr_str_get_data(names),
        "-addext", "subjectKeyIdentifier=hash",
        "-keyout", cr_str_get_data(private_key_path),
        "-out", cr_str_get_data(certificate_path), NULL), output);
    if (code != 0) {
        bd_fail("cannot create a certificate with openssl\n%s", cr_str_get_data(output));
    }
    chmod(cr_str_get_data(private_key_path), 0600);

    struct cr_str* certificate = cr_str_create_with_contents_of_file(mem, certificate_path);
    if (certificate == NULL) {
        bd_fail("cannot read %s", cr_str_get_data(certificate_path));
    }
    struct cr_str* xml = bd_cert_profile_xml(mem, certificate, host, common_name);
    if (cr_str_write_to_file(xml, xml_path) == false) {
        bd_fail("cannot write %s", cr_str_get_data(xml_path));
    }
    cr_str_remove_all(output);
    code = bd_exec(bd_args_create(mem, "openssl", "smime", "-sign", "-in", cr_str_get_data(xml_path), "-signer", cr_str_get_data(certificate_path), "-inkey", cr_str_get_data(private_key_path), "-outform", "DER", "-nodetach", "-binary", "-out", cr_str_get_data(profile_path), NULL), output);
    cr_util_remove(xml_path);
    if (code != 0) {
        bd_fail("cannot sign the certificate profile with openssl\n%s", cr_str_get_data(output));
    }
    printf("[Created %s]\n", cr_str_get_data(certificate_path));

    if (bd_exec_inherit(bd_args_create(mem, "security", "add-trusted-cert", "-r", "trustRoot", cr_str_get_data(certificate_path), NULL)) == 0) {
        printf("[Trusted on this Mac]\n");
    } else {
        printf("[Not trusted on this Mac, browsers warn until it is: security add-trusted-cert -r trustRoot %s]\n", cr_str_get_data(certificate_path));
    }

    printf("[To trust it on an iPhone or iPad, run uniform --certificate --device]\n");

    cr_str_destroy(xml);
    cr_str_destroy(certificate);
    cr_str_destroy(output);
    cr_str_destroy(names);
    cr_str_destroy(subject);
    cr_str_destroy(common_name);
    if (host != NULL) {
        cr_str_destroy(host);
    }
    cr_str_destroy(xml_path);
    cr_str_destroy(profile_path);
    cr_str_destroy(private_key_path);
    cr_str_destroy(certificate_path);
    cr_str_destroy(dir);
    return 0;
}

int bd_cert_install(struct cr_mem* mem, const char* device) {
    struct cr_str* dir = bd_cert_dir(mem);
    struct cr_str* profile_path = bd_cert_file(mem, dir, "certificate.mobileconfig");
    if (cr_util_does_file_exist(profile_path) == false) {
        bd_fail("there is no certificate in %s, create one with uniform --certificate", cr_str_get_data(dir));
    }
    struct cr_str* identifier = bd_device_resolve(mem, device);
    int code = bd_exec_inherit(bd_args_create(mem, "xcrun", "devicectl", "device", "profile", "install", "--device", cr_str_get_data(identifier), cr_str_get_data(profile_path), NULL));
    if (code == 0) {
        printf("[Sent the profile to %s, install it in its Settings, then enable it under General > About > Certificate Trust Settings]\n", cr_str_get_data(identifier));
    } else {
        printf("[Could not send the profile to %s]\n", cr_str_get_data(identifier));
    }
    cr_str_destroy(identifier);
    cr_str_destroy(profile_path);
    cr_str_destroy(dir);
    return code == 0 ? 0 : 1;
}

struct bd_cert* bd_cert_load(struct cr_mem* mem) {
    struct cr_str* dir = bd_cert_dir(mem);
    struct cr_str* certificate_path = bd_cert_file(mem, dir, "certificate.pem");
    struct cr_str* private_key_path = bd_cert_file(mem, dir, "private_key.pem");

    struct bd_cert* cert = cr_malloc(mem, sizeof(*cert), 0, cr_loc);
    memset(cert, 0, sizeof(*cert));
    cert->mem = mem;
    cert->host = bd_cert_host(mem);
    cert->certificate = cr_str_create_with_contents_of_file(mem, certificate_path);
    cert->private_key = cr_str_create_with_contents_of_file(mem, private_key_path);
    if (cert->certificate == NULL || cert->private_key == NULL) {
        bd_fail("there is no certificate in %s, create one with uniform --certificate", cr_str_get_data(dir));
    }

    struct cr_str* output = cr_str_create_empty(mem);
    int code = bd_exec(bd_args_create(mem, "openssl", "x509", "-in", cr_str_get_data(certificate_path), "-noout", "-checkend", "0", "-text", NULL), output);
    if (code != 0) {
        bd_fail("the certificate in %s %s, create a new one with uniform --certificate", cr_str_get_data(dir), strstr(cr_str_get_data(output), "will expire") != NULL ? "has expired" : "cannot be read");
    }
    if (cert->host != NULL) {
        struct cr_str* entry = cr_str_create_format(mem, "DNS:%s", cr_str_get_data(cert->host));
        if (strstr(cr_str_get_data(output), cr_str_get_data(entry)) == NULL) {
            bd_fail("the certificate in %s does not name this Mac, %s, create a new one with uniform --certificate", cr_str_get_data(dir), cr_str_get_data(cert->host));
        }
        cr_str_destroy(entry);
    }

    cr_str_destroy(output);
    cr_str_destroy(private_key_path);
    cr_str_destroy(certificate_path);
    cr_str_destroy(dir);
    return cert;
}

void bd_cert_destroy(struct bd_cert* cert) {
    cr_str_destroy(cert->private_key);
    cr_str_destroy(cert->certificate);
    if (cert->host != NULL) {
        cr_str_destroy(cert->host);
    }
    cr_free(cert->mem, cert, cr_loc);
}
