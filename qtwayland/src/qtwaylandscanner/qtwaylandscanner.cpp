/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QCoreApplication>
#include <QFile>
#include <QXmlStreamReader>
#include <QtCore/QList>

class Scanner
{
public:
    explicit Scanner() {}
    ~Scanner() { delete m_xml; }

    bool parseArguments(int argc, char **argv);
    void printUsage();
    bool process();
    void printErrors();

private:
    struct WaylandEnumEntry {
        QByteArray name;
        QByteArray value;
        QByteArray summary;
    };

    struct WaylandEnum {
        QByteArray name;

        QList<WaylandEnumEntry> entries;
    };

    struct WaylandArgument {
        QByteArray name;
        QByteArray type;
        QByteArray interface;
        QByteArray summary;
        bool allowNull;
    };

    struct WaylandEvent {
        bool request;
        QByteArray name;
        QByteArray type;
        QList<WaylandArgument> arguments;
    };

    struct WaylandInterface {
        QByteArray name;
        int version;

        QList<WaylandEnum> enums;
        QList<WaylandEvent> events;
        QList<WaylandEvent> requests;
    };

    bool isServerSide();
    bool parseOption(const char *str);

    QByteArray byteArrayValue(const QXmlStreamReader &xml, const char *name);
    int intValue(const QXmlStreamReader &xml, const char *name, int defaultValue = 0);
    bool boolValue(const QXmlStreamReader &xml, const char *name);
    WaylandEvent readEvent(QXmlStreamReader &xml, bool request);
    Scanner::WaylandEnum readEnum(QXmlStreamReader &xml);
    Scanner::WaylandInterface readInterface(QXmlStreamReader &xml);
    QByteArray waylandToCType(const QByteArray &waylandType, const QByteArray &interface);
    QByteArray waylandToQtType(const QByteArray &waylandType, const QByteArray &interface, bool cStyleArray);
    const Scanner::WaylandArgument *newIdArgument(const QList<WaylandArgument> &arguments);

    void printEvent(const WaylandEvent &e, bool omitNames = false, bool withResource = false);
    void printEventHandlerSignature(const WaylandEvent &e, const char *interfaceName, bool deepIndent = true);
    void printEnums(const QList<WaylandEnum> &enums);

    QByteArray stripInterfaceName(const QByteArray &name);
    bool ignoreInterface(const QByteArray &name);

    enum Option {
        ClientHeader,
        ServerHeader,
        ClientCode,
        ServerCode
    } m_option;

    QByteArray m_protocolName;
    QByteArray m_protocolFilePath;
    QByteArray m_scannerName;
    QByteArray m_headerPath;
    QByteArray m_prefix;
    QXmlStreamReader *m_xml = nullptr;
};

bool Scanner::parseArguments(int argc, char **argv)
{
    m_scannerName = argv[0];

    if (argc <= 2 || !parseOption(argv[1]))
        return false;

    m_protocolFilePath = QByteArray(argv[2]);

    if (argc >= 4)
        m_headerPath = QByteArray(argv[3]);
    if (argc == 5)
        m_prefix = QByteArray(argv[4]);

    return true;
}

void Scanner::printUsage()
{
    fprintf(stderr, "Usage: %s [client-header|server-header|client-code|server-code] specfile [header-path] [prefix]\n", m_scannerName.constData());
}

bool Scanner::isServerSide()
{
    return m_option == ServerHeader || m_option == ServerCode;
}

bool Scanner::parseOption(const char *str)
{
    if (str == QLatin1String("client-header"))
        m_option = ClientHeader;
    else if (str == QLatin1String("server-header"))
        m_option = ServerHeader;
    else if (str == QLatin1String("client-code"))
        m_option = ClientCode;
    else if (str == QLatin1String("server-code"))
        m_option = ServerCode;
    else
        return false;

    return true;
}

QByteArray Scanner::byteArrayValue(const QXmlStreamReader &xml, const char *name)
{
    if (xml.attributes().hasAttribute(name))
        return xml.attributes().value(name).toUtf8();
    return QByteArray();
}

int Scanner::intValue(const QXmlStreamReader &xml, const char *name, int defaultValue)
{
    bool ok;
    int result = byteArrayValue(xml, name).toInt(&ok);
    return ok ? result : defaultValue;
}

bool Scanner::boolValue(const QXmlStreamReader &xml, const char *name)
{
    return byteArrayValue(xml, name) == "true";
}

Scanner::WaylandEvent Scanner::readEvent(QXmlStreamReader &xml, bool request)
{
    WaylandEvent event;
    event.request = request;
    event.name = byteArrayValue(xml, "name");
    event.type = byteArrayValue(xml, "type");
    while (xml.readNextStartElement()) {
        if (xml.name() == "arg") {
            WaylandArgument argument;
            argument.name = byteArrayValue(xml, "name");
            argument.type = byteArrayValue(xml, "type");
            argument.interface = byteArrayValue(xml, "interface");
            argument.summary = byteArrayValue(xml, "summary");
            argument.allowNull = boolValue(xml, "allowNull");
            event.arguments << argument;
        }

        xml.skipCurrentElement();
    }
    return event;
}

Scanner::WaylandEnum Scanner::readEnum(QXmlStreamReader &xml)
{
    WaylandEnum result;
    result.name = byteArrayValue(xml, "name");

    while (xml.readNextStartElement()) {
        if (xml.name() == "entry") {
            WaylandEnumEntry entry;
            entry.name = byteArrayValue(xml, "name");
            entry.value = byteArrayValue(xml, "value");
            entry.summary = byteArrayValue(xml, "summary");
            result.entries << entry;
        }

        xml.skipCurrentElement();
    }

    return result;
}

Scanner::WaylandInterface Scanner::readInterface(QXmlStreamReader &xml)
{
    WaylandInterface interface;
    interface.name = byteArrayValue(xml, "name");
    interface.version = intValue(xml, "version", 1);

    while (xml.readNextStartElement()) {
        if (xml.name() == "event")
            interface.events << readEvent(xml, false);
        else if (xml.name() == "request")
            interface.requests << readEvent(xml, true);
        else if (xml.name() == "enum")
            interface.enums << readEnum(xml);
        else
            xml.skipCurrentElement();
    }

    return interface;
}

QByteArray Scanner::waylandToCType(const QByteArray &waylandType, const QByteArray &interface)
{
    if (waylandType == "string")
        return "const char *";
    else if (waylandType == "int")
        return "int32_t";
    else if (waylandType == "uint")
        return "uint32_t";
    else if (waylandType == "fixed")
        return "wl_fixed_t";
    else if (waylandType == "fd")
        return "int32_t";
    else if (waylandType == "array")
        return "wl_array *";
    else if (waylandType == "object" || waylandType == "new_id") {
        if (isServerSide())
            return "struct ::wl_resource *";
        if (interface.isEmpty())
            return "struct ::wl_object *";
        return "struct ::" + interface + " *";
    }
    return waylandType;
}

QByteArray Scanner::waylandToQtType(const QByteArray &waylandType, const QByteArray &interface, bool cStyleArray)
{
    if (waylandType == "string")
        return "const QString &";
    else if (waylandType == "array")
        return cStyleArray ? "wl_array *" : "const QByteArray &";
    else
        return waylandToCType(waylandType, interface);
}

const Scanner::WaylandArgument *Scanner::newIdArgument(const QList<WaylandArgument> &arguments)
{
    for (int i = 0; i < arguments.size(); ++i) {
        if (arguments.at(i).type == "new_id")
            return &arguments.at(i);
    }
    return nullptr;
}

void Scanner::printEvent(const WaylandEvent &e, bool omitNames, bool withResource)
{
    printf("%s(", e.name.constData());
    bool needsComma = false;
    if (isServerSide()) {
        if (e.request) {
            printf("Resource *%s", omitNames ? "" : "resource");
            needsComma = true;
        } else if (withResource) {
            printf("struct ::wl_resource *%s", omitNames ? "" : "resource");
            needsComma = true;
        }
    }
    for (int i = 0; i < e.arguments.size(); ++i) {
        const WaylandArgument &a = e.arguments.at(i);
        bool isNewId = a.type == "new_id";
        if (isNewId && !isServerSide() && (a.interface.isEmpty() != e.request))
            continue;
        if (needsComma)
            printf(", ");
        needsComma = true;
        if (isNewId) {
            if (isServerSide()) {
                if (e.request) {
                    printf("uint32_t");
                    if (!omitNames)
                        printf(" %s", a.name.constData());
                    continue;
                }
            } else {
                if (e.request) {
                    printf("const struct ::wl_interface *%s, uint32_t%s", omitNames ? "" : "interface", omitNames ? "" : " version");
                    continue;
                }
            }
        }

        QByteArray qtType = waylandToQtType(a.type, a.interface, e.request == isServerSide());
        printf("%s%s%s", qtType.constData(), qtType.endsWith("&") || qtType.endsWith("*") ? "" : " ", omitNames ? "" : a.name.constData());
    }
    printf(")");
}

void Scanner::printEventHandlerSignature(const WaylandEvent &e, const char *interfaceName, bool deepIndent)
{
    const char *indent = deepIndent ? "    " : "";
    printf("handle_%s(\n", e.name.constData());
    if (isServerSide()) {
        printf("        %s::wl_client *client,\n", indent);
        printf("        %sstruct wl_resource *resource", indent);
    } else {
        printf("        %svoid *data,\n", indent);
        printf("        %sstruct ::%s *object", indent, interfaceName);
    }
    for (int i = 0; i < e.arguments.size(); ++i) {
        printf(",\n");
        const WaylandArgument &a = e.arguments.at(i);
        bool isNewId = a.type == "new_id";
        if (isServerSide() && isNewId) {
            printf("        %suint32_t %s", indent, a.name.constData());
        } else {
            QByteArray cType = waylandToCType(a.type, a.interface);
            printf("        %s%s%s%s", indent, cType.constData(), cType.endsWith("*") ? "" : " ", a.name.constData());
        }
    }
    printf(")");
}

void Scanner::printEnums(const QList<WaylandEnum> &enums)
{
    for (int i = 0; i < enums.size(); ++i) {
        printf("\n");
        const WaylandEnum &e = enums.at(i);
        printf("        enum %s {\n", e.name.constData());
        for (int i = 0; i < e.entries.size(); ++i) {
            const WaylandEnumEntry &entry = e.entries.at(i);
            printf("            %s_%s = %s", e.name.constData(), entry.name.constData(), entry.value.constData());
            if (i < e.entries.size() - 1)
                printf(",");
            if (!entry.summary.isNull())
                printf(" // %s", entry.summary.constData());
            printf("\n");
        }
        printf("        };\n");
    }
}

QByteArray Scanner::stripInterfaceName(const QByteArray &name)
{
    if (!m_prefix.isEmpty() && name.startsWith(m_prefix))
        return name.mid(m_prefix.size());
    if (name.startsWith("qt_") || name.startsWith("wl_"))
        return name.mid(3);

    return name;
}

bool Scanner::ignoreInterface(const QByteArray &name)
{
    return name == "wl_display"
           || (isServerSide() && name == "wl_registry");
}

bool Scanner::process()
{
    QFile file(m_protocolFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fprintf(stderr, "Unable to open file %s\n", m_protocolFilePath.constData());
        return false;
    }

    m_xml = new QXmlStreamReader(&file);
    if (!m_xml->readNextStartElement())
        return false;

    if (m_xml->name() != "protocol") {
        m_xml->raiseError(QStringLiteral("The file is not a wayland protocol file."));
        return false;
    }

    m_protocolName = byteArrayValue(*m_xml, "name");

    if (m_protocolName.isEmpty()) {
        m_xml->raiseError(QStringLiteral("Missing protocol name."));
        return false;
    }

    //We should convert - to _ so that the preprocessor wont generate code which will lead to unexpected behavior
    //However, the wayland-scanner doesn't do so we will do the same for now
    //QByteArray preProcessorProtocolName = QByteArray(m_protocolName).replace('-', '_').toUpper();
    QByteArray preProcessorProtocolName = QByteArray(m_protocolName).toUpper();

    QList<WaylandInterface> interfaces;

    while (m_xml->readNextStartElement()) {
        if (m_xml->name() == "interface")
            interfaces << readInterface(*m_xml);
        else
            m_xml->skipCurrentElement();
    }

    if (m_xml->hasError())
        return false;

    if (m_option == ServerHeader) {
        QByteArray inclusionGuard = QByteArray("QT_WAYLAND_SERVER_") + preProcessorProtocolName.constData();
        printf("#ifndef %s\n", inclusionGuard.constData());
        printf("#define %s\n", inclusionGuard.constData());
        printf("\n");
        printf("#include \"wayland-server-core.h\"\n");
        if (m_headerPath.isEmpty())
            printf("#include \"wayland-%s-server-protocol.h\"\n", QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/wayland-%s-server-protocol.h>\n", m_headerPath.constData(), QByteArray(m_protocolName).replace('_', '-').constData());
        printf("#include <QByteArray>\n");
        printf("#include <QMultiMap>\n");
        printf("#include <QString>\n");

        printf("\n");
        printf("#ifndef WAYLAND_VERSION_CHECK\n");
        printf("#define WAYLAND_VERSION_CHECK(major, minor, micro) \\\n");
        printf("    ((WAYLAND_VERSION_MAJOR > (major)) || \\\n");
        printf("    (WAYLAND_VERSION_MAJOR == (major) && WAYLAND_VERSION_MINOR > (minor)) || \\\n");
        printf("    (WAYLAND_VERSION_MAJOR == (major) && WAYLAND_VERSION_MINOR == (minor) && WAYLAND_VERSION_MICRO >= (micro)))\n");
        printf("#endif\n");

        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");
        printf("QT_WARNING_DISABLE_CLANG(\"-Wmissing-field-initializers\")\n");
        QByteArray serverExport;
        if (m_headerPath.size()) {
            serverExport = QByteArray("Q_WAYLAND_SERVER_") + preProcessorProtocolName + "_EXPORT";
            printf("\n");
            printf("#if !defined(%s)\n", serverExport.constData());
            printf("#  if defined(QT_SHARED)\n");
            printf("#    define %s Q_DECL_EXPORT\n", serverExport.constData());
            printf("#  else\n");
            printf("#    define %s\n", serverExport.constData());
            printf("#  endif\n");
            printf("#endif\n");
        }
        printf("\n");
        printf("namespace QtWaylandServer {\n");

        for (int j = 0; j < interfaces.size(); ++j) {
            const WaylandInterface &interface = interfaces.at(j);

            if (ignoreInterface(interface.name))
                continue;

            const char *interfaceName = interface.name.constData();

            QByteArray stripped = stripInterfaceName(interface.name);
            const char *interfaceNameStripped = stripped.constData();

            printf("    class %s %s\n    {\n", serverExport.constData(), interfaceName);
            printf("    public:\n");
            printf("        %s(struct ::wl_client *client, int id, int version);\n", interfaceName);
            printf("        %s(struct ::wl_display *display, int version);\n", interfaceName);
            printf("        %s(struct ::wl_resource *resource);\n", interfaceName);
            printf("        %s();\n", interfaceName);
            printf("\n");
            printf("        virtual ~%s();\n", interfaceName);
            printf("\n");
            printf("        class Resource\n");
            printf("        {\n");
            printf("        public:\n");
            printf("            Resource() : %s_object(nullptr), handle(nullptr) {}\n", interfaceNameStripped);
            printf("            virtual ~Resource() {}\n");
            printf("\n");
            printf("            %s *%s_object;\n", interfaceName, interfaceNameStripped);
            printf("            %s *object() { return %s_object; } \n", interfaceName, interfaceNameStripped);
            printf("            struct ::wl_resource *handle;\n");
            printf("\n");
            printf("            struct ::wl_client *client() const { return wl_resource_get_client(handle); }\n");
            printf("            int version() const { return wl_resource_get_version(handle); }\n");
            printf("\n");
            printf("            static Resource *fromResource(struct ::wl_resource *resource);\n");
            printf("        };\n");
            printf("\n");
            printf("        void init(struct ::wl_client *client, int id, int version);\n");
            printf("        void init(struct ::wl_display *display, int version);\n");
            printf("        void init(struct ::wl_resource *resource);\n");
            printf("\n");
            printf("        Resource *add(struct ::wl_client *client, int version);\n");
            printf("        Resource *add(struct ::wl_client *client, int id, int version);\n");
            printf("        Resource *add(struct wl_list *resource_list, struct ::wl_client *client, int id, int version);\n");
            printf("\n");
            printf("        Resource *resource() { return m_resource; }\n");
            printf("        const Resource *resource() const { return m_resource; }\n");
            printf("\n");
            printf("        QMultiMap<struct ::wl_client*, Resource*> resourceMap() { return m_resource_map; }\n");
            printf("        const QMultiMap<struct ::wl_client*, Resource*> resourceMap() const { return m_resource_map; }\n");
            printf("\n");
            printf("        bool isGlobal() const { return m_global != nullptr; }\n");
            printf("        bool isResource() const { return m_resource != nullptr; }\n");
            printf("\n");
            printf("        static const struct ::wl_interface *interface();\n");
            printf("        static QByteArray interfaceName() { return interface()->name; }\n");
            printf("        static int interfaceVersion() { return interface()->version; }\n");
            printf("\n");

            printEnums(interface.enums);

            bool hasEvents = !interface.events.isEmpty();

            if (hasEvents) {
                printf("\n");
                foreach (const WaylandEvent &e, interface.events) {
                    printf("        void send_");
                    printEvent(e);
                    printf(";\n");
                    printf("        void send_");
                    printEvent(e, false, true);
                    printf(";\n");
                }
            }

            printf("\n");
            printf("    protected:\n");
            printf("        virtual Resource *%s_allocate();\n", interfaceNameStripped);
            printf("\n");
            printf("        virtual void %s_bind_resource(Resource *resource);\n", interfaceNameStripped);
            printf("        virtual void %s_destroy_resource(Resource *resource);\n", interfaceNameStripped);

            bool hasRequests = !interface.requests.isEmpty();

            if (hasRequests) {
                printf("\n");
                foreach (const WaylandEvent &e, interface.requests) {
                    printf("        virtual void %s_", interfaceNameStripped);
                    printEvent(e);
                    printf(";\n");
                }
            }

            printf("\n");
            printf("    private:\n");
            printf("        static void bind_func(struct ::wl_client *client, void *data, uint32_t version, uint32_t id);\n");
            printf("        static void destroy_func(struct ::wl_resource *client_resource);\n");
            printf("        static void display_destroy_func(struct ::wl_listener *listener, void *data);\n");
            printf("\n");
            printf("        Resource *bind(struct ::wl_client *client, uint32_t id, int version);\n");
            printf("        Resource *bind(struct ::wl_resource *handle);\n");

            if (hasRequests) {
                printf("\n");
                printf("        static const struct ::%s_interface m_%s_interface;\n", interfaceName, interfaceName);

                printf("\n");
                for (int i = 0; i < interface.requests.size(); ++i) {
                    const WaylandEvent &e = interface.requests.at(i);
                    printf("        static void ");

                    printEventHandlerSignature(e, interfaceName);
                    printf(";\n");
                }
            }

            printf("\n");
            printf("        QMultiMap<struct ::wl_client*, Resource*> m_resource_map;\n");
            printf("        Resource *m_resource;\n");
            printf("        struct ::wl_global *m_global;\n");
            printf("        uint32_t m_globalVersion;\n");
            printf("        struct DisplayDestroyedListener : ::wl_listener {\n");
            printf("            %s *parent;\n", interfaceName);
            printf("        };\n");
            printf("        DisplayDestroyedListener m_displayDestroyedListener;\n");
            printf("    };\n");

            if (j < interfaces.size() - 1)
                printf("\n");
        }

        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
        printf("\n");
        printf("#endif\n");
    }

    if (m_option == ServerCode) {
        if (m_headerPath.isEmpty())
            printf("#include \"qwayland-server-%s.h\"\n", QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/qwayland-server-%s.h>\n", m_headerPath.constData(), QByteArray(m_protocolName).replace('_', '-').constData());
        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");
        printf("\n");
        printf("namespace QtWaylandServer {\n");

        bool needsNewLine = false;
        for (int j = 0; j < interfaces.size(); ++j) {
            const WaylandInterface &interface = interfaces.at(j);

            if (ignoreInterface(interface.name))
                continue;

            if (needsNewLine)
                printf("\n");

            needsNewLine = true;

            const char *interfaceName = interface.name.constData();

            QByteArray stripped = stripInterfaceName(interface.name);
            const char *interfaceNameStripped = stripped.constData();

            printf("    %s::%s(struct ::wl_client *client, int id, int version)\n", interfaceName, interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("    {\n");
            printf("        init(client, id, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s(struct ::wl_display *display, int version)\n", interfaceName, interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("    {\n");
            printf("        init(display, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s(struct ::wl_resource *resource)\n", interfaceName, interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("    {\n");
            printf("        init(resource);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s()\n", interfaceName, interfaceName);
            printf("        : m_resource_map()\n");
            printf("        , m_resource(nullptr)\n");
            printf("        , m_global(nullptr)\n");
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::~%s()\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        for (auto resource : qAsConst(m_resource_map))\n");
            printf("            wl_resource_set_implementation(resource->handle, nullptr, nullptr, nullptr);\n");
            printf("\n");
            printf("        if (m_global) {\n");
            printf("            wl_global_destroy(m_global);\n");
            printf("            wl_list_remove(&m_displayDestroyedListener.link);\n");
            printf("        }\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_client *client, int id, int version)\n", interfaceName);
            printf("    {\n");
            printf("        m_resource = bind(client, id, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_resource *resource)\n", interfaceName);
            printf("    {\n");
            printf("        m_resource = bind(resource);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::Resource *%s::add(struct ::wl_client *client, int version)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        Resource *resource = bind(client, 0, version);\n");
            printf("        m_resource_map.insert(client, resource);\n");
            printf("        return resource;\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::Resource *%s::add(struct ::wl_client *client, int id, int version)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        Resource *resource = bind(client, id, version);\n");
            printf("        m_resource_map.insert(client, resource);\n");
            printf("        return resource;\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_display *display, int version)\n", interfaceName);
            printf("    {\n");
            printf("        m_global = wl_global_create(display, &::%s_interface, version, this, bind_func);\n", interfaceName);
            printf("        m_globalVersion = version;\n");
            printf("        m_displayDestroyedListener.notify = %s::display_destroy_func;\n", interfaceName);
            printf("        m_displayDestroyedListener.parent = this;\n");
            printf("        wl_display_add_destroy_listener(display, &m_displayDestroyedListener);\n");
            printf("    }\n");
            printf("\n");

            printf("    const struct wl_interface *%s::interface()\n", interfaceName);
            printf("    {\n");
            printf("        return &::%s_interface;\n", interfaceName);
            printf("    }\n");
            printf("\n");

            printf("    %s::Resource *%s::%s_allocate()\n", interfaceName, interfaceName, interfaceNameStripped);
            printf("    {\n");
            printf("        return new Resource;\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::%s_bind_resource(Resource *)\n", interfaceName, interfaceNameStripped);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::%s_destroy_resource(Resource *)\n", interfaceName, interfaceNameStripped);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::bind_func(struct ::wl_client *client, void *data, uint32_t version, uint32_t id)\n", interfaceName);
            printf("    {\n");
            printf("        %s *that = static_cast<%s *>(data);\n", interfaceName, interfaceName);
            printf("        that->add(client, id, qMin(that->m_globalVersion, version));\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::display_destroy_func(struct ::wl_listener *listener, void *data)\n", interfaceName);
            printf("    {\n");
            printf("        Q_UNUSED(data);\n");
            printf("        %s *that = static_cast<%s::DisplayDestroyedListener *>(listener)->parent;\n", interfaceName, interfaceName);
            printf("        that->m_global = nullptr;\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::destroy_func(struct ::wl_resource *client_resource)\n", interfaceName);
            printf("    {\n");
            printf("        Resource *resource = Resource::fromResource(client_resource);\n");
            printf("        Q_ASSERT(resource);\n");
            printf("        %s *that = resource->%s_object;\n", interfaceName, interfaceNameStripped);
            printf("        that->m_resource_map.remove(resource->client(), resource);\n");
            printf("        that->%s_destroy_resource(resource);\n", interfaceNameStripped);
            printf("        delete resource;\n");
            printf("    }\n");
            printf("\n");

            bool hasRequests = !interface.requests.isEmpty();

            QByteArray interfaceMember = hasRequests ? "&m_" + interface.name + "_interface" : QByteArray("nullptr");

            //We should consider changing bind so that it doesn't special case id == 0
            //and use function overloading instead. Jan do you have a lot of code dependent on this behavior?
            printf("    %s::Resource *%s::bind(struct ::wl_client *client, uint32_t id, int version)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        Q_ASSERT_X(!wl_client_get_object(client, id), \"QWaylandObject bind\", QStringLiteral(\"binding to object %%1 more than once\").arg(id).toLocal8Bit().constData());\n");
            printf("        struct ::wl_resource *handle = wl_resource_create(client, &::%s_interface, version, id);\n", interfaceName);
            printf("        return bind(handle);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::Resource *%s::bind(struct ::wl_resource *handle)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        Resource *resource = %s_allocate();\n", interfaceNameStripped);
            printf("        resource->%s_object = this;\n", interfaceNameStripped);
            printf("\n");
            printf("        wl_resource_set_implementation(handle, %s, resource, destroy_func);", interfaceMember.constData());
            printf("\n");
            printf("        resource->handle = handle;\n");
            printf("        %s_bind_resource(resource);\n", interfaceNameStripped);
            printf("        return resource;\n");
            printf("    }\n");

            printf("    %s::Resource *%s::Resource::fromResource(struct ::wl_resource *resource)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        if (Q_UNLIKELY(!resource))\n");
            printf("            return nullptr;\n");
            printf("        if (wl_resource_instance_of(resource, &::%s_interface, %s))\n",  interfaceName, interfaceMember.constData());
            printf("            return static_cast<Resource *>(wl_resource_get_user_data(resource));\n");
            printf("        return nullptr;\n");
            printf("    }\n");

            if (hasRequests) {
                printf("\n");
                printf("    const struct ::%s_interface %s::m_%s_interface = {", interfaceName, interfaceName, interfaceName);
                for (int i = 0; i < interface.requests.size(); ++i) {
                    if (i > 0)
                        printf(",");
                    printf("\n");
                    const WaylandEvent &e = interface.requests.at(i);
                    printf("        %s::handle_%s", interfaceName, e.name.constData());
                }
                printf("\n");
                printf("    };\n");

                foreach (const WaylandEvent &e, interface.requests) {
                    printf("\n");
                    printf("    void %s::%s_", interfaceName, interfaceNameStripped);
                    printEvent(e, true);
                    printf("\n");
                    printf("    {\n");
                    printf("    }\n");
                }
                printf("\n");

                for (int i = 0; i < interface.requests.size(); ++i) {
                    printf("\n");
                    printf("    void %s::", interfaceName);

                    const WaylandEvent &e = interface.requests.at(i);
                    printEventHandlerSignature(e, interfaceName, false);

                    printf("\n");
                    printf("    {\n");
                    printf("        Q_UNUSED(client);\n");
                    printf("        Resource *r = Resource::fromResource(resource);\n");
                    printf("        static_cast<%s *>(r->%s_object)->%s_%s(\n", interfaceName, interfaceNameStripped, interfaceNameStripped, e.name.constData());
                    printf("            r");
                    for (int i = 0; i < e.arguments.size(); ++i) {
                        printf(",\n");
                        const WaylandArgument &a = e.arguments.at(i);
                        QByteArray cType = waylandToCType(a.type, a.interface);
                        QByteArray qtType = waylandToQtType(a.type, a.interface, e.request);
                        const char *argumentName = a.name.constData();
                        if (cType == qtType)
                            printf("            %s", argumentName);
                        else if (a.type == "string")
                            printf("            QString::fromUtf8(%s)", argumentName);
                    }
                    printf(");\n");
                    printf("    }\n");
                }
            }

            for (int i = 0; i < interface.events.size(); ++i) {
                printf("\n");
                const WaylandEvent &e = interface.events.at(i);
                printf("    void %s::send_", interfaceName);
                printEvent(e);
                printf("\n");
                printf("    {\n");
                printf("        send_%s(\n", e.name.constData());
                printf("            m_resource->handle");
                for (int i = 0; i < e.arguments.size(); ++i) {
                    const WaylandArgument &a = e.arguments.at(i);
                    printf(",\n");
                    printf("            %s", a.name.constData());
                }
                printf(");\n");
                printf("    }\n");
                printf("\n");

                printf("    void %s::send_", interfaceName);
                printEvent(e, false, true);
                printf("\n");
                printf("    {\n");

                for (int i = 0; i < e.arguments.size(); ++i) {
                    const WaylandArgument &a = e.arguments.at(i);
                    if (a.type != "array")
                        continue;
                    QByteArray array = a.name + "_data";
                    const char *arrayName = array.constData();
                    const char *variableName = a.name.constData();
                    printf("        struct wl_array %s;\n", arrayName);
                    printf("        %s.size = %s.size();\n", arrayName, variableName);
                    printf("        %s.data = static_cast<void *>(const_cast<char *>(%s.constData()));\n", arrayName, variableName);
                    printf("        %s.alloc = 0;\n", arrayName);
                    printf("\n");
                }

                printf("        %s_send_%s(\n", interfaceName, e.name.constData());
                printf("            resource");

                for (int i = 0; i < e.arguments.size(); ++i) {
                    const WaylandArgument &a = e.arguments.at(i);
                    printf(",\n");
                    QByteArray cType = waylandToCType(a.type, a.interface);
                    QByteArray qtType = waylandToQtType(a.type, a.interface, e.request);
                    if (a.type == "string")
                        printf("            %s.toUtf8().constData()", a.name.constData());
                    else if (a.type == "array")
                        printf("            &%s_data", a.name.constData());
                    else if (cType == qtType)
                        printf("            %s", a.name.constData());
                }

                printf(");\n");
                printf("    }\n");
                printf("\n");
            }
        }
        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
    }

    if (m_option == ClientHeader) {
        QByteArray inclusionGuard = QByteArray("QT_WAYLAND_") + preProcessorProtocolName.constData();
        printf("#ifndef %s\n", inclusionGuard.constData());
        printf("#define %s\n", inclusionGuard.constData());
        printf("\n");
        if (m_headerPath.isEmpty())
            printf("#include \"wayland-%s-client-protocol.h\"\n", QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/wayland-%s-client-protocol.h>\n", m_headerPath.constData(), QByteArray(m_protocolName).replace('_', '-').constData());
        printf("#include <QByteArray>\n");
        printf("#include <QString>\n");
        printf("\n");
        printf("struct wl_registry;\n");
        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");

        QByteArray clientExport;

        if (m_headerPath.size()) {
            clientExport = QByteArray("Q_WAYLAND_CLIENT_") + preProcessorProtocolName + "_EXPORT";
            printf("\n");
            printf("#if !defined(%s)\n", clientExport.constData());
            printf("#  if defined(QT_SHARED)\n");
            printf("#    define %s Q_DECL_EXPORT\n", clientExport.constData());
            printf("#  else\n");
            printf("#    define %s\n", clientExport.constData());
            printf("#  endif\n");
            printf("#endif\n");
        }
        printf("\n");
        printf("namespace QtWayland {\n");
        for (int j = 0; j < interfaces.size(); ++j) {
            const WaylandInterface &interface = interfaces.at(j);

            if (ignoreInterface(interface.name))
                continue;

            const char *interfaceName = interface.name.constData();

            QByteArray stripped = stripInterfaceName(interface.name);
            const char *interfaceNameStripped = stripped.constData();

            printf("    class %s %s\n    {\n", clientExport.constData(), interfaceName);
            printf("    public:\n");
            printf("        %s(struct ::wl_registry *registry, int id, int version);\n", interfaceName);
            printf("        %s(struct ::%s *object);\n", interfaceName, interfaceName);
            printf("        %s();\n", interfaceName);
            printf("\n");
            printf("        virtual ~%s();\n", interfaceName);
            printf("\n");
            printf("        void init(struct ::wl_registry *registry, int id, int version);\n");
            printf("        void init(struct ::%s *object);\n", interfaceName);
            printf("\n");
            printf("        struct ::%s *object() { return m_%s; }\n", interfaceName, interfaceName);
            printf("        const struct ::%s *object() const { return m_%s; }\n", interfaceName, interfaceName);
            printf("        static %s *fromObject(struct ::%s *object);\n", interfaceName, interfaceName);
            printf("\n");
            printf("        bool isInitialized() const;\n");
            printf("\n");
            printf("        static const struct ::wl_interface *interface();\n");

            printEnums(interface.enums);

            if (!interface.requests.isEmpty()) {
                printf("\n");
                foreach (const WaylandEvent &e, interface.requests) {
                    const WaylandArgument *new_id = newIdArgument(e.arguments);
                    QByteArray new_id_str = "void ";
                    if (new_id) {
                        if (new_id->interface.isEmpty())
                            new_id_str = "void *";
                        else
                            new_id_str = "struct ::" + new_id->interface + " *";
                    }
                    printf("        %s", new_id_str.constData());
                    printEvent(e);
                    printf(";\n");
                }
            }

            bool hasEvents = !interface.events.isEmpty();

            if (hasEvents) {
                printf("\n");
                printf("    protected:\n");
                foreach (const WaylandEvent &e, interface.events) {
                    printf("        virtual void %s_", interfaceNameStripped);
                    printEvent(e);
                    printf(";\n");
                }
            }

            printf("\n");
            printf("    private:\n");
            if (hasEvents) {
                printf("        void init_listener();\n");
                printf("        static const struct %s_listener m_%s_listener;\n", interfaceName, interfaceName);
                for (int i = 0; i < interface.events.size(); ++i) {
                    const WaylandEvent &e = interface.events.at(i);
                    printf("        static void ");

                    printEventHandlerSignature(e, interfaceName);
                    printf(";\n");
                }
            }
            printf("        struct ::%s *m_%s;\n", interfaceName, interfaceName);
            printf("    };\n");

            if (j < interfaces.size() - 1)
                printf("\n");
        }
        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
        printf("\n");
        printf("#endif\n");
    }

    if (m_option == ClientCode) {
        if (m_headerPath.isEmpty())
            printf("#include \"qwayland-%s.h\"\n", QByteArray(m_protocolName).replace('_', '-').constData());
        else
            printf("#include <%s/qwayland-%s.h>\n", m_headerPath.constData(), QByteArray(m_protocolName).replace('_', '-').constData());
        printf("\n");
        printf("QT_BEGIN_NAMESPACE\n");
        printf("QT_WARNING_PUSH\n");
        printf("QT_WARNING_DISABLE_GCC(\"-Wmissing-field-initializers\")\n");
        printf("\n");
        printf("namespace QtWayland {\n");
        printf("\n");

        // wl_registry_bind is part of the protocol, so we can't use that... instead we use core
        // libwayland API to do the same thing a wayland-scanner generated wl_registry_bind would.
        printf("static inline void *wlRegistryBind(struct ::wl_registry *registry, uint32_t name, const struct ::wl_interface *interface, uint32_t version)\n");
        printf("{\n");
        printf("    const uint32_t bindOpCode = 0;\n");
        printf("#if (WAYLAND_VERSION_MAJOR == 1 && WAYLAND_VERSION_MINOR > 10) || WAYLAND_VERSION_MAJOR > 1\n");
        printf("    return (void *) wl_proxy_marshal_constructor_versioned((struct wl_proxy *) registry,\n");
        printf("        bindOpCode, interface, version, name, interface->name, version, nullptr);\n");
        printf("#else\n");
        printf("    return (void *) wl_proxy_marshal_constructor((struct wl_proxy *) registry,\n");
        printf("        bindOpCode, interface, name, interface->name, version, nullptr);\n");
        printf("#endif\n");
        printf("}\n");
        printf("\n");
        for (int j = 0; j < interfaces.size(); ++j) {
            const WaylandInterface &interface = interfaces.at(j);

            if (ignoreInterface(interface.name))
                continue;

            const char *interfaceName = interface.name.constData();

            QByteArray stripped = stripInterfaceName(interface.name);
            const char *interfaceNameStripped = stripped.constData();

            bool hasEvents = !interface.events.isEmpty();

            printf("    %s::%s(struct ::wl_registry *registry, int id, int version)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        init(registry, id, version);\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s(struct ::%s *obj)\n", interfaceName, interfaceName, interfaceName);
            printf("        : m_%s(obj)\n", interfaceName);
            printf("    {\n");
            if (hasEvents)
                printf("        init_listener();\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::%s()\n", interfaceName, interfaceName);
            printf("        : m_%s(nullptr)\n", interfaceName);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    %s::~%s()\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::wl_registry *registry, int id, int version)\n", interfaceName);
            printf("    {\n");
            printf("        m_%s = static_cast<struct ::%s *>(wlRegistryBind(registry, id, &%s_interface, version));\n", interfaceName, interfaceName, interfaceName);
            if (hasEvents)
                printf("        init_listener();\n");
            printf("    }\n");
            printf("\n");

            printf("    void %s::init(struct ::%s *obj)\n", interfaceName, interfaceName);
            printf("    {\n");
            printf("        m_%s = obj;\n", interfaceName);
            if (hasEvents)
                printf("        init_listener();\n");
            printf("    }\n");
            printf("\n");

            printf("    %s *%s::fromObject(struct ::%s *object)\n", interfaceName, interfaceName, interfaceName);
            printf("    {\n");
            if (hasEvents) {
                printf("        if (wl_proxy_get_listener((struct ::wl_proxy *)object) != (void *)&m_%s_listener)\n", interfaceName);
                printf("            return nullptr;\n");
            }
            printf("        return static_cast<%s *>(%s_get_user_data(object));\n", interfaceName, interfaceName);
            printf("    }\n");
            printf("\n");

            printf("    bool %s::isInitialized() const\n", interfaceName);
            printf("    {\n");
            printf("        return m_%s != nullptr;\n", interfaceName);
            printf("    }\n");
            printf("\n");

            printf("    const struct wl_interface *%s::interface()\n", interfaceName);
            printf("    {\n");
            printf("        return &::%s_interface;\n", interfaceName);
            printf("    }\n");

            for (int i = 0; i < interface.requests.size(); ++i) {
                printf("\n");
                const WaylandEvent &e = interface.requests.at(i);
                const WaylandArgument *new_id = newIdArgument(e.arguments);
                QByteArray new_id_str = "void ";
                if (new_id) {
                    if (new_id->interface.isEmpty())
                        new_id_str = "void *";
                    else
                        new_id_str = "struct ::" + new_id->interface + " *";
                }
                printf("    %s%s::", new_id_str.constData(), interfaceName);
                printEvent(e);
                printf("\n");
                printf("    {\n");
                for (int i = 0; i < e.arguments.size(); ++i) {
                    const WaylandArgument &a = e.arguments.at(i);
                    if (a.type != "array")
                        continue;
                    QByteArray array = a.name + "_data";
                    const char *arrayName = array.constData();
                    const char *variableName = a.name.constData();
                    printf("        struct wl_array %s;\n", arrayName);
                    printf("        %s.size = %s.size();\n", arrayName, variableName);
                    printf("        %s.data = static_cast<void *>(const_cast<char *>(%s.constData()));\n", arrayName, variableName);
                    printf("        %s.alloc = 0;\n", arrayName);
                    printf("\n");
                }
                int actualArgumentCount = new_id ? e.arguments.size() - 1 : e.arguments.size();
                printf("        %s%s_%s(\n", new_id ? "return " : "", interfaceName, e.name.constData());
                printf("            m_%s%s", interfaceName, actualArgumentCount > 0 ? "," : "");
                bool needsComma = false;
                for (int i = 0; i < e.arguments.size(); ++i) {
                    const WaylandArgument &a = e.arguments.at(i);
                    bool isNewId = a.type == "new_id";
                    if (isNewId && !a.interface.isEmpty())
                        continue;
                    if (needsComma)
                        printf(",");
                    needsComma = true;
                    printf("\n");
                    if (isNewId) {
                        printf("            interface,\n");
                        printf("            version");
                    } else {
                        QByteArray cType = waylandToCType(a.type, a.interface);
                        QByteArray qtType = waylandToQtType(a.type, a.interface, e.request);
                        if (a.type == "string")
                            printf("            %s.toUtf8().constData()", a.name.constData());
                        else if (a.type == "array")
                            printf("            &%s_data", a.name.constData());
                        else if (cType == qtType)
                            printf("            %s", a.name.constData());
                    }
                }
                printf(");\n");
                if (e.type == "destructor")
                    printf("        m_%s = nullptr;\n", interfaceName);
                printf("    }\n");
            }

            if (hasEvents) {
                printf("\n");
                for (int i = 0; i < interface.events.size(); ++i) {
                    const WaylandEvent &e = interface.events.at(i);
                    printf("    void %s::%s_", interfaceName, interfaceNameStripped);
                    printEvent(e, true);
                    printf("\n");
                    printf("    {\n");
                    printf("    }\n");
                    printf("\n");
                    printf("    void %s::", interfaceName);
                    printEventHandlerSignature(e, interfaceName, false);
                    printf("\n");
                    printf("    {\n");
                    printf("        Q_UNUSED(object);\n");
                    printf("        static_cast<%s *>(data)->%s_%s(", interfaceName, interfaceNameStripped, e.name.constData());
                    for (int i = 0; i < e.arguments.size(); ++i) {
                        printf("\n");
                        const WaylandArgument &a = e.arguments.at(i);
                        const char *argumentName = a.name.constData();
                        if (a.type == "string")
                            printf("            QString::fromUtf8(%s)", argumentName);
                        else
                            printf("            %s", argumentName);

                        if (i < e.arguments.size() - 1)
                            printf(",");
                    }
                    printf(");\n");

                    printf("    }\n");
                    printf("\n");
                }
                printf("    const struct %s_listener %s::m_%s_listener = {\n", interfaceName, interfaceName, interfaceName);
                for (int i = 0; i < interface.events.size(); ++i) {
                    const WaylandEvent &e = interface.events.at(i);
                    printf("        %s::handle_%s%s\n", interfaceName, e.name.constData(), i < interface.events.size() - 1 ? "," : "");
                }
                printf("    };\n");
                printf("\n");

                printf("    void %s::init_listener()\n", interfaceName);
                printf("    {\n");
                printf("        %s_add_listener(m_%s, &m_%s_listener, this);\n", interfaceName, interfaceName, interfaceName);
                printf("    }\n");
            }

            if (j < interfaces.size() - 1)
                printf("\n");
        }
        printf("}\n");
        printf("\n");
        printf("QT_WARNING_POP\n");
        printf("QT_END_NAMESPACE\n");
    }

    return true;
}

void Scanner::printErrors()
{
    if (m_xml->hasError())
        fprintf(stderr, "XML error: %s\nLine %lld, column %lld\n", m_xml->errorString().toLocal8Bit().constData(), m_xml->lineNumber(), m_xml->columnNumber());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Scanner scanner;

    if (!scanner.parseArguments(argc, argv)) {
        scanner.printUsage();
        return EXIT_FAILURE;
    }

    if (!scanner.process()) {
        scanner.printErrors();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
